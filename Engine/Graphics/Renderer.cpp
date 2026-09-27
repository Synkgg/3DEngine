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
out vec4 v_LightSpacePosition[3];
uniform mat4 u_LightSpaceMatrices[3];

void main()
{
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    v_Normal = normalize(normalMatrix * a_Normal);
    v_WorldPosition = vec3(u_Model * vec4(a_Position, 1.0));
    v_UV = a_UV;
    for (int i = 0; i < 3; ++i)
        v_LightSpacePosition[i] = u_LightSpaceMatrices[i] * vec4(v_WorldPosition, 1.0);

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
in vec4 v_LightSpacePosition[3];

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
uniform sampler2D u_ShadowMaps[3];
uniform float u_ShadowCascadeSplits[3];
uniform int u_ShadowsEnabled;
uniform int u_ShadowPCFRadius;
uniform float u_IndirectLightStrength;
uniform float u_ReflectionStrength;
uniform float u_ContactShadowStrength;
uniform float u_SkyIntensity;
uniform samplerCube u_EnvironmentMap;
uniform samplerCube u_IrradianceMap;
uniform int u_UseEnvironmentMap;

struct PointLight { vec3 position; vec3 color; float intensity; float range; };
struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float range; float innerCos; float outerCos; };
uniform int u_PointLightCount;
uniform int u_SpotLightCount;
uniform PointLight u_PointLights[8];
uniform SpotLight u_SpotLights[4];

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalRoughness;

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
vec2 EnvBRDFApprox(float roughness, float nDotV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nDotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 SampleEnvironment(vec3 dir, vec3 sunL)
{
    dir = normalize(dir);
    float h = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    float horizon = exp(-abs(dir.y) * 6.5);
    vec3 ground = vec3(0.050, 0.043, 0.038);
    vec3 horizonColor = vec3(0.38, 0.47, 0.58);
    vec3 sky = vec3(0.13, 0.28, 0.52);
    vec3 zenith = vec3(0.025, 0.085, 0.22);
    vec3 result = mix(ground, horizonColor, smoothstep(0.08, 0.50, h));
    result = mix(result, sky, smoothstep(0.46, 0.72, h));
    result = mix(result, zenith, smoothstep(0.72, 1.0, h));
    result += vec3(0.13,0.085,0.045) * horizon;
    float sunDot = max(dot(dir, sunL), 0.0);
    result += max(u_LightColor, vec3(0.72,0.52,0.30)) *
              (pow(sunDot, 64.0) * 0.34 + pow(sunDot, 512.0) * 2.2) *
              max(u_LightIntensity, 0.0);
    return result * u_SkyIntensity;
}

float CalculateShadow(vec3 N, vec3 L)
{
    float cameraDistance = length(u_CameraPosition - v_WorldPosition);
    int cascade = cameraDistance <= u_ShadowCascadeSplits[0] ? 0 :
                  cameraDistance <= u_ShadowCascadeSplits[1] ? 1 : 2;
    vec4 lightSpacePosition = v_LightSpacePosition[cascade];
    if (u_ShadowsEnabled == 0) return 0.0;
    vec3 p = lightSpacePosition.xyz / max(lightSpacePosition.w, 0.0001);
    p = p * 0.5 + 0.5;
    if (p.z <= 0.0 || p.z >= 1.0 || p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0)
        return 0.0;

    float nDotL = max(dot(N, L), 0.0);
    float bias = max(0.00065 * (1.0 - nDotL), 0.00032);
    vec2 texel = 1.0 / vec2(textureSize(u_ShadowMaps[cascade], 0));
    int radius = clamp(u_ShadowPCFRadius, 1, 3);
    float shadow = 0.0;
    float weight = 0.0;
    for (int x = -3; x <= 3; ++x)
    {
        for (int y = -3; y <= 3; ++y)
        {
            if (abs(x) > radius || abs(y) > radius) continue;
            float w = 1.0 / (1.0 + 0.32 * float(x*x + y*y));
            float closest = texture(u_ShadowMaps[cascade], p.xy + vec2(x,y) * texel).r;
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
    float shadow = CalculateShadow(N, sunL);
    float cameraDistance = length(u_CameraPosition - v_WorldPosition);
    float shadowFade = 1.0 - smoothstep(u_ViewDistance * 0.055, u_ViewDistance * 0.22, cameraDistance);
    shadow *= mix(0.72, 1.0, shadowFade);
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

    // Environment lighting shares the same sky response as the visible
    // atmosphere. Diffuse samples the normal hemisphere while specular follows
    // the reflection vector, giving metals and glossy surfaces a coherent world.
    vec3 R = reflect(-V, N);
    vec3 Fenv = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 envN = u_UseEnvironmentMap != 0 ? texture(u_IrradianceMap, N).rgb * u_SkyIntensity : SampleEnvironment(N, sunL);
    vec3 envR = u_UseEnvironmentMap != 0 ? textureLod(u_EnvironmentMap, R, roughness * 6.0).rgb * u_SkyIntensity : SampleEnvironment(R, sunL);

    vec3 localBounce = vec3(0.0);
    for (int i = 0; i < u_PointLightCount; ++i)
    {
        float d = length(u_PointLights[i].position - v_WorldPosition);
        float influence = clamp(1.0 - d / max(u_PointLights[i].range * 1.35, 0.001), 0.0, 1.0);
        localBounce += u_PointLights[i].color * u_PointLights[i].intensity *
                       influence * influence * 0.014;
    }
    for (int i = 0; i < u_SpotLightCount; ++i)
    {
        float d = length(u_SpotLights[i].position - v_WorldPosition);
        float influence = clamp(1.0 - d / max(u_SpotLights[i].range * 1.25, 0.001), 0.0, 1.0);
        localBounce += u_SpotLights[i].color * u_SpotLights[i].intensity *
                       influence * influence * 0.010;
    }

    vec3 kDenv = (vec3(1.0) - Fenv) * (1.0 - metallic);
    vec3 envDiffuse = kDenv * albedo * (envN + localBounce);
    // Roughness broadens and reduces the reflected environment lobe.
    float specularEnergy = mix(1.0, 0.34, roughness * roughness);
    vec2 envBRDF = EnvBRDFApprox(roughness, max(dot(N, V), 0.0));
    vec3 envSpecular = envR * (Fenv * envBRDF.x + envBRDF.y) * specularEnergy;
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
    NormalRoughness = vec4(N * 0.5 + 0.5, roughness);
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
uniform sampler2D u_NormalRoughness;
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
uniform float u_AtmosphereStrength;
uniform float u_ColorSaturation;
uniform float u_Contrast;
uniform float u_SSRStrength;
uniform float u_GIStrength;
 
float LinearizeDepth(float d)
{
    const float nearPlane = 0.1;
    const float farPlane = 1000.0;
    float z = d * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / max(farPlane + nearPlane - z * (farPlane - nearPlane), 0.0001);
}

float ScreenAO(vec2 uv)
{
    vec3 centerNormal = normalize(texture(u_NormalRoughness, uv).xyz * 2.0 - 1.0);
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
            vec3 sampleNormal = normalize(texture(u_NormalRoughness, uv + dirs[i] * texel * radius).xyz * 2.0 - 1.0);
            float normalWeight = 0.35 + 0.65 * (1.0 - max(dot(centerNormal, sampleNormal), 0.0));
            occ += smoothstep(0.015, thickness, delta) * range * normalWeight;
            samples += 1.0;
        }
    }
    if(samples < 1.0) return 1.0;
    return clamp(1.0 - (occ / samples) * 0.42, 0.82, 1.0);
}

vec3 ScreenSpaceGI(vec2 uv, float centerDepth)
{
    vec2 texel = 1.0 / vec2(textureSize(u_Scene, 0));
    vec3 sum = vec3(0.0);
    float weight = 0.0;
    const vec2 dirs[8] = vec2[8](
        vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),
        vec2(.707,.707),vec2(-.707,.707),vec2(.707,-.707),vec2(-.707,-.707));
    for(int ring=1; ring<=3; ++ring)
    {
        float radius = float(ring) * 5.0;
        for(int i=0;i<8;++i)
        {
            vec2 suv = uv + dirs[i] * texel * radius;
            if(any(lessThan(suv,vec2(0.002))) || any(greaterThan(suv,vec2(0.998)))) continue;
            float sdRaw = texture(u_Depth,suv).r;
            if(sdRaw >= 0.99999) continue;
            float sd = LinearizeDepth(sdRaw);
            float depthWeight = exp(-abs(sd-centerDepth) * 0.22);
            float w = depthWeight / float(ring);
            sum += max(texture(u_Scene,suv).rgb,vec3(0.0)) * w;
            weight += w;
        }
    }
    return weight > 0.001 ? sum / weight : vec3(0.0);
}

vec3 ScreenSpaceReflection(vec2 uv, vec3 ray, float centerDepth)
{
    // Lightweight depth-guided reflection trace in screen space. It is kept
    // deliberately conservative to avoid the silhouette halos that broad
    // depth filtering caused in the previous renderer.
    vec2 dir = normalize(vec2(ray.x, -ray.y) + vec2(0.0001));
    vec2 texel = 1.0 / vec2(textureSize(u_Scene,0));
    for(int stepIndex=1; stepIndex<=12; ++stepIndex)
    {
        float stride = float(stepIndex) * 2.5;
        vec2 suv = uv + dir * texel * stride;
        if(any(lessThan(suv,vec2(0.01))) || any(greaterThan(suv,vec2(0.99)))) break;
        float sampleRaw = texture(u_Depth,suv).r;
        if(sampleRaw >= 0.99999) continue;
        float sampleDepth = LinearizeDepth(sampleRaw);
        float expected = centerDepth + stride * 0.035 * max(abs(ray.z),0.2);
        float hit = abs(sampleDepth-expected);
        if(hit < max(0.12, centerDepth * 0.012))
        {
            float edgeFade = smoothstep(0.0,0.12,min(min(suv.x,suv.y),min(1.0-suv.x,1.0-suv.y)));
            return max(texture(u_Scene,suv).rgb,vec3(0.0)) * edgeFade;
        }
    }
    return vec3(0.0);
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

    float rawDepth = texture(u_Depth, v_UV).r;
    if(rawDepth < 0.99999)
    {
        float centerDepth = LinearizeDepth(rawDepth);
        vec2 ndcReflect = v_UV * 2.0 - 1.0;
        vec3 viewRay = normalize(u_CameraForward +
            u_CameraRight * (ndcReflect.x * u_TanHalfFov * u_Aspect) +
            u_CameraUp * (ndcReflect.y * u_TanHalfFov));

        // Low-frequency screen-space bounce gives nearby colored surfaces a
        // restrained indirect contribution. AO masks it into contact regions.
        vec3 gi = ScreenSpaceGI(v_UV, centerDepth);
        hdr += gi * (1.0 - ao) * u_GIStrength;

        vec4 normalRoughness = texture(u_NormalRoughness, v_UV);
        vec3 surfaceNormal = normalize(normalRoughness.xyz * 2.0 - 1.0);
        float surfaceRoughness = normalRoughness.w;
        vec3 reflectionRay = reflect(viewRay, surfaceNormal);
        vec3 reflected = ScreenSpaceReflection(v_UV, reflectionRay, centerDepth);
        float grazing = pow(1.0 - abs(dot(-viewRay, surfaceNormal)), 2.0) * (1.0 - surfaceRoughness);
        hdr = mix(hdr, hdr + reflected * 0.22, clamp(grazing * u_SSRStrength,0.0,0.28));
    }

    // Atmospheric aerial perspective and forward scattering. This is derived
    // from depth and the procedural sun, so it works in every existing scene
    // without requiring authored fog volumes.
    if (rawDepth < 0.99999)
    {
        float distanceToSurface = LinearizeDepth(rawDepth);
        vec2 ndc = v_UV * 2.0 - 1.0;
        vec3 ray = normalize(u_CameraForward +
            u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
            u_CameraUp * (ndc.y * u_TanHalfFov));
        vec3 sunDir = normalize(-u_SunDirection);
        float mu = clamp(dot(ray, sunDir), -1.0, 1.0);
        // Rayleigh + Henyey-Greenstein style Mie phase approximation. It is
        // still a screen-space aerial pass, but responds physically to view/sun angle.
        float rayleighPhase = 0.0596831 * (1.0 + mu * mu);
        const float g = 0.76;
        float miePhase = 0.0795775 * (1.0 - g*g) /
            max(pow(1.0 + g*g - 2.0*g*mu, 1.5), 0.001);
        float density = exp(-max(0.0, dot(ray, u_CameraUp)) * 1.8);
        float opticalDepth = distanceToSurface * 0.00105 * density * u_AtmosphereStrength;
        vec3 extinction = exp(-vec3(0.34,0.19,0.085) * opticalDepth);
        vec3 rayleighColor = vec3(0.20,0.38,0.72) * rayleighPhase;
        vec3 mieColor = max(u_SunColor, vec3(0.72,0.52,0.32)) * miePhase * max(u_SunIntensity,0.25);
        vec3 inscatter = (rayleighColor + mieColor) * (vec3(1.0) - extinction) * 1.55;
        hdr = hdr * extinction + inscatter;
    }

    // Filmic exposure and tone mapping.
    vec3 mapped = ACESFilm(hdr * max(u_Exposure, 0.001));

    // Subtle cinematic color grade: preserve saturation in highlights while
    // avoiding the flat gray look of a plain gamma-only output.
    float luma = dot(mapped, vec3(0.2126,0.7152,0.0722));
    mapped = mix(vec3(luma), mapped, u_ColorSaturation);
    mapped = (mapped - 0.5) * u_Contrast + 0.5;

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
uniform float u_SkyIntensity;

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

    FragColor = vec4(max(sky * u_SkyIntensity, vec3(0.0)), 1.0);
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
























