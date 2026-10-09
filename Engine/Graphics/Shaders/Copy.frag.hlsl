#include "PostData.hlsli"
Texture2D<float4> source : register(t0, space1);
float4 main([[vk::location(0)]] float2 uv : TEXCOORD0) : SV_Target0 { return source.SampleLevel(postSampler,uv,0); }
