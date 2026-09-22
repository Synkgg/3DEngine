#pragma once

#include <memory>
#include <string>

#include <sol/sol.hpp>

#include "../Entity.h"

class Scene;
class Input;
class Renderer;
class UICanvas;

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
        sol::state& lua
    );

    bool Load(const std::string& filepath);
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

    float m_DeltaTime = 0.0f;
};