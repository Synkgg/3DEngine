struct Constants { column_major float4x4 viewProjection; float4 camera; };
[[vk::push_constant]] Constants draw;
struct Output {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 world : TEXCOORD0;
};
Output main([[vk::location(0)]] float2 position : POSITION) {
    Output output;
    output.world = position * 200 + floor(draw.camera.xz);
    output.position = mul(draw.viewProjection, float4(output.world.x, 0.002, output.world.y, 1));
    output.position.z = (output.position.z + output.position.w) * 0.5;
    return output;
}
