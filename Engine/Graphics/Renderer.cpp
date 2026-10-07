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

    // Bring the explicit RHI online during the migration. Rendering still falls
    // through the legacy GL path until swapchain/pipeline migration is complete.
    if (!Velcryn::RHI::Initialize(window, Velcryn::RHI::GraphicsAPI::Vulkan))
    {
        Logger::Error("Failed to initialize the Vulkan RHI.");
        return false;
    }

	m_Context = SDL_GL_CreateContext(window.GetNativeWindow());

	if (m_Context == nullptr)
	{
		Logger::Error(std::string("Failed to create OpenGL context: ") + SDL_GetError());

		return false;
	}

    // Frame pacing is controlled by the application loop.
    if (!SDL_GL_SetSwapInterval(0))
    {
        Logger::Warning(std::string("Failed to disable swap interval: ") + SDL_GetError());
    }

	if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
	{
		Logger::Error("Failed to initialize GLAD.");

		SDL_GL_DestroyContext(m_Context);
		m_Context = nullptr;

		return false;
	}

	glEnable(GL_DEPTH_TEST);

	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	//glFrontFace(GL_CCW);

	if (!m_Framebuffer.Initialize(
		window.GetWidth(),
		window.GetHeight()))
	{
		Logger::Error("Failed to initialize framebuffer.");
		return false;
	}

	m_ViewportWidth = window.GetWidth();
	m_ViewportHeight = window.GetHeight();

	if (!m_Shader.Initialize(vertexShaderSource, fragmentShaderSource))
	{
		Logger::Error("Failed to initialize shader.");

		return false;
	}

	if (!m_GridShader.Initialize(
		gridVertexShaderSource,
		gridFragmentShaderSource))
	{
		Logger::Error(
			"Failed to initialize grid shader."
		);

		return false;
	}

    if (!m_ShadowShader.Initialize(shadowVertexShaderSource, shadowFragmentShaderSource))
    {
        Logger::Error("Failed to initialize shadow shader.");
        return false;
    }

    if (!m_EnvironmentSystem.Initialize())
    {
        Logger::Error("Failed to initialize HDR environment cubemap.");
        return false;
    }

    if (!CreateShadowTarget())
    {
        Logger::Error("Failed to initialize directional shadow map.");
        return false;
    }

    if (!m_PostShader.Initialize(postVertexShaderSource, postFragmentShaderSource) ||
        !m_BloomExtractShader.Initialize(postVertexShaderSource, bloomExtractFragmentShaderSource) ||
        !m_BloomBlurShader.Initialize(postVertexShaderSource, bloomBlurFragmentShaderSource) ||
        !m_TAAShader.Initialize(postVertexShaderSource, taaFragmentShaderSource))
    {
        Logger::Error("Failed to initialize post-process shaders.");
        return false;
    }

    const float postTriangle[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
    glGenVertexArrays(1, &m_PostVAO);
    glGenBuffers(1, &m_PostVBO);
    glBindVertexArray(m_PostVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_PostVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(postTriangle), postTriangle, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);

    if (!CreatePostProcessTarget())
    {
        Logger::Error("Failed to initialize post-process framebuffer.");
        return false;
    }

	if (!m_Grid.Initialize())
	{
		Logger::Error("Failed to initialize grid.");
		return false;
	}

	if (!m_SkyShader.Initialize(skyVertexShaderSource, skyFragmentShaderSource))
	{
		Logger::Error("Failed to initialize sky shader.");
		return false;
	}

	const float skyTriangle[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };
	glGenVertexArrays(1, &m_SkyVAO);
	glGenBuffers(1, &m_SkyVBO);
	glBindVertexArray(m_SkyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_SkyVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyTriangle), skyTriangle, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
	glBindVertexArray(0);

	if (!m_DebugRenderer.Initialize())
	{
		Logger::Error(
			"Failed to initialize debug renderer."
		);

		return false;
	}

	if (!m_UIRenderer.Initialize())
	{
		Logger::Error(
			"Failed to initialize UI renderer."
		);

		return false;
	}

    if (!m_ModelPreviewShader.Initialize(modelPreviewVertexShaderSource, modelPreviewFragmentShaderSource))
    {
        Logger::Error("Failed to initialize model preview shader.");
        return false;
    }

	m_CubeMesh =
		PrimitiveMesh::CreateCube();

	if (!m_CubeMesh)
	{
		Logger::Error("Failed to create cube mesh.");
		return false;
	}

	m_PlaneMesh =
		PrimitiveMesh::CreatePlane();

	if (!m_PlaneMesh)
	{
		Logger::Error("Failed to create plane mesh.");
		return false;
	}

	m_SphereMesh =
		PrimitiveMesh::CreateSphere();

	if (!m_SphereMesh)
	{
		Logger::Error("Failed to create sphere mesh.");
		return false;
	}

	m_CylinderMesh =
		PrimitiveMesh::CreateCylinder();

	if (!m_CylinderMesh)
	{
		Logger::Error(
			"Failed to create cylinder mesh."
		);

		return false;
	}

	int majorVersion = 0;
	int minorVersion = 0;

	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &majorVersion);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minorVersion);

	Logger::Info(
		std::string("OpenGL context: ") +
		std::to_string(majorVersion) +
		"." +
		std::to_string(minorVersion)
	);

	m_Camera.SetAspectRatio(
		static_cast<float>(window.GetWidth()) /
		static_cast<float>(window.GetHeight())
	);

	return true;
}

