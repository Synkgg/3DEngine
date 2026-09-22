#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"

#include "../Core/Logger.h"
#include <iostream>
#include <algorithm>

static const char* vertexShaderSource = R"(
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

uniform mat4 u_Transform;
uniform mat4 u_Model;

out vec3 v_Normal;
out vec3 v_WorldPosition;
out vec2 v_UV;

void main()
{
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    v_Normal = normalize(normalMatrix * a_Normal);
    v_WorldPosition = vec3(u_Model * vec4(a_Position, 1.0));
    v_UV = a_UV;

    gl_Position =
        u_Transform *
        vec4(a_Position, 1.0);
}
)";

static const char* fragmentShaderSource = R"(
#version 450 core

in vec3 v_Normal;
in vec3 v_WorldPosition;
in vec2 v_UV;

uniform vec4 u_Color;

uniform sampler2D u_Texture;
uniform int u_UseTexture;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_CameraPosition;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_Emissive;
uniform float u_Exposure;
uniform int u_FogEnabled;
uniform float u_FogDensity;
uniform float u_BloomStrength;
struct PointLight { vec3 position; vec3 color; float intensity; float range; };
struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float range; float innerCos; float outerCos; };
uniform int u_PointLightCount;
uniform int u_SpotLightCount;
uniform PointLight u_PointLights[8];
uniform SpotLight u_SpotLights[4];

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

    float ambient = 0.16 * u_AO;

    vec3 viewDirection = normalize(u_CameraPosition - v_WorldPosition);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float shininess = mix(128.0, 4.0, clamp(u_Roughness, 0.0, 1.0));
    float specularStrength = mix(0.04, 1.0, clamp(u_Metallic, 0.0, 1.0));
    float specular = pow(max(dot(normal, halfDirection), 0.0), shininess) * specularStrength;

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

    vec3 lighting = baseColor.rgb * u_LightColor * brightness + u_LightColor * specular * u_LightIntensity;
    for(int i=0;i<u_PointLightCount;i++) {
        vec3 toLight=u_PointLights[i].position-v_WorldPosition; float d=length(toLight);
        vec3 L=normalize(toLight); float att=pow(clamp(1.0-d/u_PointLights[i].range,0.0,1.0),2.0);
        float ndl=max(dot(normal,L),0.0);
        lighting += baseColor.rgb*u_PointLights[i].color*ndl*u_PointLights[i].intensity*att;
    }
    for(int i=0;i<u_SpotLightCount;i++) {
        vec3 toLight=u_SpotLights[i].position-v_WorldPosition; float d=length(toLight); vec3 L=normalize(toLight);
        float cone=smoothstep(u_SpotLights[i].outerCos,u_SpotLights[i].innerCos,dot(-L,normalize(u_SpotLights[i].direction)));
        float att=pow(clamp(1.0-d/u_SpotLights[i].range,0.0,1.0),2.0);
        lighting += baseColor.rgb*u_SpotLights[i].color*max(dot(normal,L),0.0)*u_SpotLights[i].intensity*att*cone;
    }
    lighting += baseColor.rgb * u_Emissive;

    if (u_BloomStrength > 0.0)
    {
        float luminance = dot(lighting, vec3(0.2126, 0.7152, 0.0722));
        lighting += lighting * max(luminance - 1.0, 0.0) * u_BloomStrength;
    }

    if (u_FogEnabled != 0)
    {
        float distanceToCamera = length(u_CameraPosition - v_WorldPosition);
        float fogFactor = 1.0 - exp(-u_FogDensity * u_FogDensity * distanceToCamera * distanceToCamera);
        vec3 fogColor = vec3(0.58, 0.68, 0.76);
        lighting = mix(lighting, fogColor, clamp(fogFactor, 0.0, 0.92));
    }

    lighting *= u_Exposure;
    // Reinhard tonemap followed by gamma correction.
    lighting = lighting / (lighting + vec3(1.0));
    lighting = pow(lighting, vec3(1.0 / 2.2));
    FragColor = vec4(lighting, baseColor.a);
}
)";

