#include "Renderer.h"
#include "../Platform/SDL/Window.h"
#include "PrimitiveMesh.h"
#include "ModelLoader.h"
#include "Texture2D.h"
#include "../Scene/Components/TextureComponent.h"

#include "../Core/Logger.h"
#include <iostream>
#include <algorithm>
#include <cmath>

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
out vec4 v_LightSpacePosition;
uniform mat4 u_LightSpaceMatrix;

void main()
{
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    v_Normal = normalize(normalMatrix * a_Normal);
    v_WorldPosition = vec3(u_Model * vec4(a_Position, 1.0));
    v_UV = a_UV;
    v_LightSpacePosition = u_LightSpaceMatrix * vec4(v_WorldPosition, 1.0);

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
in vec4 v_LightSpacePosition;

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
uniform int u_FogEnabled;
uniform float u_FogDensity;
uniform float u_ViewDistance;
uniform sampler2D u_ShadowMap;
uniform int u_ShadowsEnabled;
uniform int u_ShadowPCFRadius;
struct PointLight { vec3 position; vec3 color; float intensity; float range; };
struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float range; float innerCos; float outerCos; };
uniform int u_PointLightCount;
uniform int u_SpotLightCount;
uniform PointLight u_PointLights[8];
uniform SpotLight u_SpotLights[4];

out vec4 FragColor;

float CalculateShadow(vec4 lightSpacePosition, vec3 normal, vec3 lightDirection)
{
    if (u_ShadowsEnabled == 0)
        return 0.0;

    vec3 projected = lightSpacePosition.xyz / max(lightSpacePosition.w, 0.0001);
    projected = projected * 0.5 + 0.5;
    if (projected.z <= 0.0 || projected.z >= 1.0 ||
        projected.x <= 0.0 || projected.x >= 1.0 ||
        projected.y <= 0.0 || projected.y >= 1.0)
        return 0.0;

    float bias = max(0.0007 * (1.0 - dot(normal, lightDirection)), 0.00018);
    vec2 texel = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    float shadow = 0.0;
    int radius = clamp(u_ShadowPCFRadius, 1, 3);
    float samples = 0.0;
    for (int x = -3; x <= 3; ++x)
    {
        for (int y = -3; y <= 3; ++y)
        {
            if (abs(x) > radius || abs(y) > radius)
                continue;
            float closest = texture(u_ShadowMap, projected.xy + vec2(x, y) * texel).r;
            shadow += projected.z - bias > closest ? 1.0 : 0.0;
            samples += 1.0;
        }
    }
    return shadow / max(samples, 1.0);
}

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

    // Keep the original renderer's neutral 0.25 ambient baseline.
    float ambient = 0.25 * u_AO;

    vec3 viewDirection = normalize(u_CameraPosition - v_WorldPosition);
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float shininess = mix(128.0, 4.0, clamp(u_Roughness, 0.0, 1.0));
    float specularStrength = mix(0.04, 1.0, clamp(u_Metallic, 0.0, 1.0));
    float specular = pow(max(dot(normal, halfDirection), 0.0), shininess) * specularStrength;
    float fresnel = pow(1.0 - max(dot(normal, viewDirection), 0.0), 5.0);
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

    vec3 dielectricF0 = vec3(0.04);
    vec3 specularColor = mix(dielectricF0, baseColor.rgb, clamp(u_Metallic, 0.0, 1.0));
    vec3 diffuseColor = baseColor.rgb * (1.0 - clamp(u_Metallic, 0.0, 1.0));
    float shadow = CalculateShadow(v_LightSpacePosition, normal, lightDirection);
    vec3 ambientLighting = diffuseColor * u_LightColor * ambient;
    vec3 directLighting =
        diffuseColor * u_LightColor * diffuse * u_LightIntensity +
        specularColor * u_LightColor * specular * (1.0 + fresnel) * u_LightIntensity;
    vec3 lighting = ambientLighting + directLighting * (1.0 - shadow);
    for(int i=0;i<u_PointLightCount;i++) {
        vec3 toLight=u_PointLights[i].position-v_WorldPosition; float d=length(toLight);
        vec3 L=normalize(toLight); float att=pow(clamp(1.0-d/u_PointLights[i].range,0.0,1.0),2.0);
        float ndl=max(dot(normal,L),0.0);
        vec3 H=normalize(L+viewDirection);
        float localSpec=pow(max(dot(normal,H),0.0),shininess);
        vec3 localDiffuse=diffuseColor*ndl;
        vec3 localSpecular=specularColor*localSpec;
        lighting += (localDiffuse+localSpecular)*u_PointLights[i].color*u_PointLights[i].intensity*att;
    }
    for(int i=0;i<u_SpotLightCount;i++) {
        vec3 toLight=u_SpotLights[i].position-v_WorldPosition; float d=length(toLight); vec3 L=normalize(toLight);
        float cone=smoothstep(u_SpotLights[i].outerCos,u_SpotLights[i].innerCos,dot(-L,normalize(u_SpotLights[i].direction)));
        float att=pow(clamp(1.0-d/u_SpotLights[i].range,0.0,1.0),2.0);
        float ndl=max(dot(normal,L),0.0);
        vec3 H=normalize(L+viewDirection);
        float localSpec=pow(max(dot(normal,H),0.0),shininess);
        lighting += (diffuseColor*ndl+specularColor*localSpec)*u_SpotLights[i].color*u_SpotLights[i].intensity*att*cone;
    }
    lighting += baseColor.rgb * u_Emissive;

    if (u_FogEnabled != 0)
    {
        float distanceToCamera = length(u_CameraPosition - v_WorldPosition);
        // Gentle linear-distance fog. This deliberately avoids the old
        // exponential wash that tinted most of the scene blue-gray.
        float fogStart = max(25.0, 0.55 * u_ViewDistance);
        float fogEnd = max(fogStart + 1.0, u_ViewDistance);
        float fogFactor = smoothstep(fogStart, fogEnd, distanceToCamera);
        vec3 fogColor = vec3(0.64, 0.72, 0.76);
        lighting = mix(lighting, fogColor, fogFactor * clamp(u_FogDensity * 40.0, 0.0, 1.0));
    }

    // Materials output linear HDR. Exposure and display encoding happen once
    // in the final post-process pass.
    FragColor = vec4(max(lighting, vec3(0.0)), baseColor.a);
}
)";

