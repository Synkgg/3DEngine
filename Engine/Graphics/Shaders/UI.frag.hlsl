struct Constants
{
    float4 color;
    float4 viewport;
    float4 offset;
    float4 gradient;
    float4 style; // useGradient, horizontal, rectWidthPixels, radiusPixels
};
[[vk::push_constant]] Constants draw;
Texture2D<float4> uiTexture : register(t0, space1);
SamplerState uiSampler : register(s0, space0);

float4 main([[vk::location(0)]] float2 uv : TEXCOORD0) : SV_Target0
{
    float t = draw.style.y > 0.5 ? uv.x : uv.y;
    float4 tint = draw.style.x > 0.5 ? lerp(draw.color, draw.gradient, t) : draw.color;

    float radius = draw.style.w;
    if (radius > 0.01)
    {
        float width = max(draw.style.z, 1.0);
        float height = max(abs(ddx(uv.y)) > 0.0 ? width * abs(ddx(uv.x) / ddx(uv.y)) : width, 1.0);
        float2 size = float2(width, height);
        float2 p = uv * size;
        float2 halfSize = size * 0.5;
        radius = min(radius, min(halfSize.x, halfSize.y));
        float2 q = abs(p - halfSize) - (halfSize - radius);
        float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
        if (sd > 0.0) discard;
    }

    float4 texel = uiTexture.Sample(uiSampler, uv);
    return texel * tint;
}
