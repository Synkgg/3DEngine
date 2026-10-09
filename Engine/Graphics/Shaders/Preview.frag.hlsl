struct Constants { column_major float4x4 mvp; float4 color; float4 normalX; float4 normalY; float4 normalZ; };
[[vk::push_constant]] Constants draw;
Texture2D baseColor : register(t0, space1);
SamplerState baseSampler : register(s0, space0);
struct Input { float4 position : SV_Position; [[vk::location(0)]] float3 normal : NORMAL; [[vk::location(1)]] float2 uv : TEXCOORD0; [[vk::location(2)]] float3 worldPosition : TEXCOORD1; };
float4 main(Input input) : SV_Target0 {
    float3 color = draw.color.rgb * baseColor.Sample(baseSampler, input.uv).rgb;
    color *= .30 + max(dot(normalize(input.normal), normalize(float3(-.45,.75,.55))),0)*.78;
    return float4(pow(max(color,0),1.0/2.2),draw.color.a);
}
