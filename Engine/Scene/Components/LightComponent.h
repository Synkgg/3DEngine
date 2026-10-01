#pragma once

#include "../../Math/Vec3.h"

enum class LightType
{
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct LightComponent
{
    LightType type = LightType::Directional;
    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    Vec3 direction = Vec3(-0.5f, -1.0f, -0.5f);
    float intensity = 1.0f;
    float range = 12.0f;
    float innerAngle = 23.0f;
    float outerAngle = 35.0f;
    bool castShadows = true;
};
