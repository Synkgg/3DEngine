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
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;
uniform int u_UseNormalMap;
uniform int u_UseMetallicMap;
uniform int u_UseRoughnessMap;
uniform int u_UseAOMap;
uniform int u_UseEmissiveMap;
uniform int u_FogEnabled;
uniform float u_FogDensity;
uniform float u_ViewDistance;
uniform sampler2D u_ShadowMap;
uniform int u_ShadowsEnabled;
uniform int u_ShadowPCFRadius;
uniform float u_IndirectLightStrength;
uniform float u_ReflectionStrength;
uniform float u_ContactShadowStrength;

struct PointLight { vec3 position; vec3 color; float intensity; float range; };
struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float range; float innerCos; float outerCos; };
uniform int u_PointLightCount;
uniform int u_SpotLightCount;
uniform PointLight u_PointLights[8];
uniform SpotLight u_SpotLights[4];

out vec4 FragColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(N, H), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denom = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.000001);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
        pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float CalculateShadow(vec4 lightSpacePosition, vec3 N, vec3 L)
{
    if (u_ShadowsEnabled == 0) return 0.0;
    vec3 p = lightSpacePosition.xyz / max(lightSpacePosition.w, 0.0001);
    p = p * 0.5 + 0.5;
    if (p.z <= 0.0 || p.z >= 1.0 || p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0)
        return 0.0;

    float nDotL = max(dot(N, L), 0.0);
    float bias = max(0.00065 * (1.0 - nDotL), 0.00032);
    vec2 texel = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    int radius = clamp(u_ShadowPCFRadius, 1, 3);
    float shadow = 0.0;
    float weight = 0.0;
    for (int x = -3; x <= 3; ++x)
    {
        for (int y = -3; y <= 3; ++y)
        {
            if (abs(x) > radius || abs(y) > radius) continue;
            float w = 1.0 / (1.0 + 0.32 * float(x*x + y*y));
            float closest = texture(u_ShadowMap, p.xy + vec2(x,y) * texel).r;
            shadow += (p.z - bias > closest ? 1.0 : 0.0) * w;
            weight += w;
        }
    }
    float result = clamp(shadow / max(weight, 0.0001), 0.0, 0.88);
    // Grazing surfaces need less aggressive shadowing to avoid large black
    // bands while contact-facing surfaces retain full depth.
    result *= mix(0.72, 1.0, nDotL);
    return clamp(result * u_ContactShadowStrength, 0.0, 0.88);
}

