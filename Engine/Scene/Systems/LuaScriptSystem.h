#pragma once

#include "../Scripting/LuaScript.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <sol/sol.hpp>

class Scene;
class Input;
class Renderer;
class UICanvas;

class LuaScriptSystem
{
public:
    void Start(
        Scene& scene,
        Input& input,
        Renderer& renderer,
        UICanvas& uiCanvas
    );

    void Update(
        Scene& scene,
        float deltaTime
    );

    void Interact(
        Entity entity
    );

    void Stop();

private:
    struct ScriptInstance
    {
        std::string path;
        std::unique_ptr<LuaScript> script;
    };

    bool LoadGlobalScript(
        const std::string& filepath
    );

    std::unordered_map<
        std::uint32_t,
        std::vector<ScriptInstance>
    > m_Instances;

    std::unique_ptr<sol::state> m_Lua;
};