static const char* skyVertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec2 a_Position;
out vec2 v_UV;
void main() { v_UV = a_Position * 0.5 + 0.5; gl_Position = vec4(a_Position, 1.0, 1.0); }
)";
static const char* skyFragmentShaderSource = R"(
#version 450 core
in vec2 v_UV;
out vec4 FragColor;
void main() {
    float h = clamp(v_UV.y, 0.0, 1.0);
    vec3 horizon = vec3(0.64, 0.72, 0.76);
    vec3 zenith = vec3(0.10, 0.27, 0.50);
    vec3 sky = mix(horizon, zenith, smoothstep(0.0, 0.9, h));
    float glow = pow(max(0.0, 1.0 - length(v_UV - vec2(0.74, 0.70)) * 2.5), 5.0);
    sky += vec3(1.0, 0.68, 0.34) * glow * 0.24;
    FragColor = vec4(sky, 1.0);
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

	if (m_SkyVBO) glDeleteBuffers(1, &m_SkyVBO);
	if (m_SkyVAO) glDeleteVertexArrays(1, &m_SkyVAO);
	m_SkyVBO = 0;
	m_SkyVAO = 0;
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
	m_Framebuffer.Resolve();
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
	const Texture2D* texture,
	float metallic,
	float roughness,
	float ambientOcclusion,
	float emissive)
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

	const Vec3 cameraPosition = m_Camera.GetPosition();
	m_Shader.SetVec3(
		"u_CameraPosition",
		cameraPosition.x,
		cameraPosition.y,
		cameraPosition.z
	);
	m_Shader.SetFloat("u_Metallic", metallic);
	m_Shader.SetFloat("u_Roughness", roughness);
	m_Shader.SetFloat("u_AO", ambientOcclusion);
	m_Shader.SetFloat("u_Emissive", emissive);
	m_Shader.SetFloat("u_Exposure", m_RenderSettings.exposure);
	m_Shader.SetInt("u_FogEnabled", m_RenderSettings.fog ? 1 : 0);
	m_Shader.SetFloat("u_FogDensity", m_RenderSettings.fogDensity);
	m_Shader.SetFloat("u_BloomStrength", m_RenderSettings.bloom ? m_RenderSettings.bloomStrength : 0.0f);
	m_Shader.SetInt("u_PointLightCount", m_PointLightCount);
	m_Shader.SetInt("u_SpotLightCount", m_SpotLightCount);
	for(int i=0;i<m_PointLightCount;i++) {
		std::string b="u_PointLights["+std::to_string(i)+"]";
		m_Shader.SetVec3((b+".position").c_str(),m_PointLights[i].position.x,m_PointLights[i].position.y,m_PointLights[i].position.z);
		m_Shader.SetVec3((b+".color").c_str(),m_PointLights[i].color.x,m_PointLights[i].color.y,m_PointLights[i].color.z);
		m_Shader.SetFloat((b+".intensity").c_str(),m_PointLights[i].intensity); m_Shader.SetFloat((b+".range").c_str(),m_PointLights[i].range);
	}
	for(int i=0;i<m_SpotLightCount;i++) {
		std::string b="u_SpotLights["+std::to_string(i)+"]";
		m_Shader.SetVec3((b+".position").c_str(),m_SpotLights[i].position.x,m_SpotLights[i].position.y,m_SpotLights[i].position.z);
		m_Shader.SetVec3((b+".direction").c_str(),m_SpotLights[i].direction.x,m_SpotLights[i].direction.y,m_SpotLights[i].direction.z);
		m_Shader.SetVec3((b+".color").c_str(),m_SpotLights[i].color.x,m_SpotLights[i].color.y,m_SpotLights[i].color.z);
		m_Shader.SetFloat((b+".intensity").c_str(),m_SpotLights[i].intensity);m_Shader.SetFloat((b+".range").c_str(),m_SpotLights[i].range);
		m_Shader.SetFloat((b+".innerCos").c_str(),m_SpotLights[i].innerCos);m_Shader.SetFloat((b+".outerCos").c_str(),m_SpotLights[i].outerCos);
	}

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

void Renderer::DrawSky()
{
	if (m_SkyVAO == 0) return;
	glDisable(GL_DEPTH_TEST);
	m_SkyShader.Bind();
	glBindVertexArray(m_SkyVAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	m_SkyShader.Unbind();
	glEnable(GL_DEPTH_TEST);
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
    m_Camera.SetFarPlane(m_RenderSettings.viewDistance);

    const unsigned int samples = m_RenderSettings.antiAliasing
        ? static_cast<unsigned int>(m_RenderSettings.antiAliasingSamples) : 1u;
    if (m_Framebuffer.GetSamples() != samples)
        m_Framebuffer.SetSamples(samples);

    if (samples > 1) glEnable(GL_MULTISAMPLE);
    else glDisable(GL_MULTISAMPLE);
}

const RenderSettings& Renderer::GetRenderSettings() const
{
    return m_RenderSettings;
}