vec3 EvaluateBRDF(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 H = normalize(V + L);
    float nDotL = max(dot(N, L), 0.0);
    float nDotV = max(dot(N, V), 0.0);
    if (nDotL <= 0.0 || nDotV <= 0.0) return vec3(0.0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 specular = (NDF * G * F) / max(4.0 * nDotV * nDotL, 0.001);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * nDotL;
}

void main()
{
    vec4 baseColor = u_Color;
    if (u_UseTexture != 0)
        baseColor *= texture(u_Texture, v_UV);

    // Scene colors and texture samples are already authored in the engine's
    // working color space. Converting them with pow(2.2) here crushes the
    // deliberately dark/saturated palette used by existing scenes.
    vec3 albedo = max(baseColor.rgb, vec3(0.0));
    float metallic = clamp(u_UseMetallicMap != 0 ? texture(u_MetallicMap, v_UV).r : u_Metallic, 0.0, 1.0);
    float roughness = clamp(u_UseRoughnessMap != 0 ? texture(u_RoughnessMap, v_UV).r : u_Roughness, 0.045, 1.0);
    float ao = clamp(u_UseAOMap != 0 ? texture(u_AOMap, v_UV).r : u_AO, 0.0, 1.0);

    vec3 N = normalize(v_Normal);
    if (u_UseNormalMap != 0)
    {
        // Derivative-built TBN keeps normal mapping compatible with existing
        // meshes without requiring tangent attributes in the vertex format.
        vec3 dp1 = dFdx(v_WorldPosition), dp2 = dFdy(v_WorldPosition);
        vec2 duv1 = dFdx(v_UV), duv2 = dFdy(v_UV);
        vec3 T = normalize(dp1 * duv2.y - dp2 * duv1.y);
        vec3 B = normalize(-dp1 * duv2.x + dp2 * duv1.x);
        vec3 mapN = texture(u_NormalMap, v_UV).xyz * 2.0 - 1.0;
        N = normalize(mat3(T, B, N) * mapN);
    }
    vec3 V = normalize(u_CameraPosition - v_WorldPosition);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 sunL = normalize(-u_LightDirection);
    vec3 sunRadiance = u_LightColor * max(u_LightIntensity, 0.0);
    float shadow = CalculateShadow(v_LightSpacePosition, N, sunL);
    vec3 lighting = EvaluateBRDF(N, V, sunL, sunRadiance, albedo, metallic, roughness, F0) * (1.0 - shadow);

    for (int i = 0; i < u_PointLightCount; ++i)
    {
        vec3 toLight = u_PointLights[i].position - v_WorldPosition;
        float distance = length(toLight);
        if (distance < u_PointLights[i].range && distance > 0.0001)
        {
            vec3 L = toLight / distance;
            float rangeFalloff = clamp(1.0 - distance / max(u_PointLights[i].range, 0.001), 0.0, 1.0);
            float attenuation = rangeFalloff * rangeFalloff / max(1.0 + 0.045 * distance * distance, 1.0);
            vec3 radiance = u_PointLights[i].color * u_PointLights[i].intensity * attenuation;
            lighting += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness, F0);
        }
    }

    for (int i = 0; i < u_SpotLightCount; ++i)
    {
        vec3 toLight = u_SpotLights[i].position - v_WorldPosition;
        float distance = length(toLight);
        if (distance < u_SpotLights[i].range && distance > 0.0001)
        {
            vec3 L = toLight / distance;
            float cone = smoothstep(u_SpotLights[i].outerCos, u_SpotLights[i].innerCos,
                                    dot(-L, normalize(u_SpotLights[i].direction)));
            float rangeFalloff = clamp(1.0 - distance / max(u_SpotLights[i].range, 0.001), 0.0, 1.0);
            float attenuation = rangeFalloff * rangeFalloff / max(1.0 + 0.045 * distance * distance, 1.0);
            vec3 radiance = u_SpotLights[i].color * u_SpotLights[i].intensity * attenuation * cone;
            lighting += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness, F0);
        }
    }

    // Hemispherical environment integration. It acts as a stable probe for
    // scenes that do not yet provide an authored HDR cubemap.
    float skyWeight = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 groundIrradiance = vec3(0.060, 0.050, 0.040);
    vec3 skyIrradiance = vec3(0.20, 0.32, 0.54);
    vec3 hemiIrradiance = mix(groundIrradiance, skyIrradiance, skyWeight);
    float sunBounce = max(dot(N, -sunL), 0.0);
    hemiIrradiance += u_LightColor * u_LightIntensity * sunBounce * 0.025;

    // Approximate first-bounce energy from local lights. This is deliberately
    // low frequency so it reads as indirect illumination rather than a second
    // direct-light term.
    vec3 localBounce = vec3(0.0);
    for (int i = 0; i < u_PointLightCount; ++i)
    {
        float d = length(u_PointLights[i].position - v_WorldPosition);
        float influence = clamp(1.0 - d / max(u_PointLights[i].range * 1.35, 0.001), 0.0, 1.0);
        localBounce += u_PointLights[i].color * u_PointLights[i].intensity * influence * influence * 0.018;
    }
    for (int i = 0; i < u_SpotLightCount; ++i)
    {
        float d = length(u_SpotLights[i].position - v_WorldPosition);
        float influence = clamp(1.0 - d / max(u_SpotLights[i].range * 1.25, 0.001), 0.0, 1.0);
        localBounce += u_SpotLights[i].color * u_SpotLights[i].intensity * influence * influence * 0.012;
    }

    vec3 Fenv = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 envDiffuse = albedo * (1.0 - metallic) *
        max(hemiIrradiance + localBounce, vec3(0.085, 0.095, 0.11));

    vec3 R = reflect(-V, N);
    float sunReflection = pow(max(dot(R, sunL), 0.0), mix(8.0, 768.0, 1.0 - roughness));
    float reflectionHeight = clamp(R.y * 0.5 + 0.5, 0.0, 1.0);
    float reflectionHorizon = exp(-abs(R.y) * 6.0);
    vec3 horizonReflection = mix(groundIrradiance, skyIrradiance, reflectionHeight);
    horizonReflection += vec3(0.16, 0.11, 0.065) * reflectionHorizon;
    vec3 envSpecular = Fenv * horizonReflection * mix(0.12, 1.0, 1.0 - roughness);
    envSpecular += Fenv * u_LightColor * sunReflection * u_LightIntensity *
                   (1.0 - roughness) * 1.8;
    lighting += (envDiffuse * u_IndirectLightStrength +
                 envSpecular * u_ReflectionStrength) * ao;

    float emissiveAmount = u_UseEmissiveMap != 0 ? texture(u_EmissiveMap, v_UV).r : max(u_Emissive, 0.0);
    lighting += albedo * emissiveAmount * 2.0;

    if (u_FogEnabled != 0)
    {
        float distanceToCamera = length(u_CameraPosition - v_WorldPosition);
        float fogStart = max(20.0, 0.38 * u_ViewDistance);
        float fogEnd = max(fogStart + 1.0, u_ViewDistance);
        float distanceFog = smoothstep(fogStart, fogEnd, distanceToCamera);
        float heightFog = exp(-max(v_WorldPosition.y, 0.0) * 0.025);
        float fogAmount = clamp(distanceFog * (0.35 + u_FogDensity * 24.0) * heightFog, 0.0, 0.92);
        vec3 fogColor = vec3(0.42, 0.55, 0.70);
        lighting = mix(lighting, fogColor, fogAmount);
    }

    FragColor = vec4(max(lighting, vec3(0.0)), baseColor.a);
}
)";

