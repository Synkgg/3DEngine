#pragma once
#include <memory>
#include <string>
class Mesh;

class ModelLoader
{
public:
    // Lightweight Wavefront OBJ loader. Supports v/vt/vn faces and triangulates polygons.
    static std::unique_ptr<Mesh> LoadOBJ(const std::string& filepath);
};
