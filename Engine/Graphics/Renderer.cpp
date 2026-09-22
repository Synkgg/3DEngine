#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"

#include "../Core/Logger.h"
#include <iostream>

static const char* vertexShaderSource = R"(
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

uniform mat4 u_Transform;
uniform mat4 u_Model;

out vec3 v_Normal;
out vec2 v_UV;

void main()
{
    v_Normal = a_Normal;
    v_UV = a_UV;

    gl_Position =
        u_Transform *
        vec4(a_Position, 1.0);
}
)";

static const char* fragmentShaderSource = R"(
#version 450 core

in vec3 v_Normal;
in vec2 v_UV;

uniform vec4 u_Color;

uniform sampler2D u_Texture;
uniform int u_UseTexture;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;

out vec4 FragColor;

void main()
{
    vec3 normal =
        normalize(v_Normal);

    vec3 lightDirection =
        normalize(-u_LightDirection);

    float diffuse =
        max(
            dot(normal, lightDirection),
            0.0
        );

    float ambient = 0.25;

    float brightness =
        ambient +
        diffuse * u_LightIntensity;

    vec4 baseColor =
        u_Color;

    if (u_UseTexture != 0)
    {
        baseColor *=
            texture(
                u_Texture,
                v_UV
            );
    }

    FragColor =
        vec4(
            baseColor.rgb *
            u_LightColor *
            brightness,
            baseColor.a
        );
}
)";

static const char* gridVertexShaderSource = R"(
#version 450 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Transform;

void main()
{
    gl_Position =
        u_Transform *
        vec4(a_Position, 1.0);
}
)";

static const char* gridFragmentShaderSource = R"(
#version 450 core

uniform vec4 u_Color;

out vec4 FragColor;

void main()
{
    FragColor = u_Color;
}
)";

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

	m_Context = SDL_GL_CreateContext(window.GetNativeWindow());

	if (m_Context == nullptr)
	{
		Logger::Error(std::string("Failed to create OpenGL context: ") + SDL_GetError());

		return false;
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

	if (!m_Grid.Initialize())
	{
		Logger::Error("Failed to initialize grid.");
		return false;
	}

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

	m_CubeMesh.reset();
	m_PlaneMesh.reset();
	m_SphereMesh.reset();
	m_CylinderMesh.reset();

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

	m_Window = nullptr;
}

void Renderer::BeginFrame()
{
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
	m_Framebuffer.Unbind();
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

void Renderer::DrawMesh(
	const Transform& transform,
	PrimitiveType primitive,
	float red,
	float green,
	float blue,
	float alpha,
	const Texture2D* texture)
{
	Mesh* mesh = nullptr;

	switch (primitive)
	{
	case PrimitiveType::None:
		return;

	case PrimitiveType::Cube:
		mesh = m_CubeMesh.get();
		break;

	case PrimitiveType::Plane:
		mesh = m_PlaneMesh.get();
		break;

	case PrimitiveType::Sphere:
		mesh = m_SphereMesh.get();
		break;

	case PrimitiveType::Cylinder:
		mesh = m_CylinderMesh.get();
		break;
	}

	if (mesh == nullptr)
	{
		return;
	}

	Mat4 model =
		transform.GetMatrix();

	Mat4 view =
		m_Camera.GetViewMatrix();

	Mat4 projection =
		m_Camera.GetProjectionMatrix();

	Mat4 cameraTransform =
		projection * view * model;

	mesh->Bind();
	m_Shader.Bind();

	if (texture != nullptr &&
		texture->IsLoaded())
	{
		texture->Bind(0);

		m_Shader.SetInt(
			"u_Texture",
			0
		);

		m_Shader.SetInt(
			"u_UseTexture",
			1
		);
	}
	else
	{
		m_Shader.SetInt(
			"u_UseTexture",
			0
		);
	}

	m_Shader.SetMat4(
		"u_Transform",
		cameraTransform
	);

	m_Shader.SetVec4(
		"u_Color",
		red,
		green,
		blue,
		alpha
	);

	m_Shader.SetMat4(
		"u_Model",
		model
	);

	m_Shader.SetVec3(
		"u_LightDirection",
		m_LightDirection.x,
		m_LightDirection.y,
		m_LightDirection.z
	);

	m_Shader.SetVec3(
		"u_LightColor",
		m_LightColor.x,
		m_LightColor.y,
		m_LightColor.z
	);

	m_Shader.SetFloat(
		"u_LightIntensity",
		m_LightIntensity
	);

	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(
			mesh->GetIndexCount()
			),
		GL_UNSIGNED_INT,
		nullptr
	);

	if (texture != nullptr &&
		texture->IsLoaded())
	{
		texture->Unbind();
	}

	m_Shader.Unbind();
	mesh->Unbind();
}

SDL_GLContext Renderer::GetContext() const
{
	return m_Context;
}

unsigned int Renderer::GetViewportTexture() const
{
	return m_Framebuffer.GetColorTexture();
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

	m_Framebuffer.Resize(width, height);

	m_Camera.SetAspectRatio(
		static_cast<float>(width) /
		static_cast<float>(height)
	);

	m_UIRenderer.Resize(
		width,
		height
	);
}

void Renderer::RotateCamera(float yawDelta, float pitchDelta)
{
	m_Camera.Rotate(yawDelta, pitchDelta);
}

void Renderer::MoveCamera(
	float forward,
	float right,
	float up,
	float deltaTime)
{
	m_Camera.Move(
		forward,
		right,
		up,
		deltaTime
	);
}

void Renderer::ResetCamera()
{
	m_Camera.Reset();
}

void Renderer::DrawGrid()
{
	Mat4 model =
		Mat4::Identity();

	Mat4 view =
		m_Camera.GetViewMatrix();

	Mat4 projection =
		m_Camera.GetProjectionMatrix();

	Mat4 transform =
		projection *
		view *
		model;

	m_GridShader.Bind();

	m_GridShader.SetMat4(
		"u_Transform",
		transform
	);

	m_GridShader.SetVec4(
		"u_Color",
		0.35f,
		0.35f,
		0.35f,
		1.0f
	);

	m_Grid.Draw();

	m_GridShader.Unbind();
}

Mat4 Renderer::GetCameraViewMatrix() const
{
	return m_Camera.GetViewMatrix();
}

Mat4 Renderer::GetCameraProjectionMatrix() const
{
	return m_Camera.GetProjectionMatrix();
}

Vec3 Renderer::GetCameraPosition() const
{
	return m_Camera.GetPosition();
}

void Renderer::SetCameraPosition(
	const Vec3& position)
{
	m_Camera.SetPosition(position);
}

float Renderer::GetCameraYaw() const
{
	return m_Camera.GetYaw();
}

float Renderer::GetCameraPitch() const
{
	return m_Camera.GetPitch();
}

Vec3 Renderer::GetCameraForward() const
{
	return m_Camera.GetForward();
}

Vec3 Renderer::GetCameraRight() const
{
	return m_Camera.GetRight();
}

void Renderer::SetCameraRotation(float yaw, float pitch)
{
	m_Camera.SetRotation(yaw, pitch);
}

Vec3 Renderer::GetCameraRayDirection(
	float ndcX,
	float ndcY) const
{
	return m_Camera.GetRayDirection(
		ndcX,
		ndcY
	);
}

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

Texture2D* Renderer::LoadTexture(
	const std::string& filepath)
{
	return m_TextureManager.Load(
		filepath
	);
}