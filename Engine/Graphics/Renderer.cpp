#include "Renderer.h"
#include "RHI/RHI.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"

#include "../Core/Logger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <Mesh.vert.h>
#include <Mesh.frag.h>
#include <Preview.frag.h>
#include "MeshDrawData.h"

Renderer::Renderer()
	: m_Window(nullptr),
	m_ClearColor{ 0.16f, 0.23f, 0.36f, 1.0f },
	m_ViewportWidth(0),
	m_ViewportHeight(0),
    m_LightDirection(Vec3(-0.4f, -0.8f, -0.6f).Normalized()),
    m_LightColor(1.0f, 1.0f, 1.0f),
    m_LightIntensity(1.0f)
{
}

Renderer::~Renderer()
{
	Shutdown();
}

bool Renderer::Initialize(Window& window)
{
    m_Window = &window;
    m_ViewportWidth = window.GetWidth();
    m_ViewportHeight = window.GetHeight();

    if (!Velcryn::RHI::Initialize(window, Velcryn::RHI::GraphicsAPI::Vulkan))
    {
        Logger::Error("Failed to initialize the Vulkan RHI.");
        return false;
    }

    if (!m_Framebuffer.Initialize(m_ViewportWidth, m_ViewportHeight, 1, true))
    {
        Logger::Error("Renderer: failed to create the Vulkan scene framebuffer.");
        Velcryn::RHI::Shutdown();
        return false;
    }

    if (!CreatePostProcessTarget())
    {
        Logger::Error("Renderer: failed to create Vulkan post-process targets.");
        m_Framebuffer.Shutdown();
        Velcryn::RHI::Shutdown();
        return false;
    }

    if (!CreateMeshPipelines() || !m_EnvironmentSystem.Initialize() || !CreateShadowTarget()) { Shutdown(); return false; }

    const std::uint8_t whitePixel[4] = {255, 255, 255, 255};
    Velcryn::RHI::TextureDesc whiteDesc{};
    whiteDesc.width = whiteDesc.height = 1;
    whiteDesc.format = Velcryn::RHI::TextureFormat::RGBA8_UNorm;
    whiteDesc.usage = Velcryn::RHI::TextureUsage::Sampled | Velcryn::RHI::TextureUsage::TransferDestination;
    whiteDesc.debugName = "RendererWhiteTexture";
    m_WhiteTexture = Velcryn::RHI::GetDevice()->CreateTexture(whiteDesc, whitePixel, sizeof(whitePixel));
    if (!m_WhiteTexture || !CreateFullscreenPipelines()) { Shutdown(); return false; }

    if (!m_UIRenderer.Initialize()) { Logger::Error("Renderer: failed to initialize Vulkan runtime UI."); Shutdown(); return false; }
    m_UIRenderer.Resize(m_ViewportWidth, m_ViewportHeight);

    if (!m_Grid.Initialize() || !m_DebugRenderer.Initialize()) { Shutdown(); return false; }
    m_CubeMesh = PrimitiveMesh::CreateCube();
    m_PlaneMesh = PrimitiveMesh::CreatePlane();
    m_SphereMesh = PrimitiveMesh::CreateSphere();
    m_CylinderMesh = PrimitiveMesh::CreateCylinder();

    m_Camera.SetAspectRatio(
        static_cast<float>(window.GetWidth()) /
        static_cast<float>(window.GetHeight())
    );

    Logger::Info("Renderer: Vulkan/NRI presentation path initialized.");
    return true;
}

void Renderer::Shutdown()
{
    m_UIRenderer.Shutdown();
    m_Grid.Shutdown();
    m_DebugRenderer.Shutdown();
    m_EnvironmentSystem.Shutdown();
    DestroyModelPreviewCache();
    DestroyFullscreenPipelines();
    DestroyShadowTarget();
    if (auto* device = Velcryn::RHI::GetDevice()) {
        if (m_WhiteTexture) device->DestroyTexture(m_WhiteTexture);
        device->DestroyPipeline(m_MeshPipeline);
        device->DestroyPipeline(m_ShadowPipeline);
        device->DestroyPipeline(m_PreviewPipeline);
    }
    m_WhiteTexture = {};
    m_MeshPipeline = {}; m_ShadowPipeline = {}; m_PreviewPipeline = {};
    // The active renderer is Vulkan/NRI. Do not touch legacy GL objects here:
    // no OpenGL context exists on an SDL_WINDOW_VULKAN window.
    m_CubeMesh.reset();
    m_PlaneMesh.reset();
    m_SphereMesh.reset();
    m_CylinderMesh.reset();
    m_ModelCache.clear();
    m_ModelSourceChecks.clear();
    m_TextureManager.Clear();
    DestroyPostProcessTarget();
    m_Framebuffer.Shutdown();

    Velcryn::RHI::Shutdown();
    m_Window = nullptr;
}

void Renderer::BeginFrame()
{
    m_ShadowMapReady = false;
    m_FrameViewProjection = m_Camera.GetProjectionMatrix() * m_Camera.GetViewMatrix();
    if (auto* device = Velcryn::RHI::GetDevice())
    {
        device->BeginFrame();
        RenderPendingPreviews();
        BeginSceneRendering(true);
    }
}

void Renderer::EndScene()
{
    if (auto* device = Velcryn::RHI::GetDevice()) device->EndRendering();
    RenderPostProcess();
}

