#pragma once
struct RenderSettings
{
    bool antiAliasing=true; int antiAliasingSamples=4; bool shadows=true; bool fog=false; bool bloom=true;
    float viewDistance=1000.0f, exposure=1.0f, fogDensity=0.003f, bloomStrength=0.32f;
    int shadowQuality=2; float shadowDistance=80.0f;
    float indirectLightStrength=1.0f, environmentReflectionStrength=1.0f, reflectionStrength=1.0f, contactShadowStrength=1.0f;
    float skyIntensity=1.0f, atmosphereStrength=1.0f, colorSaturation=1.04f, contrast=1.025f;
    bool screenSpaceReflections=true; float screenSpaceReflectionStrength=0.22f; float giStrength=0.35f;
};
