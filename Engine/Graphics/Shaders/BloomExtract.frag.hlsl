// Ported from OpenGL baseline 0d45bf0. Screen UVs use the RHI top-left convention.
#include "PostData.hlsli"
Texture2D<float4> u_Scene : register(t0, space1);

    float4 main([[vk::location(0)]] float2 v_UV : TEXCOORD0) : SV_Target0
    {
float4 FragColor;
        float3 color = max(texture(u_Scene, v_UV).rgb, make3(0.0));
        float luminance = dot(color, make3(0.2126, 0.7152, 0.0722));
        float knee = smoothstep(0.62, 1.30, luminance);
        FragColor = make4(color * knee, 1.0);
    return FragColor;
}
    