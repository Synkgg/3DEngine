#pragma once

#include "../../Graphics/PrimitiveType.h"

struct MeshComponent
{
    PrimitiveType primitive = PrimitiveType::None;

    Vec3 offset = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 rotation = Vec3(0.0f, 0.0f, 0.0f);
};