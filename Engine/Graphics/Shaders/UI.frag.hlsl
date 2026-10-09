struct Constants
{
    float4 color;
    float4 viewport;
    float4 offset;
    float4 gradient;
    float4 style; // useGradient, horizontal, rectWidthPixels, radiusPixels
    float4 rect;
    float4 uvRect;
    float4 clip;
};
[[vk::push_constant]] Constants draw;
Texture2D<float4> uiTexture : register(t0, space1);
SamplerState uiSampler : register(s0, space0);

float4 main(float4 position : SV_Position, [[vk::location(0)]] float2 uv : TEXCOORD0, [[vk::location(1)]] float2 localUV : TEXCOORD1) : SV_Target0
{
    if (any(position.xy < draw.clip.xy) || any(position.xy >= draw.clip.zw)) discard;
    float t = draw.style.y > 0.5 ? localUV.x : localUV.y;
    float4 tint = draw.style.x > 0.5 ? lerp(draw.color, draw.gradient, t) : draw.color;

    float radius = draw.style.w;
    if (radius > 0.01)
    {
        float2 size = max(draw.rect.zw * draw.viewport.z, 1.0);
        float2 p = localUV * size;
        float2 halfSize = size * 0.5;
        radius = min(radius, min(halfSize.x, halfSize.y));
        float2 q = abs(p - halfSize) - (halfSize - radius);
        float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
        if (sd > 0.0) discard;
    }

    float4 texel = uiTexture.Sample(uiSampler, uv);
    return texel * tint;
}
