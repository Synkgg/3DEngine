#pragma once

#include "../../Graphics/PrimitiveType.h"
#include <string>

struct MeshComponent
{
    PrimitiveType primitive = PrimitiveType::None;
    // Optional external model. OBJ is supported by the built-in loader.
    std::string modelPath;

    // Hide this mesh for the locally owned Player hierarchy during runtime.
    // Remote replicas still render it, which is useful for first-person bodies.
    bool ownerNoSee = false;

    Vec3 offset = Vec3(0.0f, 0.0f, 0.0f);
    Vec3 rotation = Vec3(0.0f, 0.0f, 0.0f);
};