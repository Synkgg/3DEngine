#pragma once

#include "../../Math/Vec3.h"

struct LightComponent
{
    Vec3 color = Vec3(1.0f, 1.0f, 1.0f);
    Vec3 direction = Vec3(-0.5f, -1.0f, -0.5f);
    float intensity = 1.0f;
};