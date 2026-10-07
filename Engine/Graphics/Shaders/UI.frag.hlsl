struct Constants
{
    float4 color;
    float4 viewport;
    float4 offset;
};
[[vk::push_constant]] Constants draw;
Texture2D<float4> uiTexture : register(t0, space1);
SamplerState uiSampler : register(s0, space0);

float3 LinearToSRGB(float3 c)
{
    c = max(c, 0.0);
    return lerp(12.92 * c, 1.055 * pow(c, 1.0 / 2.4) - 0.055, step(0.0031308, c));
}

float4 main([[vk::location(0)]] float2 uv : TEXCOORD0) : SV_Target0
{
    float4 texel = uiTexture.Sample(uiSampler, uv);
    // Runtime UI colors are authored as ordinary sRGB UI values while the
    // scene attachment is RGBA16F. Encode the authored color before storing
    // into that linear HDR attachment so the ImGui viewport presents the same
    // colors the old SDR renderer did.
    float3 authored = saturate(texel.rgb * draw.color.rgb);
    return float4(LinearToSRGB(authored), texel.a * draw.color.a);
}
