#pragma once

#include <memory>
#include <string>
#include <cstdint>

#include <sol/sol.hpp>

#include "../Entity.h"
#include "../Components/ScriptComponent.h"
#include <unordered_map>

class Scene;
class Input;
class Renderer;
class UICanvas;
class Runtime;
class ProjectSettings;

struct LuaEntityHandle
{
    Scene* scene = nullptr;
    std::uint32_t id = 0;
    LuaEntityHandle() = default;
    explicit LuaEntityHandle(std::uint32_t entityID) : id(entityID) {}
    LuaEntityHandle(Scene* owner, std::uint32_t entityID) : scene(owner), id(entityID) {}
};

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
        Runtime* runtime = nullptr,
        ProjectSettings* projectSettings = nullptr
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
    ProjectSettings* m_ProjectSettings = nullptr;

    float m_DeltaTime = 0.0f;
};