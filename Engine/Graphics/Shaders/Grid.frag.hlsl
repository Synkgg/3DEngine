struct Constants { column_major float4x4 viewProjection; float4 camera; };
[[vk::push_constant]] Constants draw;
float lines(float2 world, float spacing) {
    float2 cell = world / spacing;
    float2 width = max(fwidth(cell), 0.00001);
    float2 distance = abs(frac(cell - 0.5) - 0.5) / width;
    return (1 - saturate(min(distance.x, distance.y))) * (1 - smoothstep(0.25, 1, max(width.x, width.y)));
}
float4 main([[vk::location(0)]] float2 world : TEXCOORD0) : SV_Target {
    float2 width = max(fwidth(world), 0.00001);
    float xAxis = 1 - saturate(abs(world.y) / width.y);
    float zAxis = 1 - saturate(abs(world.x) / width.x);
    float alpha = max(lines(world, 1)*0.28, lines(world, 10)*0.5);
    float3 color = float3(0.65, 0.68, 0.72);
    if (xAxis > alpha) { color = float3(0.9,0.25,0.22); alpha = xAxis*0.8; }
    if (zAxis > alpha) { color = float3(0.25,0.45,0.95); alpha = zAxis*0.8; }
    alpha *= 1 - smoothstep(40, 150, length(world - draw.camera.xz));
    return float4(color, alpha);
}