static const char* shadowVertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec3 a_Position;
uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;
void main()
{
    gl_Position = u_LightSpaceMatrix * u_Model * vec4(a_Position, 1.0);
}
)";

static const char* shadowFragmentShaderSource = R"(
#version 450 core
void main() {}
)";

static const char* postVertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec2 a_Position;
out vec2 v_UV;
void main() { v_UV = a_Position * 0.5 + 0.5; gl_Position = vec4(a_Position, 0.0, 1.0); }
)";

static const char* postFragmentShaderSource = R"(
#version 450 core
in vec2 v_UV;
out vec4 FragColor;
uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform float u_Exposure;
uniform float u_BloomStrength;

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr = max(texture(u_Scene, v_UV).rgb, vec3(0.0));
    vec3 bloom = max(texture(u_Bloom, v_UV).rgb, vec3(0.0));
    hdr += bloom * u_BloomStrength;
    vec3 mapped = ACESFilm(hdr * u_Exposure);
    // Mild display encoding preserves contrast while lifting dark material detail.
    mapped = pow(mapped, vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
)";

static const char* bloomExtractFragmentShaderSource = R"(
#version 450 core
in vec2 v_UV;
out vec4 FragColor;
uniform sampler2D u_Scene;
void main()
{
    vec3 color = max(texture(u_Scene, v_UV).rgb, vec3(0.0));
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float knee = smoothstep(0.85, 1.65, luminance);
    FragColor = vec4(color * knee, 1.0);
}
)";

static const char* bloomBlurFragmentShaderSource = R"(
#version 450 core
in vec2 v_UV;
out vec4 FragColor;
uniform sampler2D u_Image;
uniform int u_Horizontal;
void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_Image, 0));
    vec3 result = texture(u_Image, v_UV).rgb * 0.227027;
    vec2 axis = u_Horizontal != 0 ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    result += texture(u_Image, v_UV + axis * 1.0).rgb * 0.1945946;
    result += texture(u_Image, v_UV - axis * 1.0).rgb * 0.1945946;
    result += texture(u_Image, v_UV + axis * 2.0).rgb * 0.1216216;
    result += texture(u_Image, v_UV - axis * 2.0).rgb * 0.1216216;
    result += texture(u_Image, v_UV + axis * 3.0).rgb * 0.054054;
    result += texture(u_Image, v_UV - axis * 3.0).rgb * 0.054054;
    result += texture(u_Image, v_UV + axis * 4.0).rgb * 0.016216;
    result += texture(u_Image, v_UV - axis * 4.0).rgb * 0.016216;
    FragColor = vec4(result, 1.0);
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
    vec3 horizon = vec3(0.48, 0.58, 0.66);
    vec3 zenith = vec3(0.035, 0.12, 0.25);
    vec3 sky = mix(horizon, zenith, smoothstep(0.0, 0.92, h));
    float sunDistance = length((v_UV - vec2(0.76, 0.68)) * vec2(1.0, 1.35));
    float sunDisc = pow(max(0.0, 1.0 - sunDistance * 8.0), 18.0);
    float sunGlow = pow(max(0.0, 1.0 - sunDistance * 1.8), 5.0);
    sky += vec3(1.0, 0.72, 0.42) * (sunGlow * 0.34 + sunDisc * 1.8);
    float horizonHaze = 1.0 - smoothstep(0.0, 0.34, abs(v_UV.y - 0.32));
    sky += vec3(0.18, 0.20, 0.21) * horizonHaze * 0.18;
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

    if (!m_ShadowShader.Initialize(shadowVertexShaderSource, shadowFragmentShaderSource))
    {
        Logger::Error("Failed to initialize shadow shader.");
        return false;
    }

    if (!CreateShadowTarget())
    {
        Logger::Error("Failed to initialize directional shadow map.");
        return false;
    }

    if (!m_PostShader.Initialize(postVertexShaderSource, postFragmentShaderSource) ||
        !m_BloomExtractShader.Initialize(postVertexShaderSource, bloomExtractFragmentShaderSource) ||
        !m_BloomBlurShader.Initialize(postVertexShaderSource, bloomBlurFragmentShaderSource))
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

