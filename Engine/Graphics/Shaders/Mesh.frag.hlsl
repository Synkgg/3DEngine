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

float4 main(
    [[vk::location(0)]] float3 normal : NORMAL,
    [[vk::location(1)]] float2 uv : TEXCOORD0) : SV_Target0
{
    const float3 N = normalize(normal);
    const float3 lightDirection = normalize(float3(draw.normalX.w, draw.normalY.w, draw.normalZ.w));
    const float ndotl = saturate(dot(N, -lightDirection));
    const float diffuse = 0.18 + 0.82 * ndotl;
    const float4 texel = baseTexture.Sample(baseSampler, uv);
    // The scene target is floating-point linear. sRGB source textures are
    // decoded by their SRV, and the final SDR compositor performs encoding.
    return float4(texel.rgb * draw.color.rgb * diffuse, texel.a * draw.color.a);
}
