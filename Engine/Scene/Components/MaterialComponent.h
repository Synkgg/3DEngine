#pragma once

#include <string>

struct MaterialComponent
{
    float metallic = 0.0f;
    float roughness = 0.65f;
    float ambientOcclusion = 1.0f;
    float emissive = 0.0f;
    std::string normalMap;
    std::string metallicMap;
    std::string roughnessMap;
    std::string aoMap;
    std::string emissiveMap;
};