Mesh* Renderer::GetPrimitiveMesh(PrimitiveType primitive)
{
    switch (primitive)
    {
    case PrimitiveType::Cube: return m_CubeMesh.get();
    case PrimitiveType::Plane: return m_PlaneMesh.get();
    case PrimitiveType::Sphere: return m_SphereMesh.get();
    case PrimitiveType::Cylinder: return m_CylinderMesh.get();
    default: return nullptr;
    }
}

Mesh* Renderer::GetModelMesh(const std::string& modelPath)
{
    if (modelPath.empty()) return nullptr;
    auto it = m_ModelCache.find(modelPath);
    if (it != m_ModelCache.end()) return it->second.get();

    std::unique_ptr<Mesh> loaded = ModelLoader::LoadOBJ(modelPath);
    if (!loaded) return nullptr;
    Mesh* result = loaded.get();
    m_ModelCache.emplace(modelPath, std::move(loaded));
    return result;
}

void Renderer::DrawMeshInternal(
    Mesh* mesh, const Transform& transform,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive)
{
    if (!mesh) return;	Mat4 model =
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
    m_Shader.SetMat4("u_LightSpaceMatrix", m_LightSpaceMatrix);
    m_Shader.SetInt("u_ShadowsEnabled", (m_RenderSettings.shadows && m_ShadowMapReady) ? 1 : 0);
    m_Shader.SetInt("u_ShadowPCFRadius", std::clamp(m_RenderSettings.shadowQuality + 1, 1, 3));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_ShadowDepthTexture);
    m_Shader.SetInt("u_ShadowMap", 1);
    glActiveTexture(GL_TEXTURE0);

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
	m_Shader.SetInt("u_FogEnabled", m_RenderSettings.fog ? 1 : 0);
	m_Shader.SetFloat("u_FogDensity", m_RenderSettings.fogDensity);
    m_Shader.SetFloat("u_ViewDistance", m_RenderSettings.viewDistance);
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
    DrawMeshInternal(GetPrimitiveMesh(primitive), transform, red, green, blue, alpha,
        texture, metallic, roughness, ambientOcclusion, emissive);
}

void Renderer::DrawModel(
    const Transform& transform, const std::string& modelPath,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive)
{
    DrawMeshInternal(GetModelMesh(modelPath), transform, red, green, blue, alpha,
        texture, metallic, roughness, ambientOcclusion, emissive);
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
    const unsigned int desiredShadowSize =
        m_RenderSettings.shadowQuality <= 0 ? 1024u :
        m_RenderSettings.shadowQuality == 1 ? 2048u : 4096u;
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

    const Mat4 lightView = Mat4::LookAt(lightPosition, center, up);
    const float extent = distance * 0.62f;
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

bool Renderer::CreatePostProcessTarget()
{
    if (m_ViewportWidth == 0 || m_ViewportHeight == 0) return false;
    glGenFramebuffers(1, &m_PostFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_PostFramebuffer);
    glGenTextures(1, &m_PostColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_PostColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_ViewportWidth, m_ViewportHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Renderer::DestroyPostProcessTarget()
{
    if (m_BloomTexture[0] || m_BloomTexture[1]) glDeleteTextures(2, m_BloomTexture);
    if (m_BloomFramebuffer[0] || m_BloomFramebuffer[1]) glDeleteFramebuffers(2, m_BloomFramebuffer);
    m_BloomTexture[0] = m_BloomTexture[1] = 0;
    m_BloomFramebuffer[0] = m_BloomFramebuffer[1] = 0;
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
    m_PostShader.SetFloat("u_BloomStrength", bloomTexture ? m_RenderSettings.bloomStrength : 0.0f);
    m_PostShader.SetFloat("u_Exposure", m_RenderSettings.exposure);
    glBindVertexArray(m_PostVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_PostShader.Unbind();
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
