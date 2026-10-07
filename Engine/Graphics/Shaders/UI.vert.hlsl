struct Constants
{
    float4 color;
    float4 viewport;
    float4 offset;
    float4 gradient;
    float4 style;
    float4 rect;
    float4 uvRect;
};
[[vk::push_constant]] Constants draw;
struct Input
{
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
};
struct Output
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};
Output main(Input input)
{
    Output output;
    float2 logical = draw.rect.xy + input.position * draw.rect.zw;
    float2 pixel = logical * draw.viewport.z + draw.offset.xy;
    output.position = float4(pixel.x / draw.viewport.x * 2.0 - 1.0,
                             1.0 - pixel.y / draw.viewport.y * 2.0, 0.0, 1.0);
    output.uv = lerp(draw.uvRect.xy, draw.uvRect.zw, input.uv);
    return output;
}
