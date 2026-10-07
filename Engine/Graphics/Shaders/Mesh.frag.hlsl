struct Constants
{
    column_major float4x4 mvp;
    float4 color;
    float4 normalX;
    float4 normalY;
    float4 normalZ;
};
[[vk::push_constant]] Constants draw;
Texture2D<float4> baseTexture : register(t0, space1);
SamplerState baseSampler : register(s0, space0);

float4 main([[vk::location(0)]] float3 normal : NORMAL,
            [[vk::location(1)]] float2 uv : TEXCOORD0) : SV_Target0
{
    const float3 N = normalize(normal);
    const float3 L = normalize(-float3(draw.normalX.w, draw.normalY.w, draw.normalZ.w));
    const float ndotl = saturate(dot(N, L));

    // Base material color stays independent from light intensity.  The previous
    // Vulkan path multiplied it by the light before reaching this shader,
    // crushing every surface that was not directly facing the sun.
    const float3 ambient = draw.color.rgb * 0.32;
    const float3 direct = draw.color.rgb * (0.68 * ndotl);
    const float4 texel = baseTexture.Sample(baseSampler, uv);
    return float4(texel.rgb * (ambient + direct), texel.a * draw.color.a);
}
