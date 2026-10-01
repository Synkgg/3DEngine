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
class UIWidget;
class Runtime;
class ProjectSettings;
class ProjectManager;

class LuaScriptSystem
{
public:
    void Start(
        Scene& scene,
        Input& input,
        Renderer& renderer,
        UICanvas& uiCanvas,
        Runtime* runtime = nullptr,
        ProjectSettings* projectSettings = nullptr,
        ProjectManager* projectManager = nullptr
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

    void ProcessPendingDestructions();
    void DispatchUIEvents(UICanvas& uiCanvas);
    void DispatchButtonEvents(UIWidget& widget);

    bool LoadGlobalScript(
        const std::string& filepath
    );

    std::unordered_map<
        std::uint32_t,
        std::vector<ScriptInstance>
    > m_Instances;

    std::unique_ptr<sol::state> m_Lua;
    Scene* m_Scene = nullptr;
    UICanvas* m_UICanvas = nullptr;
};