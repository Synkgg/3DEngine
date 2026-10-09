// Ported from OpenGL baseline 0d45bf0. Screen UVs use the RHI top-left convention.
#include "PostData.hlsli"
Texture2D<float4> u_Scene : register(t0, space1);
Texture2D<float4> u_Bloom : register(t1, space1);
Texture2D<float4> u_Depth : register(t2, space1);
Texture2D<float4> u_NormalRoughness : register(t3, space1);

    float LinearizeDepth(float d)
    {
        const float nearPlane = params2.z;
        const float farPlane = params2.w;
        float z = d * 2.0 - 1.0;
        return (2.0 * nearPlane * farPlane) / max(farPlane + nearPlane - z * (farPlane - nearPlane), 0.0001);
    }
    
    float3 ReconstructWorldPosition(float2 uv, float linearDepth)
    {
        float2 ndc = uv * 2.0 - 1.0;
        float3 ray = normalize(u_CameraForward +
            u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
            u_CameraUp * (-ndc.y * u_TanHalfFov));
        float forwardAmount = max(dot(ray, u_CameraForward), 0.05);
        return ray * (linearDepth / forwardAmount);
    }
    
    float ScreenAO(float2 uv)
    {
        float centerRaw = texture(u_Depth, uv).r;
        if (centerRaw >= 0.99999) return 1.0;
        float centerDepth = LinearizeDepth(centerRaw);
        float3 centerPos = ReconstructWorldPosition(uv, centerDepth);
        float3 N = normalize(texture(u_NormalRoughness, uv).xyz * 2.0 - 1.0);
        float2 texel = 1.0 / make2(textureSize(u_Depth, 0));
        const float2 dirs[8] = {make2(1,0),make2(.707,.707),make2(0,1),make2(-.707,.707),make2(-1,0),make2(-.707,-.707),make2(0,-1),make2(.707,-.707)};
        float visibility = 0.0, weight = 0.0;
        float pixelRadius = clamp(26.0 / max(centerDepth, 1.0), 2.5, 13.0);
        for(int ring=1; ring<=2; ++ring) for(int i=0;i<8;++i)
        {
            float2 suv = uv + dirs[i] * texel * pixelRadius * float(ring);
            if(any(lessThanEqual(suv,make2(0.001))) || any(greaterThanEqual(suv,make2(0.999)))) continue;
            float sdRaw=texture(u_Depth,suv).r; if(sdRaw>=0.99999) continue;
            float sd=LinearizeDepth(sdRaw); float3 samplePos=ReconstructWorldPosition(suv,sd);
            float3 delta=samplePos-centerPos; float dist=length(delta); if(dist<0.001) continue;
            float3 dir=delta/dist; float horizon=max(dot(N,dir)-0.08,0.0);
            float range=exp(-dist*0.55); float depthReject=1.0-smoothstep(1.5,6.0,abs(sd-centerDepth));
            visibility += horizon*range*depthReject; weight += range*depthReject;
        }
        float occ = weight>0.001 ? visibility/weight : 0.0;
        return clamp(1.0-occ*1.35,0.72,1.0);
    }
    
    float3 ScreenSpaceGI(float2 uv, float centerDepth)
    {
        float2 texel = 1.0 / make2(textureSize(u_Scene, 0));
        float3 sum = make3(0.0);
        float weight = 0.0;
        const float2 dirs[8] = {
            make2(1,0),make2(-1,0),make2(0,1),make2(0,-1),
            make2(.707,.707),make2(-.707,.707),make2(.707,-.707),make2(-.707,-.707)};
        for(int ring=1; ring<=3; ++ring)
        {
            float radius = float(ring) * 5.0;
            for(int i=0;i<8;++i)
            {
                float2 suv = uv + dirs[i] * texel * radius;
                if(any(lessThan(suv,make2(0.002))) || any(greaterThan(suv,make2(0.998)))) continue;
                float sdRaw = texture(u_Depth,suv).r;
                if(sdRaw >= 0.99999) continue;
                float sd = LinearizeDepth(sdRaw);
                float depthWeight = exp(-abs(sd-centerDepth) * 0.22);
                float w = depthWeight / float(ring);
                sum += max(texture(u_Scene,suv).rgb,make3(0.0)) * w;
                weight += w;
            }
        }
        return weight > 0.001 ? sum / weight : make3(0.0);
    }
    
    float3 ScreenSpaceReflection(float2 uv, float3 ray, float centerDepth)
    {
        float3 origin = ReconstructWorldPosition(uv, centerDepth);
        float3 direction = normalize(ray);
        float travel = max(0.12, centerDepth * 0.006);
        float stride = max(0.18, centerDepth * 0.012);
        for(int i=0;i<28;++i)
        {
            travel += stride * (1.0 + float(i) * 0.035);
            float3 p = origin + direction * travel;
            float forwardDepth = dot(p, u_CameraForward); if(forwardDepth <= 0.1) break;
            float x = dot(p,u_CameraRight)/(forwardDepth*u_TanHalfFov*u_Aspect);
            float y = dot(p,u_CameraUp)/(forwardDepth*u_TanHalfFov);
            float2 suv=make2(x,-y)*0.5+0.5;
            if(any(lessThan(suv,make2(0.005)))||any(greaterThan(suv,make2(0.995)))) break;
            float raw=texture(u_Depth,suv).r; if(raw>=0.99999) continue;
            float sceneDepth=LinearizeDepth(raw); float thickness=max(0.10,sceneDepth*0.008);
            float delta=forwardDepth-sceneDepth;
            if(delta>=0.0 && delta<thickness)
            {
                float edge=min(min(suv.x,suv.y),min(1.0-suv.x,1.0-suv.y));
                float edgeFade=smoothstep(0.01,0.12,edge); float distanceFade=1.0-smoothstep(15.0,90.0,travel);
                return max(texture(u_Scene,suv).rgb,make3(0.0))*edgeFade*distanceFade;
            }
        }
        return make3(0.0);
    }
    
    float3 ACESFilm(float3 x)
    {
        const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
        return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
    }
    
    float4 main([[vk::location(0)]] float2 v_UV : TEXCOORD0) : SV_Target0
    {
float4 FragColor;
        float3 hdr = max(texture(u_Scene, v_UV).rgb, make3(0.0));
        float3 bloom = max(texture(u_Bloom, v_UV).rgb, make3(0.0));
        hdr += bloom * u_BloomStrength;
    
        // Depth-aware screen-space ambient occlusion adds contact depth at corners
        // and intersections without changing the authored material colors.
        float ao = ScreenAO(v_UV);
        hdr *= mix(1.0, ao, 0.34);
    
        float rawDepth = texture(u_Depth, v_UV).r;
        if(rawDepth < 0.99999)
        {
            float centerDepth = LinearizeDepth(rawDepth);
            float2 ndcReflect = v_UV * 2.0 - 1.0;
            float3 viewRay = normalize(u_CameraForward +
                u_CameraRight * (ndcReflect.x * u_TanHalfFov * u_Aspect) +
                u_CameraUp * (-ndcReflect.y * u_TanHalfFov));
    
            // Low-frequency screen-space bounce gives nearby colored surfaces a
            // restrained indirect contribution. AO masks it into contact regions.
            float3 gi = u_GIStrength > 0 ? ScreenSpaceGI(v_UV, centerDepth) : make3(0);
            hdr += gi * (1.0 - ao) * u_GIStrength;
    
            float4 normalRoughness = texture(u_NormalRoughness, v_UV);
            float3 surfaceNormal = normalize(normalRoughness.xyz * 2.0 - 1.0);
            float surfaceRoughness = normalRoughness.w;
            float3 reflectionRay = reflect(viewRay, surfaceNormal);
            float3 reflected = surfaceRoughness < 0.82 && u_SSRStrength > 0 ? ScreenSpaceReflection(v_UV, reflectionRay, centerDepth) : make3(0.0);
            float grazing = pow(1.0 - abs(dot(-viewRay, surfaceNormal)), 2.0) * (1.0 - surfaceRoughness);
            hdr = mix(hdr, hdr + reflected * 0.22, clamp(grazing * u_SSRStrength,0.0,0.28));
        }
    
        // Atmospheric aerial perspective and forward scattering. This is derived
        // from depth and the procedural sun, so it works in every existing scene
        // without requiring authored fog volumes.
        if (rawDepth < 0.99999)
        {
            float distanceToSurface = LinearizeDepth(rawDepth);
            float2 ndc = v_UV * 2.0 - 1.0;
            float3 ray = normalize(u_CameraForward +
                u_CameraRight * (ndc.x * u_TanHalfFov * u_Aspect) +
                u_CameraUp * (-ndc.y * u_TanHalfFov));
            float3 sunDir = normalize(-u_SunDirection);
            float mu = clamp(dot(ray, sunDir), -1.0, 1.0);
            // Rayleigh + Henyey-Greenstein style Mie phase approximation. It is
            // still a screen-space aerial pass, but responds physically to view/sun angle.
            float rayleighPhase = 0.0596831 * (1.0 + mu * mu);
            const float g = 0.76;
            float miePhase = 0.0795775 * (1.0 - g*g) /
                max(pow(1.0 + g*g - 2.0*g*mu, 1.5), 0.001);
            float3 betaR = make3(0.34,0.19,0.085) * 0.00105 * u_AtmosphereStrength;
            float3 betaM = make3(0.075) * 0.00105 * u_AtmosphereStrength;
            float3 transmittance = make3(1.0);
            float3 inscatter = make3(0.0);
            const int atmosphereSteps = 8;
            float stepLength = distanceToSurface / float(atmosphereSteps);
            for(int stepIndex=0; stepIndex<atmosphereSteps; ++stepIndex)
            {
                float t=(float(stepIndex)+0.5)*stepLength;
                float3 sampleOffset=ray*t;
                float relativeHeight=max(sampleOffset.y,0.0);
                float rayleighDensity=exp(-relativeHeight*0.018);
                float mieDensity=exp(-relativeHeight*0.065);
                float3 extinction=(betaR*rayleighDensity+betaM*mieDensity)*stepLength;
                float3 stepTrans=exp(-extinction);
                float3 scatter=betaR*rayleighDensity*make3(0.20,0.38,0.72)*rayleighPhase +
                             betaM*mieDensity*max(u_SunColor,make3(0.72,0.52,0.32))*miePhase*max(u_SunIntensity,0.25);
                inscatter += transmittance*scatter*stepLength;
                transmittance *= stepTrans;
            }
            hdr = hdr * transmittance + inscatter * 1.35;
        }
    
        if (u_DebugView != 0)
        {
            float debugDepth = texture(u_Depth, v_UV).r;
            float4 nr = texture(u_NormalRoughness, v_UV);
            if (u_DebugView == 1) { FragColor = make4(normalize(nr.xyz * 2.0 - 1.0) * 0.5 + 0.5, 1.0); return FragColor; }
            if (u_DebugView == 2) { FragColor = make4(make3(nr.w), 1.0); return FragColor; }
            if (u_DebugView == 3) { float d = debugDepth >= 0.99999 ? 1.0 : clamp(LinearizeDepth(debugDepth) / 100.0, 0.0, 1.0); FragColor = make4(make3(d), 1.0); return FragColor; }
            if (u_DebugView == 4) { FragColor = make4(make3(ScreenAO(v_UV)), 1.0); return FragColor; }
            if (u_DebugView == 5)
            {
                if (debugDepth >= 0.99999) { FragColor = make4(0,0,0,1); return FragColor; }
                float cd=LinearizeDepth(debugDepth); float2 ndc=v_UV*2.0-1.0; float3 vr=normalize(u_CameraForward+u_CameraRight*(ndc.x*u_TanHalfFov*u_Aspect)+u_CameraUp*(-ndc.y*u_TanHalfFov));
                float3 n=normalize(nr.xyz*2.0-1.0); float3 reflected=nr.w<0.82?ScreenSpaceReflection(v_UV,reflect(vr,n),cd):make3(0.0); FragColor=make4(reflected,1.0); return FragColor;
            }
        }
    
        // Filmic exposure and tone mapping.
        float3 mapped = ACESFilm(hdr * max(u_Exposure, 0.001));
    
        // Subtle cinematic color grade: preserve saturation in highlights while
        // avoiding the flat gray look of a plain gamma-only output.
        float luma = dot(mapped, make3(0.2126,0.7152,0.0722));
        mapped = mix(make3(luma), mapped, u_ColorSaturation);
        mapped = (mapped - 0.5) * u_Contrast + 0.5;
    
        // Gentle vignette anchors the image without crushing the corners.
        float2 q = v_UV * (1.0 - v_UV.yx);
        float vignette = pow(clamp(16.0 * q.x * q.y, 0.0, 1.0), 0.08);
        mapped *= mix(0.88, 1.0, vignette);
    
        mapped = pow(clamp(mapped,0.0,1.0), make3(1.0/2.2));
    
        // MSAA handles geometry edges. Avoid depth-edge color filtering here:
        // sampling across foreground/background silhouettes creates visible halos.
        FragColor = make4(mapped,1.0);
    return FragColor;
}
    