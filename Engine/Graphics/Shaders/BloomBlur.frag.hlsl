// Ported from OpenGL baseline 0d45bf0. Screen UVs use the RHI top-left convention.
#include "PostData.hlsli"
Texture2D<float4> u_Image : register(t0, space1);

    float4 main([[vk::location(0)]] float2 v_UV : TEXCOORD0) : SV_Target0
    {
float4 FragColor;
        float2 texel = 1.0 / make2(textureSize(u_Image, 0));
        float3 result = texture(u_Image, v_UV).rgb * 0.227027;
        float2 axis = u_Horizontal != 0 ? make2(texel.x, 0.0) : make2(0.0, texel.y);
        result += texture(u_Image, v_UV + axis * 1.0).rgb * 0.1945946;
        result += texture(u_Image, v_UV - axis * 1.0).rgb * 0.1945946;
        result += texture(u_Image, v_UV + axis * 2.0).rgb * 0.1216216;
        result += texture(u_Image, v_UV - axis * 2.0).rgb * 0.1216216;
        result += texture(u_Image, v_UV + axis * 3.0).rgb * 0.054054;
        result += texture(u_Image, v_UV - axis * 3.0).rgb * 0.054054;
        result += texture(u_Image, v_UV + axis * 4.0).rgb * 0.016216;
        result += texture(u_Image, v_UV - axis * 4.0).rgb * 0.016216;
        FragColor = make4(result, 1.0);
    return FragColor;
}
    