#include "Renderer.h"
#include "RHI/RHI.h"
#include "OpenGL/OpenGLShaderSources.h"

using namespace Velcryn::Graphics::OpenGLShaders;
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"

#include "../Core/Logger.h"
#include <iostream>
#include <algorithm>
#include <cmath>














Renderer::Renderer()
	: m_Context(nullptr),
	m_Window(nullptr),
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

    m_Camera.SetAspectRatio(
        static_cast<float>(window.GetWidth()) /
        static_cast<float>(window.GetHeight())
    );

    Logger::Info("Renderer: Vulkan/NRI presentation path initialized.");
    return true;
}

void Renderer::Shutdown()
{
    // The active renderer is Vulkan/NRI. Do not touch legacy GL objects here:
    // no OpenGL context exists on an SDL_WINDOW_VULKAN window.
    m_CubeMesh.reset();
    m_PlaneMesh.reset();
    m_SphereMesh.reset();
    m_CylinderMesh.reset();
    m_ModelCache.clear();
    m_TextureManager.Clear();

    Velcryn::RHI::Shutdown();
    m_Context = nullptr;
    m_Window = nullptr;
}

void Renderer::BeginFrame()
{
    m_FrameViewProjection = m_Camera.GetProjectionMatrix() * m_Camera.GetViewMatrix();
    if (auto* device = Velcryn::RHI::GetDevice())
        device->BeginFrame();
}

void Renderer::EndScene()
{
}

void Renderer::BeginOverlay()
{
}

void Renderer::EndOverlay()
{
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










SDL_GLContext Renderer::GetContext() const
{
	return m_Context;
}

unsigned int Renderer::GetViewportTexture() const
{
	return m_PostColorTexture ? m_PostColorTexture : m_Framebuffer.GetColorTexture();
}

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
