void Renderer::Shutdown()
{
	m_DebugRenderer.Shutdown();
	m_UIRenderer.Shutdown();
    DestroyModelPreviewCache();
    DestroyModelPreviewTarget();
    m_ModelPreviewShader.Shutdown();

	m_CubeMesh.reset();
	m_PlaneMesh.reset();
	m_SphereMesh.reset();
	m_CylinderMesh.reset();
    m_ModelCache.clear();

	if (m_SkyVBO) glDeleteBuffers(1, &m_SkyVBO);
	if (m_SkyVAO) glDeleteVertexArrays(1, &m_SkyVAO);
	m_SkyVBO = 0;
	m_SkyVAO = 0;
    DestroyPostProcessTarget();
    if (m_PostVBO) glDeleteBuffers(1, &m_PostVBO);
    if (m_PostVAO) glDeleteVertexArrays(1, &m_PostVAO);
    m_PostVBO = 0;
    m_PostVAO = 0;
    m_PostShader.Shutdown();
    m_BloomExtractShader.Shutdown();
    m_BloomBlurShader.Shutdown();
    m_TAAShader.Shutdown();
    m_EnvironmentSystem.Shutdown();
    DestroyShadowTarget();
    m_ShadowShader.Shutdown();
	m_SkyShader.Shutdown();
	m_Grid.Shutdown();
	m_Shader.Shutdown();
	m_GridShader.Shutdown();
	m_Framebuffer.Shutdown();
	m_TextureManager.Clear();

	if (m_Context != nullptr)
	{
		SDL_GL_DestroyContext(m_Context);
		m_Context = nullptr;
	}

	Velcryn::RHI::Shutdown();
	m_Window = nullptr;
}

void Renderer::BeginFrame()
{
    m_FrameViewProjection = m_Camera.GetProjectionMatrix() * m_Camera.GetViewMatrix();
    m_FrameShaderStateReady = false;
    UploadFrameShaderState();
	m_Framebuffer.Bind();

	glViewport(
		0,
		0,
		static_cast<int>(m_ViewportWidth),
		static_cast<int>(m_ViewportHeight)
	);

	glClearColor(
		m_ClearColor[0],
		m_ClearColor[1],
		m_ClearColor[2],
		m_ClearColor[3]
	);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::EndScene()
{
    m_Framebuffer.Resolve();
    m_Framebuffer.Unbind();
    RenderPostProcess();
}

void Renderer::BeginOverlay()
{
    if (!m_PostFramebuffer) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_PostFramebuffer);
    glViewport(0, 0, static_cast<int>(m_ViewportWidth), static_cast<int>(m_ViewportHeight));
}

void Renderer::EndOverlay()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::EndFrame()
{
    SDL_GL_SwapWindow(m_Window->GetNativeWindow());
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
























