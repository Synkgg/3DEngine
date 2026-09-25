#pragma once

#include <memory>
#include <string>

#include <sol/sol.hpp>

#include "../Entity.h"
#include "../Components/ScriptComponent.h"
#include <unordered_map>

class Scene;
class Input;
class Renderer;
class UICanvas;
class Runtime;

class LuaScript
{
public:
    LuaScript();

    void Initialize(
        Entity entity,
        Scene& scene,
        Input& input,
        Renderer& renderer,
        UICanvas& uiCanvas,
        sol::state& lua,
        Runtime* runtime = nullptr
    );

    bool Load(const std::string& filepath, const std::unordered_map<std::string, ScriptPropertyValue>* propertyOverrides = nullptr);
    bool Create();
    bool Update(float deltaTime);

    void Interact();

    bool Destroy();

private:
    void BindEngineAPI();

    sol::state* m_Lua = nullptr;

    std::unique_ptr<
        sol::environment
    > m_Environment;

    sol::protected_function m_OnCreate;
    sol::protected_function m_OnUpdate;
    sol::protected_function m_OnDestroy;
    sol::protected_function m_OnInteract;

    Entity m_Entity;
    Scene* m_Scene = nullptr;
    Input* m_Input = nullptr;
    Renderer* m_Renderer = nullptr;
    UICanvas* m_UICanvas = nullptr;
    Runtime* m_Runtime = nullptr;

    float m_DeltaTime = 0.0f;
};