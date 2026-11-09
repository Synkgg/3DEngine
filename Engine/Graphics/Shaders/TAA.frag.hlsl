// Ported from OpenGL baseline 0d45bf0. Screen UVs use the RHI top-left convention.
#include "PostData.hlsli"
Texture2D<float4> u_Current : register(t0, space1);
Texture2D<float4> u_History : register(t1, space1);
Texture2D<float4> u_Depth : register(t2, space1);
Texture2D<float4> u_NormalRoughness : register(t3, space1);

    float LinearizeDepth(float d){const float n=params2.z,f=params2.w;float z=d*2.-1.;return (2.*n*f)/max(f+n-z*(f-n),.0001);}
    float3 ReconstructWorld(float2 uv,float depth){float2 ndc=uv*2.-1.;float3 ray=normalize(u_CameraForward+u_CameraRight*(ndc.x*u_TanHalfFov*u_Aspect)+u_CameraUp*(-ndc.y*u_TanHalfFov));float fd=max(dot(ray,u_CameraForward),.05);return u_CameraPosition+ray*(depth/fd);}
    float4 main([[vk::location(0)]] float2 v_UV : TEXCOORD0) : SV_Target0{
float4 FragColor;
     float3 current=texture(u_Current,v_UV).rgb; if(u_HistoryValid==0){FragColor=make4(current,1);return FragColor;}
     float raw=texture(u_Depth,v_UV).r; if(raw>=.99999){FragColor=make4(current,1);return FragColor;}
     float depth=LinearizeDepth(raw); float3 world=ReconstructWorld(v_UV,depth); float3 rel=world-u_PreviousPosition; float z=dot(rel,u_PreviousForward);
     if(z<=.1){FragColor=make4(current,1);return FragColor;} float2 prevNdc=make2(dot(rel,u_PreviousRight)/(z*u_PreviousTanHalfFov*u_PreviousAspect),dot(rel,u_PreviousUp)/(z*u_PreviousTanHalfFov)); float2 prevUV=prevNdc*make2(.5,-.5)+.5;
     if(any(lessThan(prevUV,make2(.002)))||any(greaterThan(prevUV,make2(.998)))){FragColor=make4(current,1);return FragColor;}
     float2 texel=1./make2(textureSize(u_Current,0)); float3 mn=current,mx=current,mean=make3(0); float count=0;
     for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){float3 s=texture(u_Current,v_UV+make2(x,y)*texel).rgb;mn=min(mn,s);mx=max(mx,s);mean+=s;count+=1.;}
     mean/=count; float3 history=clamp(texture(u_History,prevUV).rgb,mn,mx); float motion=length(prevUV-v_UV); float feedback=clamp(.90-motion*12.,.45,.90);
     float3 delta=abs(history-mean); float rejection=smoothstep(.06,.32,max(delta.r,max(delta.g,delta.b)));
     float3 n=normalize(texture(u_NormalRoughness,v_UV).xyz*2.-1.); float2 px=1./make2(textureSize(u_Depth,0)); float edge=0.;
     edge=max(edge,abs(LinearizeDepth(texture(u_Depth,v_UV+make2(px.x,0)).r)-depth)); edge=max(edge,abs(LinearizeDepth(texture(u_Depth,v_UV+make2(0,px.y)).r)-depth));
     float edgeReject=smoothstep(max(.08,depth*.004),max(.35,depth*.018),edge); float grazing=1.-abs(dot(n,normalize(world-u_CameraPosition))); feedback*=1.-rejection*.82;feedback*=1.-edgeReject*.72;feedback*=1.-grazing*.10;
     FragColor=make4(mix(current,history,feedback),1);
    return FragColor;
}
    