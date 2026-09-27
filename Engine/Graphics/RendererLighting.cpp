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

void Renderer::SetDirectionalLight(
	const Vec3& direction,
	const Vec3& color,
	float intensity)
{
	if (direction.Length() > 0.0f)
	{
		m_LightDirection =
			direction.Normalized();
	}

	m_LightColor = color;
	m_LightIntensity = intensity;
}

void Renderer::DrawDirectionalLight(
	const Vec3& position,
	const Vec3& direction)
{
	m_DebugRenderer.DrawDirectionalLight(
		m_Camera.GetViewMatrix(),
		m_Camera.GetProjectionMatrix(),
		position,
		direction
	);
}

void Renderer::DrawCollider(
	const Transform& transform,
	float width,
	float height,
	float depth)
{
	const Vec3 halfExtents(
		width * 0.5f,
		height * 0.5f,
		depth * 0.5f
	);

	m_DebugRenderer.DrawBox(
		GetCameraViewMatrix(),
		GetCameraProjectionMatrix(),
		transform.position,
		halfExtents,
		transform.rotation
	);
}

void Renderer::ClearLocalLights(){ m_PointLightCount=0; m_SpotLightCount=0; }

void Renderer::AddPointLight(const PointLightData& light){ if(m_PointLightCount<(int)m_PointLights.size()) m_PointLights[m_PointLightCount++]=light; }

void Renderer::AddSpotLight(const SpotLightData& light){ if(m_SpotLightCount<(int)m_SpotLights.size()) m_SpotLights[m_SpotLightCount++]=light; }

void Renderer::SetRenderSettings(const RenderSettings& settings)
{
    m_RenderSettings = settings;
    m_RenderSettings.viewDistance = std::clamp(m_RenderSettings.viewDistance, 25.0f, 10000.0f);
    m_RenderSettings.exposure = std::clamp(m_RenderSettings.exposure, 0.1f, 5.0f);
    m_RenderSettings.fogDensity = std::clamp(m_RenderSettings.fogDensity, 0.0f, 0.1f);
    m_RenderSettings.bloomStrength = std::clamp(m_RenderSettings.bloomStrength, 0.0f, 2.0f);
    m_RenderSettings.antiAliasingSamples = std::clamp(m_RenderSettings.antiAliasingSamples, 1, 8);
    m_RenderSettings.shadowQuality = std::clamp(m_RenderSettings.shadowQuality, 0, 3);
    m_RenderSettings.shadowDistance = std::clamp(m_RenderSettings.shadowDistance, 10.0f, 500.0f);
    m_RenderSettings.indirectLightStrength = std::clamp(m_RenderSettings.indirectLightStrength, 0.0f, 2.5f);
    m_RenderSettings.reflectionStrength = std::clamp(m_RenderSettings.reflectionStrength, 0.0f, 2.5f);
    m_RenderSettings.contactShadowStrength = std::clamp(m_RenderSettings.contactShadowStrength, 0.0f, 1.5f);
    m_RenderSettings.skyIntensity = std::clamp(m_RenderSettings.skyIntensity, 0.1f, 3.0f);
    m_RenderSettings.atmosphereStrength = std::clamp(m_RenderSettings.atmosphereStrength, 0.0f, 2.0f);
    m_RenderSettings.colorSaturation = std::clamp(m_RenderSettings.colorSaturation, 0.0f, 2.0f);
    m_RenderSettings.contrast = std::clamp(m_RenderSettings.contrast, 0.5f, 1.6f);
    m_RenderSettings.screenSpaceReflectionStrength = std::clamp(m_RenderSettings.screenSpaceReflectionStrength, 0.0f, 1.0f);
    m_RenderSettings.giStrength = std::clamp(m_RenderSettings.giStrength, 0.0f, 1.5f);
    const unsigned int baseShadowSize = m_RenderSettings.shadowQuality <= 0 ? 1024u : m_RenderSettings.shadowQuality == 1 ? 2048u : 4096u;
    const std::array<unsigned int, ShadowCascadeCount> desiredSizes{ baseShadowSize, std::max(1024u, baseShadowSize / 2u), 1024u };
    if (desiredSizes != m_ShadowMapSizes) { m_ShadowMapSizes = desiredSizes; if (!CreateShadowTarget()) Logger::Error("Failed to resize cascaded shadow maps."); }
    m_Camera.SetFarPlane(m_RenderSettings.viewDistance);

    const unsigned int samples = m_RenderSettings.antiAliasing
        ? static_cast<unsigned int>(m_RenderSettings.antiAliasingSamples) : 1u;
    if (m_Framebuffer.GetSamples() != samples &&
        !m_Framebuffer.SetSamples(samples))
    {
        Logger::Error("Failed to apply anti-aliasing sample count.");
        m_RenderSettings.antiAliasing = m_Framebuffer.GetSamples() > 1;
        m_RenderSettings.antiAliasingSamples = static_cast<int>(m_Framebuffer.GetSamples());
    }

    if (samples > 1) glEnable(GL_MULTISAMPLE);
    else glDisable(GL_MULTISAMPLE);
}

