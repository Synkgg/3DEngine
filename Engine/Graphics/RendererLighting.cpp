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
    const unsigned int desiredShadowSize =
        m_RenderSettings.shadowQuality <= 0 ? 1024u :
        m_RenderSettings.shadowQuality == 1 ? 2048u :
        m_RenderSettings.shadowQuality == 2 ? 4096u : 4096u;
    if (desiredShadowSize != m_ShadowMapSize)
    {
        m_ShadowMapSize = desiredShadowSize;
        DestroyShadowTarget();
        if (!CreateShadowTarget())
            Logger::Error("Failed to resize directional shadow map.");
    }
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

void Renderer::UpdateLightSpaceMatrix()
{
    const float distance = m_RenderSettings.shadowDistance;
    const Vec3 center = m_Camera.GetPosition() + m_Camera.GetForward() * (distance * 0.28f);
    Vec3 direction = m_LightDirection.Length() > 0.001f ? m_LightDirection.Normalized() : Vec3(-0.5f, -1.0f, -0.5f).Normalized();
    const Vec3 lightPosition = center - direction * distance;
    Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(Vec3::Dot(direction, up)) > 0.96f)
        up = Vec3(0.0f, 0.0f, 1.0f);

    const float extent = distance * 0.62f;
    // Snap the shadow focus to shadow-map texels. This removes the crawling
    // shimmer that otherwise appears whenever the camera translates.
    const float worldUnitsPerTexel = (extent * 2.0f) / static_cast<float>(std::max(1u, m_ShadowMapSize));
    Vec3 stableCenter = center;
    if (worldUnitsPerTexel > 0.000001f)
    {
        stableCenter.x = std::floor(stableCenter.x / worldUnitsPerTexel + 0.5f) * worldUnitsPerTexel;
        stableCenter.y = std::floor(stableCenter.y / worldUnitsPerTexel + 0.5f) * worldUnitsPerTexel;
        stableCenter.z = std::floor(stableCenter.z / worldUnitsPerTexel + 0.5f) * worldUnitsPerTexel;
    }
    const Vec3 stableLightPosition = stableCenter - direction * distance;
    const Mat4 lightView = Mat4::LookAt(stableLightPosition, stableCenter, up);
    const Mat4 lightProjection = Mat4::Orthographic(-extent, extent, -extent, extent, 0.1f, distance * 2.5f);
    m_LightSpaceMatrix = lightProjection * lightView;
}

bool Renderer::CreateShadowTarget()
{
    glGenFramebuffers(1, &m_ShadowFramebuffer);
    glGenTextures(1, &m_ShadowDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_ShadowDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_ShadowMapSize, m_ShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_ShadowDepthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) { DestroyShadowTarget(); return false; }
    return true;
}

void Renderer::DestroyShadowTarget()
{
    if (m_ShadowDepthTexture) glDeleteTextures(1, &m_ShadowDepthTexture);
    if (m_ShadowFramebuffer) glDeleteFramebuffers(1, &m_ShadowFramebuffer);
    m_ShadowDepthTexture = 0;
    m_ShadowFramebuffer = 0;
    m_ShadowMapReady = false;
}

void Renderer::BeginShadowPass()
{
    m_ShadowMapReady = false;
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffer)
        return;
    UpdateLightSpaceMatrix();
    glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFramebuffer);
    glViewport(0, 0, static_cast<int>(m_ShadowMapSize), static_cast<int>(m_ShadowMapSize));
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
}

void Renderer::DrawShadowMesh(const Transform& transform, PrimitiveType primitive)
{
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffer)
        return;

    Mesh* mesh = GetPrimitiveMesh(primitive);
    if (!mesh) return;

    mesh->Bind();
    m_ShadowShader.Bind();
    m_ShadowShader.SetMat4("u_Model", transform.GetMatrix());
    m_ShadowShader.SetMat4("u_LightSpaceMatrix", m_LightSpaceMatrix);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
    m_ShadowShader.Unbind();
    mesh->Unbind();
}

void Renderer::DrawShadowModel(const Transform& transform, const std::string& modelPath)
{
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffer) return;
    Mesh* mesh = GetModelMesh(modelPath);
    if (!mesh) return;
    mesh->Bind();
    m_ShadowShader.Bind();
    m_ShadowShader.SetMat4("u_Model", transform.GetMatrix());
    m_ShadowShader.SetMat4("u_LightSpaceMatrix", m_LightSpaceMatrix);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh->GetIndexCount()), GL_UNSIGNED_INT, nullptr);
    m_ShadowShader.Unbind();
    mesh->Unbind();
}

void Renderer::EndShadowPass()
{
    if (!m_RenderSettings.shadows || !m_ShadowFramebuffer)
        return;
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_ShadowMapReady = true;
}
