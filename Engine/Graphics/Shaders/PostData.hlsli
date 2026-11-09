cbuffer PostData : register(b32, space1) {
float4 cameraForward;
float4 cameraRight;
float4 cameraUp;
float4 cameraPosition;
float4 sunDirection;
float4 sunColor;
float4 params0;
float4 params1;
float4 params2;
float4 params3;
float4 previousForward;
float4 previousRight;
float4 previousUp;
float4 previousPosition;
float4 previousParams;
};
#define u_CameraForward cameraForward.xyz
#define u_CameraRight cameraRight.xyz
#define u_CameraUp cameraUp.xyz
#define u_CameraPosition cameraPosition.xyz
#define u_SunDirection sunDirection.xyz
#define u_SunColor sunColor.xyz
#define u_SunIntensity sunColor.w
#define u_Exposure params0.x
#define u_BloomStrength params0.y
#define u_AtmosphereStrength params0.z
#define u_SkyIntensity params0.w
#define u_ColorSaturation params1.x
#define u_Contrast params1.y
#define u_SSRStrength params1.z
#define u_GIStrength params1.w
#define u_TanHalfFov params2.x
#define u_Aspect params2.y
#define u_DebugView params3.x
#define u_HistoryValid params3.y
#define u_Horizontal params3.z
#define u_PreviousForward previousForward.xyz
#define u_PreviousRight previousRight.xyz
#define u_PreviousUp previousUp.xyz
#define u_PreviousPosition previousPosition.xyz
#define u_PreviousTanHalfFov previousParams.x
#define u_PreviousAspect previousParams.y
SamplerState postSampler : register(s0, space0);
#define texture(tex,uv) tex.SampleLevel(postSampler,uv,0)
#define mix lerp
#define lessThan(a,b) ((a)<(b))
#define greaterThan(a,b) ((a)>(b))
#define lessThanEqual(a,b) ((a)<=(b))
#define greaterThanEqual(a,b) ((a)>=(b))
float2 textureSize(Texture2D<float4> tex,int level){uint w,h;tex.GetDimensions(w,h);return float2(w,h);}
float2 make2(float a){return a.xx;} float2 make2(float2 a){return a;} float2 make2(float a,float b){return float2(a,b);}
float3 make3(float a){return a.xxx;} float3 make3(float3 a){return a;} float3 make3(float a,float b,float c){return float3(a,b,c);}
float4 make4(float a){return a.xxxx;} float4 make4(float3 a,float b){return float4(a,b);} float4 make4(float a,float b,float c,float d){return float4(a,b,c,d);}
