#include "MeshData.hlsli"
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
    [[vk::location(3)]] float4 joints : TEXCOORD2;
    [[vk::location(4)]] float4 weights : TEXCOORD3;
};
struct Output
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] float3 worldPosition : TEXCOORD1;
};
Output main(Input input)
{
    Output output;
    if (settings.w > 0.5) {
        uint4 joints = min((uint4)input.joints, 127);
        float4x4 skin = bones[joints.x]*input.weights.x+bones[joints.y]*input.weights.y+bones[joints.z]*input.weights.z+bones[joints.w]*input.weights.w;
        input.position = mul(skin,float4(input.position,1)).xyz;
        input.normal = mul((float3x3)skin,input.normal);
    }
    output.position = mul(draw.mvp, float4(input.position, 1));
    // CPU matrices retain the editor/gizmo convention. Convert only the raster output.
    // NRI applies the top-left viewport convention, including Vulkan's Y flip.
    output.position.z = (output.position.z + output.position.w) * 0.5;
    output.normal = input.normal.x * draw.normalX.xyz + input.normal.y * draw.normalY.xyz + input.normal.z * draw.normalZ.xyz;
    output.uv = input.uv;
    output.worldPosition = mul(model, float4(input.position, 1)).xyz;
    return output;
}
