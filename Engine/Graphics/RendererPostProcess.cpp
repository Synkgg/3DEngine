#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Core/Logger.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <iostream>

bool Renderer::CreatePostProcessTarget()
{
    if (m_ViewportWidth == 0 || m_ViewportHeight == 0) return false;
    glGenFramebuffers(1, &m_PostFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_PostFramebuffer);
    glGenTextures(1, &m_PostColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_PostColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_ViewportWidth, m_ViewportHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_PostColorTexture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) { DestroyPostProcessTarget(); return false; }

    glGenFramebuffers(2, m_BloomFramebuffer);
    glGenTextures(2, m_BloomTexture);
    for (int i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffer[i]);
        glBindTexture(GL_TEXTURE_2D, m_BloomTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_ViewportWidth, m_ViewportHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomTexture[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            DestroyPostProcessTarget();
            return false;
        }
    }
    glGenFramebuffers(1, &m_HistoryFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_HistoryFramebuffer);
    glGenTextures(1, &m_HistoryTexture);
    glBindTexture(GL_TEXTURE_2D, m_HistoryTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_ViewportWidth, m_ViewportHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_HistoryTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        DestroyPostProcessTarget();
        return false;
    }
    m_HistoryValid = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Renderer::DestroyPostProcessTarget()
{
    if (m_BloomTexture[0] || m_BloomTexture[1]) glDeleteTextures(2, m_BloomTexture);
    if (m_BloomFramebuffer[0] || m_BloomFramebuffer[1]) glDeleteFramebuffers(2, m_BloomFramebuffer);
    m_BloomTexture[0] = m_BloomTexture[1] = 0;
    m_BloomFramebuffer[0] = m_BloomFramebuffer[1] = 0;
    if (m_HistoryTexture) glDeleteTextures(1, &m_HistoryTexture);
    if (m_HistoryFramebuffer) glDeleteFramebuffers(1, &m_HistoryFramebuffer);
    m_HistoryTexture = 0;
    m_HistoryFramebuffer = 0;
    m_HistoryValid = false;
    if (m_PostColorTexture) glDeleteTextures(1, &m_PostColorTexture);
    if (m_PostFramebuffer) glDeleteFramebuffers(1, &m_PostFramebuffer);
    m_PostColorTexture = 0;
    m_PostFramebuffer = 0;
}

unsigned int Renderer::RenderBloom()
{
    if (!m_RenderSettings.bloom || m_RenderSettings.bloomStrength <= 0.0f ||
        !m_BloomFramebuffer[0] || !m_BloomFramebuffer[1])
        return 0;

    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_PostVAO);

    glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffer[0]);
    glViewport(0, 0, static_cast<int>(m_ViewportWidth), static_cast<int>(m_ViewportHeight));
    m_BloomExtractShader.Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Framebuffer.GetColorTexture());
    m_BloomExtractShader.SetInt("u_Scene", 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_BloomExtractShader.Unbind();

    bool horizontal = true;
    unsigned int sourceTexture = m_BloomTexture[0];
    for (int pass = 0; pass < 8; ++pass)
    {
        const int target = horizontal ? 1 : 0;
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffer[target]);
        m_BloomBlurShader.Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        m_BloomBlurShader.SetInt("u_Image", 0);
        m_BloomBlurShader.SetInt("u_Horizontal", horizontal ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        m_BloomBlurShader.Unbind();
        sourceTexture = m_BloomTexture[target];
        horizontal = !horizontal;
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return sourceTexture;
}

void Renderer::RenderPostProcess()
{
    if (!m_PostFramebuffer || !m_PostColorTexture || !m_PostVAO) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_PostFramebuffer);
    glViewport(0, 0, static_cast<int>(m_ViewportWidth), static_cast<int>(m_ViewportHeight));
    glDisable(GL_DEPTH_TEST);
    const unsigned int bloomTexture = RenderBloom();
    glBindFramebuffer(GL_FRAMEBUFFER, m_PostFramebuffer);
    glViewport(0, 0, static_cast<int>(m_ViewportWidth), static_cast<int>(m_ViewportHeight));
    m_PostShader.Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Framebuffer.GetColorTexture());
    m_PostShader.SetInt("u_Scene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexture);
    m_PostShader.SetInt("u_Bloom", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_Framebuffer.GetDepthTexture());
    m_PostShader.SetInt("u_Depth", 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_Framebuffer.GetNormalTexture());
    m_PostShader.SetInt("u_NormalRoughness", 3);
    m_PostShader.SetFloat("u_BloomStrength", bloomTexture ? m_RenderSettings.bloomStrength : 0.0f);
    m_PostShader.SetFloat("u_Exposure", m_RenderSettings.exposure);
    const Vec3 postForward = m_Camera.GetForward();
    const Vec3 postRight = m_Camera.GetRight();
    const Vec3 postUp = Vec3::Cross(postRight, postForward).Normalized();
    const float postTanHalfFov = std::tan(m_Camera.GetFovDegrees() * 0.5f * 0.017453292519943295f);
    const float postAspect = m_ViewportHeight > 0 ? static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight) : 1.0f;
    m_PostShader.SetVec3("u_CameraForward", postForward.x, postForward.y, postForward.z);
    m_PostShader.SetVec3("u_CameraRight", postRight.x, postRight.y, postRight.z);
    m_PostShader.SetVec3("u_CameraUp", postUp.x, postUp.y, postUp.z);
    m_PostShader.SetVec3("u_SunDirection", m_LightDirection.x, m_LightDirection.y, m_LightDirection.z);
    m_PostShader.SetVec3("u_SunColor", m_LightColor.x, m_LightColor.y, m_LightColor.z);
    m_PostShader.SetFloat("u_SunIntensity", m_LightIntensity);
    m_PostShader.SetFloat("u_TanHalfFov", postTanHalfFov);
    m_PostShader.SetFloat("u_Aspect", postAspect);
    m_PostShader.SetFloat("u_AtmosphereStrength", m_RenderSettings.atmosphereStrength);
    m_PostShader.SetFloat("u_ColorSaturation", m_RenderSettings.colorSaturation);
    m_PostShader.SetFloat("u_Contrast", m_RenderSettings.contrast);
    m_PostShader.SetFloat("u_SSRStrength", m_RenderSettings.screenSpaceReflections ? m_RenderSettings.screenSpaceReflectionStrength : 0.0f);
    m_PostShader.SetFloat("u_GIStrength", m_RenderSettings.giStrength);
    glBindVertexArray(m_PostVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_PostShader.Unbind();

    glActiveTexture(GL_TEXTURE0);
    ResolveTAA();
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::ResolveTAA()
{
    if (!m_HistoryFramebuffer || !m_HistoryTexture || !m_PostColorTexture || !m_PostVAO) return;
    const Vec3 forward=m_Camera.GetForward(), right=m_Camera.GetRight(), up=Vec3::Cross(right,forward).Normalized(), position=m_Camera.GetPosition();
    const float tanHalf=std::tan(m_Camera.GetFovDegrees()*0.5f*0.017453292519943295f);
    const float aspect=m_ViewportHeight?static_cast<float>(m_ViewportWidth)/static_cast<float>(m_ViewportHeight):1.0f;
    // Resolve into history using camera reprojection and neighborhood clipping.
    glBindFramebuffer(GL_FRAMEBUFFER,m_HistoryFramebuffer); glViewport(0,0,(int)m_ViewportWidth,(int)m_ViewportHeight); glDisable(GL_DEPTH_TEST);
    m_TAAShader.Bind();
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_PostColorTexture);m_TAAShader.SetInt("u_Current",0);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,m_HistoryTexture);m_TAAShader.SetInt("u_History",1);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,m_Framebuffer.GetDepthTexture());m_TAAShader.SetInt("u_Depth",2);
    m_TAAShader.SetVec3("u_CameraForward",forward.x,forward.y,forward.z);m_TAAShader.SetVec3("u_CameraRight",right.x,right.y,right.z);m_TAAShader.SetVec3("u_CameraUp",up.x,up.y,up.z);m_TAAShader.SetVec3("u_CameraPosition",position.x,position.y,position.z);
    m_TAAShader.SetVec3("u_PreviousForward",m_PreviousCameraForward.x,m_PreviousCameraForward.y,m_PreviousCameraForward.z);m_TAAShader.SetVec3("u_PreviousRight",m_PreviousCameraRight.x,m_PreviousCameraRight.y,m_PreviousCameraRight.z);m_TAAShader.SetVec3("u_PreviousUp",m_PreviousCameraUp.x,m_PreviousCameraUp.y,m_PreviousCameraUp.z);m_TAAShader.SetVec3("u_PreviousPosition",m_PreviousCameraPosition.x,m_PreviousCameraPosition.y,m_PreviousCameraPosition.z);
    m_TAAShader.SetFloat("u_TanHalfFov",tanHalf);m_TAAShader.SetFloat("u_Aspect",aspect);m_TAAShader.SetFloat("u_PreviousTanHalfFov",m_PreviousTanHalfFov);m_TAAShader.SetFloat("u_PreviousAspect",m_PreviousAspect);m_TAAShader.SetInt("u_HistoryValid",m_HistoryValid?1:0);
    glBindVertexArray(m_PostVAO);glDrawArrays(GL_TRIANGLES,0,3);glBindVertexArray(0);m_TAAShader.Unbind();
    // Copy resolved history back to the viewport target without sampling and rendering the same texture.
    glBindFramebuffer(GL_READ_FRAMEBUFFER,m_HistoryFramebuffer);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_PostFramebuffer);glBlitFramebuffer(0,0,m_ViewportWidth,m_ViewportHeight,0,0,m_ViewportWidth,m_ViewportHeight,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    m_PreviousCameraPosition=position;m_PreviousCameraForward=forward;m_PreviousCameraRight=right;m_PreviousCameraUp=up;m_PreviousTanHalfFov=tanHalf;m_PreviousAspect=aspect;m_HistoryValid=true;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}
