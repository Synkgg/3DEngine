struct Constants
{
    column_major float4x4 mvp;
    float4 color;
    float4 normalX;
    float4 normalY;
    float4 normalZ;
};
[[vk::push_constant]] Constants draw;
struct Input
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
};
struct Output
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
};
Output main(Input input)
{
    Output output;
    output.position = mul(draw.mvp, float4(input.position, 1));
    // CPU matrices retain the editor/gizmo convention. Convert only the raster output.
    // NRI applies the top-left viewport convention, including Vulkan's Y flip.
    output.position.z = (output.position.z + output.position.w) * 0.5;
    output.normal = input.normal.x * draw.normalX.xyz + input.normal.y * draw.normalY.xyz + input.normal.z * draw.normalZ.xyz;
    return output;
}
