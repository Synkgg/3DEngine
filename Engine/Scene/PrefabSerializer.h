#pragma once

#include "Entity.h"
#include <string>

class Scene;

class PrefabSerializer
{
public:
    static bool Save(Scene& scene, Entity root, const std::string& filepath);
    static Entity Instantiate(Scene& scene, const std::string& filepath, Entity parent = Entity());
};
