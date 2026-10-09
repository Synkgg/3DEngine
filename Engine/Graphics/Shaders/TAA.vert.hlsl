struct Output { float4 position : SV_Position; [[vk::location(0)]] float2 uv : TEXCOORD0; };
Output main([[vk::location(0)]] float2 position : POSITION) {
    Output o; o.position=float4(position,1,1);o.uv=float2(position.x*0.5+0.5,0.5-position.y*0.5);return o;
}
