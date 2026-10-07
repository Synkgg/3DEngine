struct Constants
{
    float4 color;
    float4 viewport;
    float4 offset;
};
[[vk::push_constant]] Constants draw;
Texture2D<float4> uiTexture : register(t0, space0);
SamplerState uiSampler : register(s0, space0);
float4 main([[vk::location(0)]] float2 uv : TEXCOORD0) : SV_Target0
{
    return uiTexture.Sample(uiSampler, uv) * draw.color;
}
