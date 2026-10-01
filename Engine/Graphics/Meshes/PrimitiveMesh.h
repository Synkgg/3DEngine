#pragma once

#include "Mesh.h"

#include <memory>

class PrimitiveMesh
{
public:
    static std::unique_ptr<Mesh> CreateCube();
    static std::unique_ptr<Mesh> CreatePlane();
    static std::unique_ptr<Mesh> CreateSphere();
    static std::unique_ptr<Mesh> CreateCylinder();
};