const RenderSettings& Renderer::GetRenderSettings() const
{
    return m_RenderSettings;
}

void Renderer::UpdateLightSpaceMatrices()
{
    const float maxDistance = m_RenderSettings.shadowDistance;
    m_ShadowCascadeSplits = { maxDistance * 0.15f, maxDistance * 0.40f, maxDistance };
    Vec3 direction = m_LightDirection.Length() > 0.001f ? m_LightDirection.Normalized() : Vec3(-0.5f, -1.0f, -0.5f).Normalized();
    Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(Vec3::Dot(direction, up)) > 0.96f) up = Vec3(0.0f, 0.0f, 1.0f);
    float previousSplit = 0.1f;
    for (int i = 0; i < ShadowCascadeCount; ++i)
    {
        const float split = m_ShadowCascadeSplits[i];
        const float centerDistance = (previousSplit + split) * 0.5f;
        const float extent = std::max(6.0f, split * 0.72f);
        Vec3 center = m_Camera.GetPosition() + m_Camera.GetForward() * centerDistance;
        const float texel = (extent * 2.0f) / static_cast<float>(std::max(1u, m_ShadowMapSizes[i]));
        center.x = std::floor(center.x / texel + 0.5f) * texel;
        center.y = std::floor(center.y / texel + 0.5f) * texel;
        center.z = std::floor(center.z / texel + 0.5f) * texel;
        const Vec3 lightPosition = center - direction * (split + extent);
        const Mat4 view = Mat4::LookAt(lightPosition, center, up);
        const Mat4 projection = Mat4::Orthographic(-extent, extent, -extent, extent, 0.1f, (split + extent) * 3.0f);
        m_LightSpaceMatrices[i] = projection * view;
        previousSplit = split;
    }
}

bool Renderer::CreateShadowTarget()
{
    DestroyShadowTarget();
    for (int i = 0; i < ShadowCascadeCount; ++i)
    {
        glGenFramebuffers(1, &m_ShadowFramebuffers[i]);
        glGenTextures(1, &m_ShadowDepthTextures[i]);
        glBindTexture(GL_TEXTURE_2D, m_ShadowDepthTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_ShadowMapSizes[i], m_ShadowMapSizes[i], 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float border[] = { 1,1,1,1 }; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFramebuffers[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_ShadowDepthTextures[i], 0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { glBindFramebuffer(GL_FRAMEBUFFER, 0); DestroyShadowTarget(); return false; }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); return true;
}

void Renderer::DestroyShadowTarget()
{
    for (int i = 0; i < ShadowCascadeCount; ++i) {
        if (m_ShadowDepthTextures[i]) glDeleteTextures(1, &m_ShadowDepthTextures[i]);
        if (m_ShadowFramebuffers[i]) glDeleteFramebuffers(1, &m_ShadowFramebuffers[i]);
        m_ShadowDepthTextures[i] = 0; m_ShadowFramebuffers[i] = 0;
    }
    m_ShadowMapReady = false;
}

void Renderer::BeginShadowPass(int cascadeIndex)
{
    if (cascadeIndex == 0) { m_ShadowMapReady = false; UpdateLightSpaceMatrices(); }
    if (!m_RenderSettings.shadows || cascadeIndex < 0 || cascadeIndex >= ShadowCascadeCount || !m_ShadowFramebuffers[cascadeIndex]) return;
    m_ActiveShadowCascade = cascadeIndex;
    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFramebuffers[cascadeIndex]);
    glViewport(0, 0, static_cast<int>(m_ShadowMapSizes[cascadeIndex]), static_cast<int>(m_ShadowMapSizes[cascadeIndex]));
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
}

void Renderer::DrawShadowMesh(const Transform& transform, PrimitiveType primitive)
{
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffers[m_ActiveShadowCascade])
        return;

    Mesh* mesh = GetPrimitiveMesh(primitive);
    if (!mesh) return;

    mesh->Bind();
    m_ShadowShader.Bind();
    m_ShadowShader.SetMat4("u_Model", transform.GetMatrix());
    m_ShadowShader.SetMat4("u_LightSpaceMatrix", m_LightSpaceMatrices[m_ActiveShadowCascade]);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
    m_ShadowShader.Unbind();
    mesh->Unbind();
}

void Renderer::DrawShadowModel(const Transform& transform, const std::string& modelPath)
{
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffers[m_ActiveShadowCascade]) return;
    Mesh* mesh = GetModelMesh(modelPath);
    if (!mesh) return;
    mesh->Bind();
    m_ShadowShader.Bind();
    m_ShadowShader.SetMat4("u_Model", transform.GetMatrix());
    m_ShadowShader.SetMat4("u_LightSpaceMatrix", m_LightSpaceMatrices[m_ActiveShadowCascade]);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
    m_ShadowShader.Unbind();
    mesh->Unbind();
}

void Renderer::EndShadowPass()
{
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffers[m_ActiveShadowCascade]) return;
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (m_ActiveShadowCascade == ShadowCascadeCount - 1) m_ShadowMapReady = true;
}
