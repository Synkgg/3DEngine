#pragma once
#include "../../Math/Vec3.h"
#include "../Entity.h"

class Scene;

struct RaycastHit
{
    bool hit = false;
    Entity entity;
    Vec3 point{};
    Vec3 normal{};
    float distance = 0.0f;
};

class CollisionSystem
{
public:
    void Update(Scene& scene);
    static RaycastHit Raycast(const Scene& scene, const Vec3& origin, const Vec3& direction, float maxDistance, Entity ignore = Entity());
};
