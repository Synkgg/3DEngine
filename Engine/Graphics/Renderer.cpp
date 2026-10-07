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














Renderer::Renderer()
	: m_Window(nullptr),
	m_ClearColor{ 0.1f, 0.1f, 0.15f, 1.0f },
	m_ViewportWidth(0),
	m_ViewportHeight(0)
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

    const Velcryn::RHI::VertexAttribute attributes[] = {
        {0, offsetof(Vertex, position), Velcryn::RHI::VertexFormat::Float3},
        {1, offsetof(Vertex, normal), Velcryn::RHI::VertexFormat::Float3},
        {2, offsetof(Vertex, uv), Velcryn::RHI::VertexFormat::Float2}
    };
    Velcryn::RHI::GraphicsPipelineDesc pipeline{};
    pipeline.vertexShader = Mesh_vert; pipeline.fragmentShader = Mesh_frag;
    pipeline.attributes = attributes; pipeline.vertexStride = sizeof(Vertex);
    pipeline.constantSize = sizeof(Mat4) + sizeof(float) * 16;
    pipeline.sampledTexture = true;
    pipeline.debugName = "BasicMesh";
    m_MeshPipeline = Velcryn::RHI::GetDevice()->CreateGraphicsPipeline(pipeline);
    if (!m_MeshPipeline) { Shutdown(); return false; }

    const std::uint8_t whitePixel[4] = {255, 255, 255, 255};
    Velcryn::RHI::TextureDesc whiteDesc{};
    whiteDesc.width = whiteDesc.height = 1;
    whiteDesc.format = Velcryn::RHI::TextureFormat::RGBA8_UNorm;
    whiteDesc.usage = Velcryn::RHI::TextureUsage::Sampled | Velcryn::RHI::TextureUsage::TransferDestination;
    whiteDesc.debugName = "RendererWhiteTexture";
    m_WhiteTexture = Velcryn::RHI::GetDevice()->CreateTexture(whiteDesc, whitePixel, sizeof(whitePixel));
    if (!m_WhiteTexture) { Shutdown(); return false; }

    if (!m_UIRenderer.Initialize()) { Logger::Error("Renderer: failed to initialize Vulkan runtime UI."); Shutdown(); return false; }
    m_UIRenderer.Resize(m_ViewportWidth, m_ViewportHeight);

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
    if (auto* device = Velcryn::RHI::GetDevice()) {
        if (m_WhiteTexture) device->DestroyTexture(m_WhiteTexture);
        device->DestroyPipeline(m_MeshPipeline);
    }
    m_WhiteTexture = {};
    m_MeshPipeline = {};
    // The active renderer is Vulkan/NRI. Do not touch legacy GL objects here:
    // no OpenGL context exists on an SDL_WINDOW_VULKAN window.
    m_CubeMesh.reset();
    m_PlaneMesh.reset();
    m_SphereMesh.reset();
    m_CylinderMesh.reset();
    m_ModelCache.clear();
    m_TextureManager.Clear();
    DestroyPostProcessTarget();
    m_Framebuffer.Shutdown();

    Velcryn::RHI::Shutdown();
    m_Window = nullptr;
}

void Renderer::BeginFrame()
{
    m_FrameViewProjection = m_Camera.GetProjectionMatrix() * m_Camera.GetViewMatrix();
    if (auto* device = Velcryn::RHI::GetDevice())
    {
        device->BeginFrame();
        device->BeginRendering(m_Framebuffer.GetColorTexture(), m_Framebuffer.GetDepthTexture(), m_ClearColor);
    }
}

void Renderer::EndScene()
{
    if (auto* device = Velcryn::RHI::GetDevice()) device->EndRendering();
}

void Renderer::BeginOverlay()
{
    if (auto* device = Velcryn::RHI::GetDevice())
        device->BeginRendering(m_Framebuffer.GetColorTexture(), m_Framebuffer.GetDepthTexture(), m_ClearColor, false);
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










std::uint64_t Renderer::GetViewportTexture() const { if(auto*d=Velcryn::RHI::GetDevice()) return d->GetImGuiTextureID(m_Framebuffer.GetColorTexture()); return 0; }

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
























