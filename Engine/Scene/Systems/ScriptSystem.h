#pragma once

#include "../Scripting/Script.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Scene;

class ScriptSystem
{
public:
    using ScriptFactory =
        std::function<std::unique_ptr<Script>()>;

    void Register(
        const std::string& name,
        ScriptFactory factory
    );

    void Start(Scene& scene);

    void Update(
        Scene& scene,
        float deltaTime
    );

    void Stop();

private:
    struct ScriptInstance
    {
        Entity entity;
        std::unique_ptr<Script> script;
    };

    std::unordered_map<
        std::string,
        ScriptFactory
    > m_Factories;

    std::unordered_map<
        std::uint32_t,
        std::vector<ScriptInstance>
    > m_Instances;
};