static const char* modelPreviewVertexShaderSource = R"(
#version 450 core
layout(location=0) in vec3 a_Position;
layout(location=1) in vec3 a_Normal;
uniform mat4 u_MVP;
out vec3 v_Normal;
void main(){ v_Normal=a_Normal; gl_Position=u_MVP*vec4(a_Position,1.0); }
)";
static const char* modelPreviewFragmentShaderSource = R"(
#version 450 core
in vec3 v_Normal;
out vec4 FragColor;
void main(){ vec3 n=normalize(v_Normal); float d=max(dot(n,normalize(vec3(-0.45,0.75,0.55))),0.0); vec3 b=vec3(0.58,0.62,0.68); FragColor=vec4(b*(0.30+d*0.78),1.0); }
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
uniform sampler2D u_Depth;
 uniform float u_Exposure;
uniform float u_BloomStrength;
uniform vec3 u_CameraForward;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;
uniform vec3 u_SunDirection;
uniform vec3 u_SunColor;
uniform float u_SunIntensity;
uniform float u_TanHalfFov;
uniform float u_Aspect;
 
float LinearizeDepth(float d)
{
    const float nearPlane = 0.1;
    const float farPlane = 1000.0;
    float z = d * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / max(farPlane + nearPlane - z * (farPlane - nearPlane), 0.0001);
}

float ScreenAO(vec2 uv)
{
    float centerRaw = texture(u_Depth, uv).r;
    if (centerRaw >= 0.99999) return 1.0;
    float center = LinearizeDepth(centerRaw);
    vec2 texel = 1.0 / vec2(textureSize(u_Depth, 0));
    float occ = 0.0;
    float samples = 0.0;
    const vec2 dirs[8] = vec2[8](
        vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),
        vec2(0.707,0.707),vec2(-0.707,0.707),vec2(0.707,-0.707),vec2(-0.707,-0.707));
    for(int ring=1; ring<=2; ++ring)
    {
        float radius = float(ring) * 1.35;
        for(int i=0;i<8;++i)
        {
            float sampleRaw = texture(u_Depth, uv + dirs[i] * texel * radius).r;
            if(sampleRaw >= 0.99999) continue; // never darken an object silhouette against sky
            float sd = LinearizeDepth(sampleRaw);
            float delta = center - sd;
            float thickness = max(0.08, center * 0.012);
            float range = 1.0 - smoothstep(thickness, thickness * 5.0, abs(delta));
            occ += smoothstep(0.015, thickness, delta) * range;
            samples += 1.0;
        }
    }
    if(samples < 1.0) return 1.0;
    return clamp(1.0 - (occ / samples) * 0.42, 0.82, 1.0);
}

vec3 ACESFilm(vec3 x)
{
    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
}

