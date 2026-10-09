// Ported from OpenGL baseline 0d45bf0. Screen UVs use the RHI top-left convention.
#include "PostData.hlsli"

    float4 SkyColor(float2 v_UV)
    {
float4 FragColor;
        float2 ndc = v_UV * 2.0 - 1.0;
        float3 ray = normalize(u_CameraForward +
                             u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
                             u_CameraUp * (-ndc.y * u_TanHalfFov));
    
        float height = clamp(ray.y * 0.5 + 0.5, 0.0, 1.0);
        float horizon = exp(-abs(ray.y) * 7.0);
        // These are HDR inputs: ACES and display encoding brighten them later.
        // Keep the horizon blue and fade below it instead of filling the lower
        // hemisphere with the same pale haze.
        float3 zenith = make3(0.018, 0.050, 0.13);
        float3 midSky = make3(0.050, 0.13, 0.28);
        float3 horizonColor = make3(0.090, 0.16, 0.25);
        float3 lowerSky = make3(0.025, 0.040, 0.065);
        float3 sky = mix(lowerSky, horizonColor, smoothstep(0.20, 0.50, height));
        sky = mix(sky, midSky, smoothstep(0.50, 0.72, height));
        sky = mix(sky, zenith, smoothstep(0.70, 1.0, height));
        sky += make3(0.018, 0.014, 0.008) * horizon;
    
        float3 sunDir = normalize(-u_SunDirection);
        float sunDot = max(dot(ray, sunDir), 0.0);
        float sunDisk = smoothstep(0.99972, 0.99993, sunDot);
        float sunGlow = pow(sunDot, 64.0);
        float wideGlow = pow(sunDot, 8.0);
        float3 warmSun = max(u_SunColor, make3(0.75, 0.55, 0.32));
        sky += warmSun * (sunDisk * 7.0 + sunGlow * 0.85 + wideGlow * 0.055) *
               max(u_SunIntensity, 0.25);
    
        // A subtle opposite-horizon haze keeps the atmosphere from reading as a flat gradient.
        float forwardScatter = pow(max(dot(ray, sunDir), 0.0), 3.0);
        sky += make3(0.030, 0.025, 0.018) * forwardScatter * horizon * 0.6;
    
        FragColor = make4(max(sky * u_SkyIntensity, make3(0.0)), 1.0);
    return FragColor;
}
    
struct SkyOutput {float4 color : SV_Target0;float4 normalRoughness : SV_Target1;};
SkyOutput main([[vk::location(0)]] float2 uv : TEXCOORD0) {SkyOutput o;o.color=SkyColor(uv);o.normalRoughness=float4(0.5,0.5,1,1);return o;}