void Renderer::BeginOverlay()
{
    if (auto* device = Velcryn::RHI::GetDevice())
        device->BeginRendering(m_PostColorTexture, m_Framebuffer.GetDepthTexture(), m_ClearColor, false);
}

void Renderer::EndOverlay()
{
    if (auto* device = Velcryn::RHI::GetDevice())
        device->EndRendering();
}

void Renderer::EndFrame()
{
    if (auto* device = Velcryn::RHI::GetDevice())
    {
        // Hub frames do not run the scene BeginFrame path, so acquire here
        // before presenting. Scene frames already acquired their backbuffer.
        device->BeginFrame();
        device->EndFrame();
    }
}

void Renderer::SetClearColor(float red, float green, float blue, float alpha)
{
	m_ClearColor[0] = red;
	m_ClearColor[1] = green;
	m_ClearColor[2] = blue;
	m_ClearColor[3] = alpha;
}

std::uint64_t Renderer::GetViewportTexture() const { if(auto*d=Velcryn::RHI::GetDevice()) return d->GetImGuiTextureID(m_PostColorTexture); return 0; }

void Renderer::ResizeViewport(
	unsigned int width,
	unsigned int height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (width == m_ViewportWidth &&
		height == m_ViewportHeight)
	{
		return;
	}

	m_ViewportWidth = width;
	m_ViewportHeight = height;

    if (!m_Framebuffer.Resize(width, height))
    {
        Logger::Error("Viewport framebuffer resize failed.");
    }
    DestroyPostProcessTarget();
    if (!CreatePostProcessTarget())
    {
        Logger::Error("Post-process framebuffer resize failed.");
    }

	m_Camera.SetAspectRatio(
		static_cast<float>(width) /
		static_cast<float>(height)
	);

	m_UIRenderer.Resize(
		width,
		height
	);
}

bool Renderer::CreateMeshPipelines()
{
    auto* device=Velcryn::RHI::GetDevice();
    device->DestroyPipeline(m_MeshPipeline);device->DestroyPipeline(m_ShadowPipeline);device->DestroyPipeline(m_PreviewPipeline);
    const Velcryn::RHI::VertexAttribute attributes[] = {
        {0, offsetof(Vertex, position), Velcryn::RHI::VertexFormat::Float3},
        {1, offsetof(Vertex, normal), Velcryn::RHI::VertexFormat::Float3},
        {2, offsetof(Vertex, uv), Velcryn::RHI::VertexFormat::Float2},
        {3, offsetof(MeshVertex, joints), Velcryn::RHI::VertexFormat::Float4},
        {4, offsetof(MeshVertex, weights), Velcryn::RHI::VertexFormat::Float4}
    };
    Velcryn::RHI::GraphicsPipelineDesc pipeline{};
    pipeline.vertexShader = Mesh_vert; pipeline.fragmentShader = Mesh_frag;
    pipeline.attributes = attributes; pipeline.vertexStride = sizeof(MeshVertex);
    pipeline.constantSize = sizeof(Mat4) + sizeof(float) * 16;
    pipeline.sampledTexture = true;
    pipeline.cullBackFaces = false; // Existing scenes include two-sided geometry.
    pipeline.secondColor = true; pipeline.textureCount = 11; pipeline.rawTextureMask = 0x3f; // Preserve the OpenGL-authored working space for existing assets.
    pipeline.uniformSize = sizeof(MeshDrawData);
    pipeline.sampleCount=m_Framebuffer.GetSamples();
    pipeline.debugName = "BasicMesh";
    m_MeshPipeline = Velcryn::RHI::GetDevice()->CreateGraphicsPipeline(pipeline);
    if (!m_MeshPipeline) return false;
    pipeline.sampleCount=1;
    pipeline.secondColor = false; pipeline.textureCount = 1; pipeline.rawTextureMask = 1;
    pipeline.fragmentShader = Preview_frag; pipeline.debugName = "ModelPreview";
    m_PreviewPipeline = Velcryn::RHI::GetDevice()->CreateGraphicsPipeline(pipeline);
    if (!m_PreviewPipeline) return false;
    pipeline.secondColor = false; pipeline.fragmentShader = {}; pipeline.colorFormat = Velcryn::RHI::TextureFormat::Unknown;
    pipeline.textureCount = 1; pipeline.cullFrontFaces = true; pipeline.debugName = "ShadowDepth";
    m_ShadowPipeline = Velcryn::RHI::GetDevice()->CreateGraphicsPipeline(pipeline);
    return bool(m_ShadowPipeline);

}

void Renderer::BeginSceneRendering(bool clear, bool normals)
{
    auto* device=Velcryn::RHI::GetDevice();
    if(!device)return;
    const bool multisampled=m_Framebuffer.GetSamples()>1;
    device->BeginRendering(m_Framebuffer.GetRenderColor(),m_Framebuffer.GetRenderDepth(),m_ClearColor,clear,
        normals?m_Framebuffer.GetRenderNormal():Velcryn::RHI::TextureHandle{},
        multisampled?m_Framebuffer.GetColorTexture():Velcryn::RHI::TextureHandle{},
        multisampled?m_Framebuffer.GetDepthTexture():Velcryn::RHI::TextureHandle{},
        multisampled&&normals?m_Framebuffer.GetNormalTexture():Velcryn::RHI::TextureHandle{});
}