void main()
{
    vec3 hdr = max(texture(u_Scene, v_UV).rgb, vec3(0.0));
    vec3 bloom = max(texture(u_Bloom, v_UV).rgb, vec3(0.0));
    hdr += bloom * u_BloomStrength;

    // Depth-aware screen-space ambient occlusion adds contact depth at corners
    // and intersections without changing the authored material colors.
    float ao = ScreenAO(v_UV);
    hdr *= mix(1.0, ao, 0.34);

    // Atmospheric aerial perspective and forward scattering. This is derived
    // from depth and the procedural sun, so it works in every existing scene
    // without requiring authored fog volumes.
    float rawDepth = texture(u_Depth, v_UV).r;
    if (rawDepth < 0.99999)
    {
        float distanceToSurface = LinearizeDepth(rawDepth);
        vec2 ndc = v_UV * 2.0 - 1.0;
        vec3 ray = normalize(u_CameraForward +
            u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
            u_CameraUp * (ndc.y * u_TanHalfFov));
        vec3 sunDir = normalize(-u_SunDirection);
        float phase = 0.035 + 0.24 * pow(max(dot(ray, sunDir), 0.0), 5.0);
        float aerial = 1.0 - exp(-distanceToSurface * 0.00115);
        vec3 atmosphere = mix(vec3(0.18,0.28,0.42), max(u_SunColor, vec3(0.65,0.48,0.30)),
                              pow(max(dot(ray, sunDir),0.0), 8.0));
        hdr = mix(hdr, atmosphere * (0.7 + phase * max(u_SunIntensity,0.25)),
                  clamp(aerial * 0.42, 0.0, 0.42));
    }

    // Filmic exposure and tone mapping.
    vec3 mapped = ACESFilm(hdr * max(u_Exposure, 0.001));

    // Subtle cinematic color grade: preserve saturation in highlights while
    // avoiding the flat gray look of a plain gamma-only output.
    float luma = dot(mapped, vec3(0.2126,0.7152,0.0722));
    mapped = mix(vec3(luma), mapped, 1.10);
    mapped = (mapped - 0.5) * 1.055 + 0.5;

    // Gentle vignette anchors the image without crushing the corners.
    vec2 q = v_UV * (1.0 - v_UV.yx);
    float vignette = pow(clamp(16.0 * q.x * q.y, 0.0, 1.0), 0.08);
    mapped *= mix(0.88, 1.0, vignette);

    mapped = pow(clamp(mapped,0.0,1.0), vec3(1.0/2.2));

    // MSAA handles geometry edges. Avoid depth-edge color filtering here:
    // sampling across foreground/background silhouettes creates visible halos.
    FragColor = vec4(mapped,1.0);
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
    float knee = smoothstep(0.62, 1.30, luminance);
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

uniform vec3 u_CameraForward;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;
uniform vec3 u_SunDirection;
uniform vec3 u_SunColor;
uniform float u_SunIntensity;
uniform float u_TanHalfFov;
uniform float u_Aspect;

void main()
{
    vec2 ndc = v_UV * 2.0 - 1.0;
    vec3 ray = normalize(u_CameraForward +
                         u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
                         u_CameraUp * (ndc.y * u_TanHalfFov));

    float height = clamp(ray.y * 0.5 + 0.5, 0.0, 1.0);
    float horizon = exp(-abs(ray.y) * 7.0);
    vec3 zenith = vec3(0.018, 0.070, 0.19);
    vec3 midSky = vec3(0.12, 0.30, 0.58);
    vec3 horizonColor = vec3(0.62, 0.72, 0.78);
    vec3 sky = mix(horizonColor, midSky, smoothstep(0.48, 0.72, height));
    sky = mix(sky, zenith, smoothstep(0.70, 1.0, height));
    sky += vec3(0.12, 0.09, 0.055) * horizon;

    vec3 sunDir = normalize(-u_SunDirection);
    float sunDot = max(dot(ray, sunDir), 0.0);
    float sunDisk = smoothstep(0.99972, 0.99993, sunDot);
    float sunGlow = pow(sunDot, 64.0);
    float wideGlow = pow(sunDot, 8.0);
    vec3 warmSun = max(u_SunColor, vec3(0.75, 0.55, 0.32));
    sky += warmSun * (sunDisk * 7.0 + sunGlow * 0.85 + wideGlow * 0.055) *
           max(u_SunIntensity, 0.25);

    // A subtle opposite-horizon haze keeps the atmosphere from reading as a flat gradient.
    float forwardScatter = pow(max(dot(ray, sunDir), 0.0), 3.0);
    sky += vec3(0.16, 0.10, 0.055) * forwardScatter * horizon * 1.5;

    FragColor = vec4(max(sky, vec3(0.0)), 1.0);
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

bool Renderer::EnsureModelPreviewTarget(unsigned int width, unsigned int height)
{
    width=std::max(1u,width); height=std::max(1u,height);
    if(m_ModelPreviewFramebuffer && m_ModelPreviewWidth==width && m_ModelPreviewHeight==height) return true;
    DestroyModelPreviewTarget();
    glGenFramebuffers(1,&m_ModelPreviewFramebuffer); glBindFramebuffer(GL_FRAMEBUFFER,m_ModelPreviewFramebuffer);
    glGenTextures(1,&m_ModelPreviewTexture); glBindTexture(GL_TEXTURE_2D,m_ModelPreviewTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,(GLsizei)width,(GLsizei)height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,m_ModelPreviewTexture,0);
    glGenRenderbuffers(1,&m_ModelPreviewDepth); glBindRenderbuffer(GL_RENDERBUFFER,m_ModelPreviewDepth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,(GLsizei)width,(GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,m_ModelPreviewDepth);
    bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE; glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(!ok){DestroyModelPreviewTarget();return false;} m_ModelPreviewWidth=width;m_ModelPreviewHeight=height;return true;
}
void Renderer::DestroyModelPreviewTarget()
{
    if(m_ModelPreviewDepth)glDeleteRenderbuffers(1,&m_ModelPreviewDepth);
    if(m_ModelPreviewTexture)glDeleteTextures(1,&m_ModelPreviewTexture);
    if(m_ModelPreviewFramebuffer)glDeleteFramebuffers(1,&m_ModelPreviewFramebuffer);
    m_ModelPreviewDepth=m_ModelPreviewTexture=m_ModelPreviewFramebuffer=0;m_ModelPreviewWidth=m_ModelPreviewHeight=0;
}
unsigned int Renderer::RenderModelPreview(const std::string& path,unsigned int width,unsigned int height)
{
    Mesh* mesh=GetModelMesh(path);
    if(!mesh||mesh->GetVertices().empty()) return 0;

    width=std::max(1u,width); height=std::max(1u,height);
    const std::string cacheKey=path+"#"+std::to_string(width)+"x"+std::to_string(height);
    auto cached=m_ModelPreviewCache.find(cacheKey);
    if(cached!=m_ModelPreviewCache.end()) return cached->second.texture;

    ModelPreviewTexture target;
    target.width=width; target.height=height;
    glGenFramebuffers(1,&target.framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);
    glGenTextures(1,&target.texture); glBindTexture(GL_TEXTURE_2D,target.texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,(GLsizei)width,(GLsizei)height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target.texture,0);
    glGenRenderbuffers(1,&target.depth); glBindRenderbuffer(GL_RENDERBUFFER,target.depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,(GLsizei)width,(GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,target.depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        if(target.depth)glDeleteRenderbuffers(1,&target.depth);
        if(target.texture)glDeleteTextures(1,&target.texture);
        if(target.framebuffer)glDeleteFramebuffers(1,&target.framebuffer);
        return 0;
    }

    const auto& v=mesh->GetVertices(); Vec3 mn(v[0].position[0],v[0].position[1],v[0].position[2]),mx=mn;
    for(const Vertex& x:v){mn.x=std::min(mn.x,x.position[0]);mn.y=std::min(mn.y,x.position[1]);mn.z=std::min(mn.z,x.position[2]);mx.x=std::max(mx.x,x.position[0]);mx.y=std::max(mx.y,x.position[1]);mx.z=std::max(mx.z,x.position[2]);}
    Vec3 center((mn.x+mx.x)*.5f,(mn.y+mx.y)*.5f,(mn.z+mx.z)*.5f);
    float radius=std::max(.1f,std::max(mx.x-mn.x,std::max(mx.y-mn.y,mx.z-mn.z))*.5f),dist=radius*3.1f;
    Mat4 view=Mat4::LookAt(Vec3(center.x+dist*.78f,center.y+dist*.58f,center.z+dist),center,Vec3(0,1,0));
    Mat4 proj=Mat4::Perspective(45.f*.0174532925f,(float)width/(float)height,.01f,dist+radius*4.f);
    Mat4 mvp=proj*view;

    GLint oldFbo=0,oldVp[4]{};glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&oldFbo);glGetIntegerv(GL_VIEWPORT,oldVp);
    glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glViewport(0,0,(GLsizei)width,(GLsizei)height);glEnable(GL_DEPTH_TEST);
    glClearColor(.075f,.082f,.095f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    m_ModelPreviewShader.Bind();m_ModelPreviewShader.SetMat4("u_MVP",mvp);mesh->Bind();
    glDrawElements(GL_TRIANGLES,(GLsizei)mesh->GetIndexCount(),GL_UNSIGNED_INT,nullptr);mesh->Unbind();m_ModelPreviewShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)oldFbo);glViewport(oldVp[0],oldVp[1],oldVp[2],oldVp[3]);

    const unsigned int texture=target.texture;
    m_ModelPreviewCache.emplace(cacheKey,target);
    return texture;
}

void Renderer::DestroyModelPreviewCache()
{
    for(auto& pair:m_ModelPreviewCache)
    {
        ModelPreviewTexture& target=pair.second;
        if(target.depth)glDeleteRenderbuffers(1,&target.depth);
        if(target.texture)glDeleteTextures(1,&target.texture);
        if(target.framebuffer)glDeleteFramebuffers(1,&target.framebuffer);
    }
    m_ModelPreviewCache.clear();
}

void Renderer::DrawMeshInternal(
    Mesh* mesh, const Transform& transform,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive, const Texture2D* normalMap,
    const Texture2D* metallicMap, const Texture2D* roughnessMap,
    const Texture2D* aoMap, const Texture2D* emissiveMap)
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
    m_Shader.SetFloat("u_IndirectLightStrength", m_RenderSettings.indirectLightStrength);
    m_Shader.SetFloat("u_ReflectionStrength", m_RenderSettings.reflectionStrength);
    m_Shader.SetFloat("u_ContactShadowStrength", m_RenderSettings.contactShadowStrength);
    const Texture2D* maps[5] = { normalMap, metallicMap, roughnessMap, aoMap, emissiveMap };
    const char* samplers[5] = { "u_NormalMap", "u_MetallicMap", "u_RoughnessMap", "u_AOMap", "u_EmissiveMap" };
    const char* toggles[5] = { "u_UseNormalMap", "u_UseMetallicMap", "u_UseRoughnessMap", "u_UseAOMap", "u_UseEmissiveMap" };
    for (int mapIndex = 0; mapIndex < 5; ++mapIndex)
    {
        const bool valid = maps[mapIndex] && maps[mapIndex]->IsLoaded();
        m_Shader.SetInt(toggles[mapIndex], valid ? 1 : 0);
        if (valid)
        {
            maps[mapIndex]->Bind(2 + mapIndex);
            m_Shader.SetInt(samplers[mapIndex], 2 + mapIndex);
        }
    }
    glActiveTexture(GL_TEXTURE0);
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
	float emissive,
    const Texture2D* normalMap, const Texture2D* metallicMap,
    const Texture2D* roughnessMap, const Texture2D* aoMap,
    const Texture2D* emissiveMap)
{
    DrawMeshInternal(GetPrimitiveMesh(primitive), transform, red, green, blue, alpha,
        texture, metallic, roughness, ambientOcclusion, emissive,
        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap);
}

void Renderer::DrawModel(
    const Transform& transform, const std::string& modelPath,
    float red, float green, float blue, float alpha,
    const Texture2D* texture, float metallic, float roughness,
    float ambientOcclusion, float emissive,
    const Texture2D* normalMap, const Texture2D* metallicMap,
    const Texture2D* roughnessMap, const Texture2D* aoMap,
    const Texture2D* emissiveMap)
{
    DrawMeshInternal(GetModelMesh(modelPath), transform, red, green, blue, alpha,
        texture, metallic, roughness, ambientOcclusion, emissive,
        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap);
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

    const Vec3 forward = m_Camera.GetForward();
    const Vec3 right = m_Camera.GetRight();
    const Vec3 up = Vec3::Cross(right, forward).Normalized();
    const float tanHalfFov = std::tan(m_Camera.GetFovDegrees() * 0.5f * 0.017453292519943295f);
    const float aspect = m_ViewportHeight > 0
        ? static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight)
        : 1.0f;

    glDisable(GL_DEPTH_TEST);
    m_SkyShader.Bind();
    m_SkyShader.SetVec3("u_CameraForward", forward.x, forward.y, forward.z);
    m_SkyShader.SetVec3("u_CameraRight", right.x, right.y, right.z);
    m_SkyShader.SetVec3("u_CameraUp", up.x, up.y, up.z);
    m_SkyShader.SetVec3("u_SunDirection", m_LightDirection.x, m_LightDirection.y, m_LightDirection.z);
    m_SkyShader.SetVec3("u_SunColor", m_LightColor.x, m_LightColor.y, m_LightColor.z);
    m_SkyShader.SetFloat("u_SunIntensity", m_LightIntensity);
    m_SkyShader.SetFloat("u_TanHalfFov", tanHalfFov);
    m_SkyShader.SetFloat("u_Aspect", aspect);
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
    m_RenderSettings.indirectLightStrength = std::clamp(m_RenderSettings.indirectLightStrength, 0.0f, 2.5f);
    m_RenderSettings.reflectionStrength = std::clamp(m_RenderSettings.reflectionStrength, 0.0f, 2.5f);
    m_RenderSettings.contactShadowStrength = std::clamp(m_RenderSettings.contactShadowStrength, 0.0f, 1.5f);
    const unsigned int desiredShadowSize =
        m_RenderSettings.shadowQuality <= 0 ? 1024u :
        m_RenderSettings.shadowQuality == 1 ? 2048u :
        m_RenderSettings.shadowQuality == 2 ? 4096u : 8192u;
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

bool Renderer::CreatePostProcessTarget()
{
    if (m_ViewportWidth == 0 || m_ViewportHeight == 0) return false;
    glGenFramebuffers(1, &m_PostFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_PostFramebuffer);
    glGenTextures(1, &m_PostColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_PostColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_ViewportWidth, m_ViewportHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
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
    glGenFramebuffers(1, &m_HistoryFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_HistoryFramebuffer);
    glGenTextures(1, &m_HistoryTexture);
    glBindTexture(GL_TEXTURE_2D, m_HistoryTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_ViewportWidth, m_ViewportHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_HistoryTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        DestroyPostProcessTarget();
        return false;
    }
    m_HistoryValid = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Renderer::DestroyPostProcessTarget()
{
    if (m_BloomTexture[0] || m_BloomTexture[1]) glDeleteTextures(2, m_BloomTexture);
    if (m_BloomFramebuffer[0] || m_BloomFramebuffer[1]) glDeleteFramebuffers(2, m_BloomFramebuffer);
    m_BloomTexture[0] = m_BloomTexture[1] = 0;
    m_BloomFramebuffer[0] = m_BloomFramebuffer[1] = 0;
    if (m_HistoryTexture) glDeleteTextures(1, &m_HistoryTexture);
    if (m_HistoryFramebuffer) glDeleteFramebuffers(1, &m_HistoryFramebuffer);
    m_HistoryTexture = 0;
    m_HistoryFramebuffer = 0;
    m_HistoryValid = false;
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
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_Framebuffer.GetDepthTexture());
    m_PostShader.SetInt("u_Depth", 2);
    m_PostShader.SetFloat("u_BloomStrength", bloomTexture ? m_RenderSettings.bloomStrength : 0.0f);
    m_PostShader.SetFloat("u_Exposure", m_RenderSettings.exposure);
    const Vec3 postForward = m_Camera.GetForward();
    const Vec3 postRight = m_Camera.GetRight();
    const Vec3 postUp = Vec3::Cross(postRight, postForward).Normalized();
    const float postTanHalfFov = std::tan(m_Camera.GetFovDegrees() * 0.5f * 0.017453292519943295f);
    const float postAspect = m_ViewportHeight > 0 ? static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight) : 1.0f;
    m_PostShader.SetVec3("u_CameraForward", postForward.x, postForward.y, postForward.z);
    m_PostShader.SetVec3("u_CameraRight", postRight.x, postRight.y, postRight.z);
    m_PostShader.SetVec3("u_CameraUp", postUp.x, postUp.y, postUp.z);
    m_PostShader.SetVec3("u_SunDirection", m_LightDirection.x, m_LightDirection.y, m_LightDirection.z);
    m_PostShader.SetVec3("u_SunColor", m_LightColor.x, m_LightColor.y, m_LightColor.z);
    m_PostShader.SetFloat("u_SunIntensity", m_LightIntensity);
    m_PostShader.SetFloat("u_TanHalfFov", postTanHalfFov);
    m_PostShader.SetFloat("u_Aspect", postAspect);
    glBindVertexArray(m_PostVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_PostShader.Unbind();

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
