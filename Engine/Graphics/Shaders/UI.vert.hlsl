struct Constants
{
    float4 color;
    float4 viewport; // width, height, scale, unused
    float4 offset;   // x, y, unused, unused
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
    float2 pixel = input.position * draw.viewport.z + draw.offset.xy;
    output.position = float4(pixel.x / draw.viewport.x * 2.0 - 1.0,
                             1.0 - pixel.y / draw.viewport.y * 2.0, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}
