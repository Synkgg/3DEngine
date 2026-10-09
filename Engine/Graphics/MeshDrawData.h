#pragma once
#include "../Math/Mat4.h"

// Matches MeshData.hlsli. Every vector occupies a 16-byte constant-buffer slot.
struct MeshDrawData
{
    Mat4 model;
    float lightColorIntensity[4];
    float cameraAmbient[4];
    float material[4]; // metallic, roughness, AO, emissive
    float settings[4]; // exposure, point count, spot count, reserved
    struct Point { float positionRange[4]; float colorIntensity[4]; } points[8];
    struct Spot { float positionRange[4]; float colorIntensity[4]; float directionInner[4]; float outer[4]; } spots[4];
    float environment[4]; // indirect, environment reflection, reflection, sky
    float fog[4]; // enabled, density, view distance, contact shadows
    float mapFlags[4]; // bit mask, combined MR, shadows enabled, PCF radius
    float cascadeSplits[4];
    Mat4 lightMatrices[3];
    Mat4 bones[128];
};
static_assert(sizeof(MeshDrawData) == 9088);
