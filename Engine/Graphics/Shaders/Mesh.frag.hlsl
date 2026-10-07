struct Constants
{
    column_major float4x4 mvp;
    float4 color;
    float4 normalX;
    float4 normalY;
    float4 normalZ;
};
[[vk::push_constant]] Constants draw;
float4 main([[vk::location(0)]] float3 normal : NORMAL) : SV_Target0
{
    float light = 0.25 + 0.75 * saturate(dot(normalize(normal), normalize(float3(0.4, 0.8, 0.6))));
    // Temporary basic scene output is display encoded for the SDR ImGui compositor.
    return float4(pow(saturate(draw.color.rgb * light), 1.0 / 2.2), draw.color.a);
}
