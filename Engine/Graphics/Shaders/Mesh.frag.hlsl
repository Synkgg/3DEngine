#include "MeshData.hlsli"
struct Constants
{
    column_major float4x4 mvp;
    float4 color;
    float4 normalX;
    float4 normalY;
    float4 normalZ;
};
[[vk::push_constant]] Constants draw;
Texture2D<float4> baseTexture : register(t0, space1);
Texture2D<float4> normalTexture : register(t1, space1);
Texture2D<float4> metallicTexture : register(t2, space1);
Texture2D<float4> roughnessTexture : register(t3, space1);
Texture2D<float4> aoTexture : register(t4, space1);
Texture2D<float4> emissiveTexture : register(t5, space1);
Texture2D<float> shadow0 : register(t6, space1);
Texture2D<float> shadow1 : register(t7, space1);
Texture2D<float> shadow2 : register(t8, space1);
TextureCube<float4> radianceTexture : register(t9, space1);
TextureCube<float4> irradianceTexture : register(t10, space1);
SamplerState baseSampler : register(s0, space0);

float3 EvaluateLight(float3 N, float3 V, float3 L, float3 radiance, float3 base, float metallic, float roughness)
{
    const float pi = 3.14159265;
    float3 H = normalize(V + L + 1e-7);
    float nv = max(dot(N, V), 0.001), nl = saturate(dot(N, L));
    float nh = saturate(dot(N, H)), vh = saturate(dot(V, H));
    float a = roughness * roughness, a2 = a * a;
    float denom = nh * nh * (a2 - 1) + 1;
    float D = a2 / max(pi * denom * denom, 1e-6);
    float k = (roughness + 1) * (roughness + 1) / 8;
    float G = nv / (nv * (1 - k) + k) * nl / max(nl * (1 - k) + k, 1e-6);
    float3 F = lerp(0.04.xxx, base, metallic);
    F += (1 - F) * pow(1 - vh, 5);
    float3 specular = D * G * F / max(4 * nv * nl, 0.001);
    return ((1 - F) * (1 - metallic) * base / pi + specular) * radiance * nl;
}
float Attenuation(float distanceToLight, float range)
{
    float cutoff = saturate(1 - distanceToLight / max(range, 0.001));
    return cutoff * cutoff / max(1 + 0.045 * distanceToLight * distanceToLight, 1);
}
float ShadowCascade(uint cascade, float3 world, float3 N, float3 L) {
    float4 projected = mul(lightMatrices[cascade], float4(world,1));
    float3 p=projected.xyz/projected.w;
    p=float3(p.x*0.5+0.5,0.5-p.y*0.5,p.z*0.5+0.5);
    if(any(p<=0)||any(p>=1))return 0;
    uint w,h;
    if(cascade==0)shadow0.GetDimensions(w,h); else if(cascade==1)shadow1.GetDimensions(w,h);else shadow2.GetDimensions(w,h);
    float bias=max(0.00065*(1-saturate(dot(N,L))),0.00032), sum=0, total=0;
    int radius=clamp((int)mapFlags.w,1,3);
    for(int x=-radius;x<=radius;++x)for(int y=-radius;y<=radius;++y) {
        float2 uv=clamp(p.xy+float2(x,y)/float2(w,h),0.5/float2(w,h),1-0.5/float2(w,h));float value;
        if(cascade==0)value=shadow0.SampleLevel(baseSampler,uv,0);else if(cascade==1)value=shadow1.SampleLevel(baseSampler,uv,0);else value=shadow2.SampleLevel(baseSampler,uv,0);
        float weight=1/(1+0.32*(x*x+y*y));sum+=(p.z-bias>value?1:0)*weight;total+=weight;
    }
    return min(sum/total*lerp(0.72,1,saturate(dot(N,L)))*fog.w,0.88);
}
float Shadow(float3 world,float3 N,float3 L) {
    if(mapFlags.z<0.5)return 0;
    float d=length(cameraAmbient.xyz-world);uint cascade=d<cascadeSplits.x?0:d<cascadeSplits.y?1:2;
    float result=ShadowCascade(cascade,world,N,L);
    if(cascade<2){float split=cascadeSplits[cascade],prev=cascade==0?0:cascadeSplits[cascade-1];float blend=smoothstep(split-max((split-prev)*0.12,1),split,d);if(blend>0)result=lerp(result,ShadowCascade(cascade+1,world,N,L),blend);}
    return result*lerp(0.72,1,1-smoothstep(fog.z*0.055,fog.z*0.22,d));
}
struct SurfaceOutput { float4 color : SV_Target0; float4 normalRoughness : SV_Target1; };
SurfaceOutput main([[vk::location(0)]] float3 normal : NORMAL,
            [[vk::location(1)]] float2 uv : TEXCOORD0,
            [[vk::location(2)]] float3 worldPosition : TEXCOORD1)
{
    float4 texel = baseTexture.Sample(baseSampler, uv);
    float3 base = max(texel.rgb * draw.color.rgb, 0);
    float3 N = normalize(normal), V = normalize(cameraAmbient.xyz - worldPosition);
    float3 L = normalize(-float3(draw.normalX.w, draw.normalY.w, draw.normalZ.w));
    float metallic = saturate(material.x), roughness = clamp(material.y, 0.045, 1);
    uint flags=(uint)mapFlags.x;
    if(flags&1) {
        float3 dp1=ddx(worldPosition),dp2=ddy(worldPosition);float2 duv1=ddx(uv),duv2=ddy(uv);
        float3 T=dp1*duv2.y-dp2*duv1.y,B=-dp1*duv2.x+dp2*duv1.x;
        if(dot(T,T)>1e-12 && dot(B,B)>1e-12){float3 n=normalTexture.Sample(baseSampler,uv).xyz*2-1;N=normalize(normalize(T)*n.x+normalize(B)*n.y+N*n.z);}
    }
    float4 mr=metallicTexture.Sample(baseSampler,uv);
    if(mapFlags.y>0.5){metallic=saturate(mr.b*metallic);roughness=clamp(mr.g*roughness,0.045,1);}
    else{if(flags&2)metallic=saturate(mr.r);if(flags&4)roughness=clamp(roughnessTexture.Sample(baseSampler,uv).r,0.045,1);}
    float ao=(flags&8)?aoTexture.Sample(baseSampler,uv).r:saturate(material.z);
    float3 result=0;
    result += EvaluateLight(N, V, L, lightColorIntensity.rgb * max(lightColorIntensity.w, 0), base, metallic, roughness)*(1-Shadow(worldPosition,N,L));
    for (uint i = 0; i < (uint)settings.y; ++i)
    {
        float3 delta = points[i].positionRange.xyz - worldPosition;
        float distanceToLight = max(length(delta), 0.001);
        result += EvaluateLight(N, V, delta / distanceToLight,
            points[i].colorIntensity.rgb * max(points[i].colorIntensity.w, 0) * Attenuation(distanceToLight, points[i].positionRange.w), base, metallic, roughness);
    }
    for (uint j = 0; j < (uint)settings.z; ++j)
    {
        float3 delta = spots[j].positionRange.xyz - worldPosition;
        float distanceToLight = max(length(delta), 0.001);
        float3 direction = delta / distanceToLight;
        float cone = saturate((dot(-direction, normalize(spots[j].directionInner.xyz)) - spots[j].outer.x) /
            max(spots[j].directionInner.w - spots[j].outer.x, 0.0001));
        result += EvaluateLight(N, V, direction,
            spots[j].colorIntensity.rgb * max(spots[j].colorIntensity.w, 0) * cone * Attenuation(distanceToLight, spots[j].positionRange.w), base, metallic, roughness);
    }
    float3 F0=lerp(0.04.xxx,base,metallic);
    float nv=saturate(dot(N,V));
    float3 F=F0+(max((1-roughness).xxx,F0)-F0)*pow(1-nv,5);
    float3 diffuse=irradianceTexture.SampleLevel(baseSampler,N,0).rgb;
    float3 reflected=radianceTexture.SampleLevel(baseSampler,reflect(-V,N),roughness*6).rgb;
    float4 r=roughness*float4(-1,-0.0275,-0.572,0.022)+float4(1,0.0425,1.04,-0.04);
    float a004=min(r.x*r.x,exp2(-9.28*nv))*r.x+r.y;
    float2 brdf=float2(-1.04,1.04)*a004+r.zw;
    float3 bounce=0;
    for(uint k=0;k<(uint)settings.y;++k){float influence=saturate(1-length(points[k].positionRange.xyz-worldPosition)/max(points[k].positionRange.w*1.35,0.001));bounce+=points[k].colorIntensity.rgb*points[k].colorIntensity.w*influence*influence*0.014;}
    for(uint k=0;k<(uint)settings.z;++k){float influence=saturate(1-length(spots[k].positionRange.xyz-worldPosition)/max(spots[k].positionRange.w*1.25,0.001));bounce+=spots[k].colorIntensity.rgb*spots[k].colorIntensity.w*influence*influence*0.010;}
    result+=((1-F)*(1-metallic)*base*(diffuse*environment.w+bounce)*environment.x+
        reflected*(F*brdf.x+brdf.y)*lerp(1,0.34,roughness*roughness)*environment.y*environment.z*environment.w)*ao;
    result+=base*((flags&16)?emissiveTexture.Sample(baseSampler,uv).r:max(material.w,0))*2;
    if(fog.x>0.5){float start=max(20,0.38*fog.z);float amount=clamp(smoothstep(start,max(start+1,fog.z),length(cameraAmbient.xyz-worldPosition))*(0.35+fog.y*24)*exp(-max(worldPosition.y,0)*0.025),0,0.92);result=lerp(result,float3(0.42,0.55,0.70),amount);}
    SurfaceOutput output;
    output.color=float4(max(result,0),texel.a*draw.color.a);
    output.normalRoughness=float4(N*0.5+0.5,roughness);
    return output;
}
