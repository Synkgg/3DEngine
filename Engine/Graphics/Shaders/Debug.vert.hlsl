struct Constants {column_major float4x4 vp;float4 color;float4 start;float4 end;};
[[vk::push_constant]] Constants draw;
float4 main([[vk::location(0)]] float2 position : POSITION) : SV_Position {float4 p=mul(draw.vp,lerp(draw.start,draw.end,position.x));p.z=(p.z+p.w)*0.5;return p;}
