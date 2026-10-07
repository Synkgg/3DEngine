#include "OpenGLShaderSources.h"

namespace Velcryn::Graphics::OpenGLShaders
{
    const char* vertexShaderSource = R"(
    #version 450 core
    
    layout(location = 0) in vec3 a_Position;
    layout(location = 1) in vec3 a_Normal;
    layout(location = 2) in vec2 a_UV;
    layout(location = 3) in uvec4 a_Joints;
    layout(location = 4) in vec4 a_Weights;
    uniform int u_Skinned;
    uniform mat4 u_Bones[128];
    
    uniform mat4 u_Transform;
    uniform mat4 u_Model;
    
    out vec3 v_Normal;
    out vec3 v_WorldPosition;
    out vec2 v_UV;
    out vec4 v_LightSpacePosition[3];
    uniform mat4 u_LightSpaceMatrices[3];
    
    void main()
    {
        mat4 skin=mat4(1.0);
        if(u_Skinned!=0) skin=a_Weights.x*u_Bones[a_Joints.x]+a_Weights.y*u_Bones[a_Joints.y]+a_Weights.z*u_Bones[a_Joints.z]+a_Weights.w*u_Bones[a_Joints.w];
        vec4 localPosition=skin*vec4(a_Position,1.0);
        vec3 localNormal=mat3(skin)*a_Normal;
        mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
        v_Normal = normalize(normalMatrix * localNormal);
        v_WorldPosition = vec3(u_Model * localPosition);
        v_UV = a_UV;
        for (int i = 0; i < 3; ++i)
            v_LightSpacePosition[i] = u_LightSpaceMatrices[i] * vec4(v_WorldPosition, 1.0);
    
        gl_Position =
            u_Transform *
            localPosition;
    }
    )";

    const char* fragmentShaderSource = R"(
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
    uniform int u_UseCombinedMR;
    uniform int u_FogEnabled;
    uniform float u_FogDensity;
    uniform float u_ViewDistance;
    uniform sampler2D u_ShadowMaps[3];
    uniform float u_ShadowCascadeSplits[3];
    uniform int u_ShadowsEnabled;
    uniform int u_ShadowPCFRadius;
    uniform float u_IndirectLightStrength;
    uniform float u_EnvironmentReflectionStrength;
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
    
    float SampleShadowCascade(int cascade, vec3 N, vec3 L)
    {
        vec4 lightSpacePosition=v_LightSpacePosition[cascade]; vec3 p=lightSpacePosition.xyz/max(lightSpacePosition.w,0.0001);p=p*.5+.5;
        if(p.z<=0.0||p.z>=1.0||p.x<=0.0||p.x>=1.0||p.y<=0.0||p.y>=1.0)return 0.0;
        float nDotL=max(dot(N,L),0.0);float bias=max(0.00065*(1.0-nDotL),0.00032);vec2 texel=1.0/vec2(textureSize(u_ShadowMaps[cascade],0));int radius=clamp(u_ShadowPCFRadius,1,3);float shadow=0.0,weight=0.0;
        for(int x=-3;x<=3;++x)for(int y=-3;y<=3;++y){if(abs(x)>radius||abs(y)>radius)continue;float w=1.0/(1.0+0.32*float(x*x+y*y));float closest=texture(u_ShadowMaps[cascade],p.xy+vec2(x,y)*texel).r;shadow+=(p.z-bias>closest?1.0:0.0)*w;weight+=w;}
        float result=clamp(shadow/max(weight,0.0001),0.0,0.88);return clamp(result*mix(0.72,1.0,nDotL)*u_ContactShadowStrength,0.0,0.88);
    }
    float CalculateShadow(vec3 N, vec3 L)
    {
        if(u_ShadowsEnabled==0)return 0.0;float d=length(u_CameraPosition-v_WorldPosition);int cascade=d<=u_ShadowCascadeSplits[0]?0:d<=u_ShadowCascadeSplits[1]?1:2;float s=SampleShadowCascade(cascade,N,L);
        if(cascade<2){float split=u_ShadowCascadeSplits[cascade];float previous=cascade==0?0.0:u_ShadowCascadeSplits[cascade-1];float band=max((split-previous)*0.12,1.0);float blend=smoothstep(split-band,split,d);if(blend>0.0)s=mix(s,SampleShadowCascade(cascade+1,N,L),blend);}return s;
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
        vec4 mrSample = u_UseCombinedMR != 0 ? texture(u_MetallicMap, v_UV) : vec4(0.0);
        float metallic = clamp(u_UseCombinedMR != 0 ? mrSample.b * u_Metallic : (u_UseMetallicMap != 0 ? texture(u_MetallicMap, v_UV).r : u_Metallic), 0.0, 1.0);
        float roughness = clamp(u_UseCombinedMR != 0 ? mrSample.g * u_Roughness : (u_UseRoughnessMap != 0 ? texture(u_RoughnessMap, v_UV).r : u_Roughness), 0.045, 1.0);
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
        vec3 envN = u_UseEnvironmentMap != 0 ? texture(u_IrradianceMap, N).rgb : SampleEnvironment(N, sunL);
        vec3 envR = u_UseEnvironmentMap != 0 ? textureLod(u_EnvironmentMap, R, roughness * 6.0).rgb : SampleEnvironment(R, sunL);
    
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
                     envSpecular * u_EnvironmentReflectionStrength * u_ReflectionStrength) * ao;
    
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

    const char* modelPreviewVertexShaderSource = R"(
    #version 450 core
    layout(location=0) in vec3 a_Position;
    layout(location=1) in vec3 a_Normal;
    layout(location=3) in uvec4 a_Joints;
    layout(location=4) in vec4 a_Weights;
    uniform mat4 u_MVP;
    uniform int u_Skinned;
    uniform mat4 u_Bones[128];
    out vec3 v_Normal;
    void main(){ mat4 skin=mat4(1.0); if(u_Skinned!=0) skin=a_Weights.x*u_Bones[a_Joints.x]+a_Weights.y*u_Bones[a_Joints.y]+a_Weights.z*u_Bones[a_Joints.z]+a_Weights.w*u_Bones[a_Joints.w]; v_Normal=mat3(skin)*a_Normal; gl_Position=u_MVP*skin*vec4(a_Position,1.0); }
    )";

    const char* modelPreviewFragmentShaderSource = R"(
    #version 450 core
    in vec3 v_Normal;
    out vec4 FragColor;
    uniform vec4 u_Color;
    void main(){ vec3 n=normalize(v_Normal); float d=max(dot(n,normalize(vec3(-0.45,0.75,0.55))),0.0); FragColor=vec4(u_Color.rgb*(0.30+d*0.78),u_Color.a); }
    )";

    const char* shadowVertexShaderSource = R"(
    #version 450 core
    layout(location = 0) in vec3 a_Position;
    layout(location = 3) in uvec4 a_Joints;
    layout(location = 4) in vec4 a_Weights;
    uniform mat4 u_Model;
    uniform int u_Skinned;
    uniform mat4 u_Bones[128];
    uniform mat4 u_LightSpaceMatrix;
    void main()
    {
        mat4 skin=mat4(1.0);if(u_Skinned!=0)skin=a_Weights.x*u_Bones[a_Joints.x]+a_Weights.y*u_Bones[a_Joints.y]+a_Weights.z*u_Bones[a_Joints.z]+a_Weights.w*u_Bones[a_Joints.w];
        gl_Position = u_LightSpaceMatrix * u_Model * skin * vec4(a_Position, 1.0);
    }
    )";

    const char* shadowFragmentShaderSource = R"(
    #version 450 core
    void main() {}
    )";

    const char* postVertexShaderSource = R"(
    #version 450 core
    layout(location = 0) in vec2 a_Position;
    out vec2 v_UV;
    void main() { v_UV = a_Position * 0.5 + 0.5; gl_Position = vec4(a_Position, 0.0, 1.0); }
    )";

    const char* postFragmentShaderSource = R"(
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
    uniform int u_DebugView;
     
    float LinearizeDepth(float d)
    {
        const float nearPlane = 0.1;
        const float farPlane = 1000.0;
        float z = d * 2.0 - 1.0;
        return (2.0 * nearPlane * farPlane) / max(farPlane + nearPlane - z * (farPlane - nearPlane), 0.0001);
    }
    
    vec3 ReconstructWorldPosition(vec2 uv, float linearDepth)
    {
        vec2 ndc = uv * 2.0 - 1.0;
        vec3 ray = normalize(u_CameraForward +
            u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
            u_CameraUp * (ndc.y * u_TanHalfFov));
        float forwardAmount = max(dot(ray, u_CameraForward), 0.05);
        return ray * (linearDepth / forwardAmount);
    }
    
    float ScreenAO(vec2 uv)
    {
        float centerRaw = texture(u_Depth, uv).r;
        if (centerRaw >= 0.99999) return 1.0;
        float centerDepth = LinearizeDepth(centerRaw);
        vec3 centerPos = ReconstructWorldPosition(uv, centerDepth);
        vec3 N = normalize(texture(u_NormalRoughness, uv).xyz * 2.0 - 1.0);
        vec2 texel = 1.0 / vec2(textureSize(u_Depth, 0));
        const vec2 dirs[8] = vec2[8](vec2(1,0),vec2(.707,.707),vec2(0,1),vec2(-.707,.707),vec2(-1,0),vec2(-.707,-.707),vec2(0,-1),vec2(.707,-.707));
        float visibility = 0.0, weight = 0.0;
        float pixelRadius = clamp(26.0 / max(centerDepth, 1.0), 2.5, 13.0);
        for(int ring=1; ring<=2; ++ring) for(int i=0;i<8;++i)
        {
            vec2 suv = uv + dirs[i] * texel * pixelRadius * float(ring);
            if(any(lessThanEqual(suv,vec2(0.001))) || any(greaterThanEqual(suv,vec2(0.999)))) continue;
            float sdRaw=texture(u_Depth,suv).r; if(sdRaw>=0.99999) continue;
            float sd=LinearizeDepth(sdRaw); vec3 samplePos=ReconstructWorldPosition(suv,sd);
            vec3 delta=samplePos-centerPos; float dist=length(delta); if(dist<0.001) continue;
            vec3 dir=delta/dist; float horizon=max(dot(N,dir)-0.08,0.0);
            float range=exp(-dist*0.55); float depthReject=1.0-smoothstep(1.5,6.0,abs(sd-centerDepth));
            visibility += horizon*range*depthReject; weight += range*depthReject;
        }
        float occ = weight>0.001 ? visibility/weight : 0.0;
        return clamp(1.0-occ*1.35,0.72,1.0);
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
        vec3 origin = ReconstructWorldPosition(uv, centerDepth);
        vec3 direction = normalize(ray);
        float travel = max(0.12, centerDepth * 0.006);
        float stride = max(0.18, centerDepth * 0.012);
        for(int i=0;i<28;++i)
        {
            travel += stride * (1.0 + float(i) * 0.035);
            vec3 p = origin + direction * travel;
            float forwardDepth = dot(p, u_CameraForward); if(forwardDepth <= 0.1) break;
            float x = dot(p,u_CameraRight)/(forwardDepth*u_TanHalfFov*u_Aspect);
            float y = dot(p,u_CameraUp)/(forwardDepth*u_TanHalfFov);
            vec2 suv=vec2(x,y)*0.5+0.5;
            if(any(lessThan(suv,vec2(0.005)))||any(greaterThan(suv,vec2(0.995)))) break;
            float raw=texture(u_Depth,suv).r; if(raw>=0.99999) continue;
            float sceneDepth=LinearizeDepth(raw); float thickness=max(0.10,sceneDepth*0.008);
            float delta=forwardDepth-sceneDepth;
            if(delta>=0.0 && delta<thickness)
            {
                float edge=min(min(suv.x,suv.y),min(1.0-suv.x,1.0-suv.y));
                float edgeFade=smoothstep(0.01,0.12,edge); float distanceFade=1.0-smoothstep(15.0,90.0,travel);
                return max(texture(u_Scene,suv).rgb,vec3(0.0))*edgeFade*distanceFade;
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
            vec3 reflected = surfaceRoughness < 0.82 ? ScreenSpaceReflection(v_UV, reflectionRay, centerDepth) : vec3(0.0);
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
            vec3 betaR = vec3(0.34,0.19,0.085) * 0.00105 * u_AtmosphereStrength;
            vec3 betaM = vec3(0.075) * 0.00105 * u_AtmosphereStrength;
            vec3 transmittance = vec3(1.0);
            vec3 inscatter = vec3(0.0);
            const int atmosphereSteps = 8;
            float stepLength = distanceToSurface / float(atmosphereSteps);
            for(int stepIndex=0; stepIndex<atmosphereSteps; ++stepIndex)
            {
                float t=(float(stepIndex)+0.5)*stepLength;
                vec3 sampleOffset=ray*t;
                float relativeHeight=max(sampleOffset.y,0.0);
                float rayleighDensity=exp(-relativeHeight*0.018);
                float mieDensity=exp(-relativeHeight*0.065);
                vec3 extinction=(betaR*rayleighDensity+betaM*mieDensity)*stepLength;
                vec3 stepTrans=exp(-extinction);
                vec3 scatter=betaR*rayleighDensity*vec3(0.20,0.38,0.72)*rayleighPhase +
                             betaM*mieDensity*max(u_SunColor,vec3(0.72,0.52,0.32))*miePhase*max(u_SunIntensity,0.25);
                inscatter += transmittance*scatter*stepLength;
                transmittance *= stepTrans;
            }
            hdr = hdr * transmittance + inscatter * 1.35;
        }
    
        if (u_DebugView != 0)
        {
            float debugDepth = texture(u_Depth, v_UV).r;
            vec4 nr = texture(u_NormalRoughness, v_UV);
            if (u_DebugView == 1) { FragColor = vec4(normalize(nr.xyz * 2.0 - 1.0) * 0.5 + 0.5, 1.0); return; }
            if (u_DebugView == 2) { FragColor = vec4(vec3(nr.w), 1.0); return; }
            if (u_DebugView == 3) { float d = debugDepth >= 0.99999 ? 1.0 : clamp(LinearizeDepth(debugDepth) / 100.0, 0.0, 1.0); FragColor = vec4(vec3(d), 1.0); return; }
            if (u_DebugView == 4) { FragColor = vec4(vec3(ScreenAO(v_UV)), 1.0); return; }
            if (u_DebugView == 5)
            {
                if (debugDepth >= 0.99999) { FragColor = vec4(0,0,0,1); return; }
                float cd=LinearizeDepth(debugDepth); vec2 ndc=v_UV*2.0-1.0; vec3 vr=normalize(u_CameraForward+u_CameraRight*(ndc.x*u_TanHalfFov*u_Aspect)+u_CameraUp*(ndc.y*u_TanHalfFov));
                vec3 n=normalize(nr.xyz*2.0-1.0); vec3 reflected=nr.w<0.82?ScreenSpaceReflection(v_UV,reflect(vr,n),cd):vec3(0.0); FragColor=vec4(reflected,1.0); return;
            }
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

    const char* taaFragmentShaderSource = R"(
    #version 450 core
    in vec2 v_UV;
    out vec4 FragColor;
    uniform sampler2D u_Current;
    uniform sampler2D u_History;
    uniform sampler2D u_Depth;
    uniform sampler2D u_NormalRoughness;
    uniform vec3 u_CameraForward, u_CameraRight, u_CameraUp, u_CameraPosition;
    uniform vec3 u_PreviousForward, u_PreviousRight, u_PreviousUp, u_PreviousPosition;
    uniform float u_TanHalfFov, u_Aspect, u_PreviousTanHalfFov, u_PreviousAspect;
    uniform int u_HistoryValid;
    float LinearizeDepth(float d){const float n=.1,f=1000.;float z=d*2.-1.;return (2.*n*f)/max(f+n-z*(f-n),.0001);}
    vec3 ReconstructWorld(vec2 uv,float depth){vec2 ndc=uv*2.-1.;vec3 ray=normalize(u_CameraForward+u_CameraRight*(ndc.x*u_TanHalfFov*u_Aspect)+u_CameraUp*(ndc.y*u_TanHalfFov));float fd=max(dot(ray,u_CameraForward),.05);return u_CameraPosition+ray*(depth/fd);}
    void main(){
     vec3 current=texture(u_Current,v_UV).rgb; if(u_HistoryValid==0){FragColor=vec4(current,1);return;}
     float raw=texture(u_Depth,v_UV).r; if(raw>=.99999){FragColor=vec4(current,1);return;}
     float depth=LinearizeDepth(raw); vec3 world=ReconstructWorld(v_UV,depth); vec3 rel=world-u_PreviousPosition; float z=dot(rel,u_PreviousForward);
     if(z<=.1){FragColor=vec4(current,1);return;} vec2 prevNdc=vec2(dot(rel,u_PreviousRight)/(z*u_PreviousTanHalfFov*u_PreviousAspect),dot(rel,u_PreviousUp)/(z*u_PreviousTanHalfFov)); vec2 prevUV=prevNdc*.5+.5;
     if(any(lessThan(prevUV,vec2(.002)))||any(greaterThan(prevUV,vec2(.998)))){FragColor=vec4(current,1);return;}
     vec2 texel=1./vec2(textureSize(u_Current,0)); vec3 mn=current,mx=current,mean=vec3(0); float count=0;
     for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){vec3 s=texture(u_Current,v_UV+vec2(x,y)*texel).rgb;mn=min(mn,s);mx=max(mx,s);mean+=s;count+=1.;}
     mean/=count; vec3 history=clamp(texture(u_History,prevUV).rgb,mn,mx); float motion=length(prevUV-v_UV); float feedback=clamp(.90-motion*12.,.45,.90);
     vec3 delta=abs(history-mean); float rejection=smoothstep(.06,.32,max(delta.r,max(delta.g,delta.b)));
     vec3 n=normalize(texture(u_NormalRoughness,v_UV).xyz*2.-1.); vec2 px=1./vec2(textureSize(u_Depth,0)); float edge=0.;
     edge=max(edge,abs(LinearizeDepth(texture(u_Depth,v_UV+vec2(px.x,0)).r)-depth)); edge=max(edge,abs(LinearizeDepth(texture(u_Depth,v_UV+vec2(0,px.y)).r)-depth));
     float edgeReject=smoothstep(max(.08,depth*.004),max(.35,depth*.018),edge); float grazing=1.-abs(dot(n,normalize(world-u_CameraPosition))); feedback*=1.-rejection*.82;feedback*=1.-edgeReject*.72;feedback*=1.-grazing*.10;
     FragColor=vec4(mix(current,history,feedback),1);
    }
    )";

    const char* bloomExtractFragmentShaderSource = R"(
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

    const char* bloomBlurFragmentShaderSource = R"(
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

    const char* skyVertexShaderSource = R"(
    #version 450 core
    layout(location = 0) in vec2 a_Position;
    out vec2 v_UV;
    void main() { v_UV = a_Position * 0.5 + 0.5; gl_Position = vec4(a_Position, 1.0, 1.0); }
    )";

    const char* skyFragmentShaderSource = R"(
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

    const char* gridVertexShaderSource = R"(
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

    const char* gridFragmentShaderSource = R"(
    #version 450 core
    
    uniform vec4 u_Color;
    
    out vec4 FragColor;
    
    void main()
    {
        FragColor = u_Color;
    }
    )";
}
