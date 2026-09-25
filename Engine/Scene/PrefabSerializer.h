#pragma once

#include "Entity.h"
#include <string>
#include <unordered_map>
#include <cstdint>

class Scene;

class PrefabSerializer
{
public:
    static bool Save(Scene& scene, Entity root, const std::string& filepath);
    static Entity Instantiate(Scene& scene, const std::string& filepath, Entity parent = Entity());

    static bool IsInstanceRoot(const Scene& scene, Entity entity);
    static std::string GetSource(const Scene& scene, Entity entity);
    static bool Apply(Scene& scene, Entity instanceRoot);
    static bool Revert(Scene& scene, Entity instanceRoot);
    static bool Unpack(Scene& scene, Entity instanceRoot, bool completely = true);
    static void ForgetEntity(const Scene& scene, Entity entity);
    static void ForgetScene(const Scene& scene);

private:
    struct InstanceInfo
    {
        std::string source;
    };
    static std::unordered_map<const Scene*, std::unordered_map<std::uint32_t, InstanceInfo>> s_Instances;
};
