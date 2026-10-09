struct Constants {column_major float4x4 vp;float4 color;float4 start;float4 end;};
[[vk::push_constant]] Constants draw;
float4 main() : SV_Target0 {return draw.color;}
