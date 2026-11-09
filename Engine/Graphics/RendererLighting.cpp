#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Core/Logger.h"
#include "RHI/RHI.h"
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
if(auto* d=Velcryn::RHI::GetDevice()){d->EndRendering();BeginSceneRendering(false,false);}
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

if(auto* d=Velcryn::RHI::GetDevice()){d->EndRendering();BeginSceneRendering(false,false);}
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

    const unsigned int requested=m_RenderSettings.antiAliasing?static_cast<unsigned int>(m_RenderSettings.antiAliasingSamples):1u;
    const unsigned int samples=requested>=8?8:requested>=4?4:requested>=2?2:1;
    if(m_Framebuffer.GetSamples()!=samples) {
        if(!m_Framebuffer.SetSamples(samples)) {Logger::Error("Failed to apply MSAA targets.");return;}
        DestroyFullscreenPipelines();m_Grid.Shutdown();m_DebugRenderer.Shutdown();
        if(!CreateMeshPipelines()||!CreateFullscreenPipelines()||!m_Grid.Initialize(samples)||!m_DebugRenderer.Initialize(samples))
            Logger::Error("Failed to apply MSAA pipelines.");
        m_HistoryValid=false;
    }


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
    auto* device = Velcryn::RHI::GetDevice();
    if (!device)
        return false;

    for (int i = 0; i < ShadowCascadeCount; ++i)
    {
        Velcryn::RHI::TextureDesc desc{};
        desc.width = m_ShadowMapSizes[i];
        desc.height = m_ShadowMapSizes[i];
        desc.format = Velcryn::RHI::TextureFormat::D32_Float;
        desc.usage = Velcryn::RHI::TextureUsage::DepthStencil |
                     Velcryn::RHI::TextureUsage::Sampled;
        desc.debugName = "CascadedShadowDepth";
        m_ShadowDepthTextures[i] = device->CreateTexture(desc);
        if (!m_ShadowDepthTextures[i])
        {
            DestroyShadowTarget();
            return false;
        }
    }
    return true;
}

void Renderer::DestroyShadowTarget()
{
    if (auto* device = Velcryn::RHI::GetDevice())
        for (auto& texture : m_ShadowDepthTextures)
        {
            if (texture) device->DestroyTexture(texture);
            texture = {};
        }
    m_ShadowMapReady = false;
}

bool Renderer::BeginShadowPass(int cascadeIndex)
{
    if(cascadeIndex==0){m_ShadowMapReady=false;UpdateLightSpaceMatrices();}
    if(!m_RenderSettings.shadows||cascadeIndex<0||cascadeIndex>=ShadowCascadeCount||!m_ShadowDepthTextures[cascadeIndex])return false;
    auto* device=Velcryn::RHI::GetDevice();if(!device)return false;
    device->EndRendering();m_ActiveShadowCascade=cascadeIndex;
    m_InShadowPass=device->BeginRendering({},m_ShadowDepthTextures[cascadeIndex],m_ClearColor);
    return m_InShadowPass;
}
void Renderer::DrawShadowMesh(const Transform& transform,PrimitiveType primitive)
{
    if(m_InShadowPass)DrawMesh(transform,primitive,1,1,1,1);
}
void Renderer::DrawShadowModel(const Transform& transform,const std::string& path)
{
    if(m_InShadowPass)DrawModel(transform,path,1,1,1,1);
}
void Renderer::DrawAnimatedShadowModel(const Transform& transform,const std::string& path,std::size_t clip,float time,bool loop)
{
    if(m_InShadowPass)DrawAnimatedModel(transform,path,clip,time,loop,1,1,1,1);
}
void Renderer::EndShadowPass()
{
    if(!m_InShadowPass)return;
    auto* device=Velcryn::RHI::GetDevice();device->EndRendering();m_InShadowPass=false;
    if(m_ActiveShadowCascade==ShadowCascadeCount-1)m_ShadowMapReady=true;
    BeginSceneRendering();
}

void Renderer::AddDebugLine(const Vec3& start,const Vec3& end,const Vec3& color,float duration){m_DebugLines.push_back({start,end,color,duration});}
void Renderer::DrawDebugLines(float deltaTime){if(auto* d=Velcryn::RHI::GetDevice()){d->EndRendering();BeginSceneRendering(false,false);}for(const DebugLine& line:m_DebugLines)m_DebugRenderer.DrawLine(GetCameraViewMatrix(),GetCameraProjectionMatrix(),line.start,line.end,line.color);for(auto it=m_DebugLines.begin();it!=m_DebugLines.end();){if(it->remaining<=0.0f||(it->remaining-=deltaTime)<=0.0f)it=m_DebugLines.erase(it);else ++it;}}
