#include "LuaScript.h"

#include "../../Core/Logger.h"
#include "../../Core/ProjectSettings.h"

#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../Audio/AudioEngine.h"
#include "../../UI/UICanvas.h"
#include "../../UI/UIWidget.h"
#include "../../UI/UIText.h"
#include "../../UI/UIButton.h"
#include "../../UI/UITextInput.h"
#include "../../UI/UISlider.h"
#include "../../UI/UISerializer.h"

#include <filesystem>
#include <fstream>

#include "../Scene.h"
#include "../PrefabSerializer.h"
#include "../Runtime/Runtime.h"
#include "../Components/TransformComponent.h"
#include "../Components/CharacterControllerComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/InteractableComponent.h"
#include "../Components/LightComponent.h"

LuaScript::LuaScript()
{
}

void LuaScript::Initialize(
    Entity entity,
    Scene& scene,
    Input& input,
    Renderer& renderer,
    UICanvas& uiCanvas,
    sol::state& lua,
    Runtime* runtime,
    ProjectSettings* projectSettings)
{
    m_Entity = entity;
    m_Scene = &scene;
    m_Input = &input;
    m_Renderer = &renderer;
    m_UICanvas = &uiCanvas;
    m_Lua = &lua;
    m_Runtime = runtime;
    m_ProjectSettings = projectSettings;

    m_Environment =
        std::make_unique<
        sol::environment
        >(
            lua,
            sol::create,
            lua.globals()
        );

    BindEngineAPI();
}

bool LuaScript::Load(
    const std::string& filepath,
    const std::unordered_map<std::string, ScriptPropertyValue>* propertyOverrides)
{
    if (m_Lua == nullptr ||
        m_Environment == nullptr)
    {
        Logger::Error(
            "LuaScript is not initialized."
        );

        return false;
    }

    sol::load_result result =
        m_Lua->load_file(filepath);

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Failed to load Lua script: "
            ) +
            filepath +
            " - " +
            error.what()
        );

        return false;
    }

    sol::protected_function script =
        result;

    sol::set_environment(
        *m_Environment,
        script
    );

    sol::protected_function_result execution =
        script();

    if (!execution.valid())
    {
        sol::error error =
            execution;

        Logger::Error(
            std::string(
                "Failed to execute Lua script: "
            ) +
            filepath +
            " - " +
            error.what()
        );

        return false;
    }

    // Scripts declare editor-facing defaults in a global Properties table.
    // Instance overrides are injected after the script executes but before OnCreate.
    if (propertyOverrides != nullptr)
    {
        sol::object propertiesObject = (*m_Environment)["Properties"];
        if (propertiesObject.is<sol::table>())
        {
            sol::table properties = propertiesObject.as<sol::table>();
            for (const auto& [name, property] : *propertyOverrides)
            {
                switch (property.type)
                {
                case ScriptPropertyType::Number:
                    try { properties[name] = std::stod(property.value); } catch (...) {}
                    break;
                case ScriptPropertyType::Boolean:
                    properties[name] = (property.value == "1" || property.value == "true");
                    break;
                case ScriptPropertyType::Entity:
                    try { properties[name] = LuaEntityHandle{m_Scene, static_cast<std::uint32_t>(std::stoul(property.value))}; }
                    catch (...) { properties[name] = LuaEntityHandle{m_Scene, 0}; }
                    break;
                case ScriptPropertyType::String:
                default:
                    properties[name] = property.value;
                    break;
                }
            }
        }
    }

    sol::object onCreate =
        (*m_Environment)["OnCreate"];

    if (onCreate.is<
        sol::protected_function>())
    {
        m_OnCreate =
            onCreate.as<
            sol::protected_function
            >();
    }

    sol::object onUpdate =
        (*m_Environment)["OnUpdate"];

    if (onUpdate.is<
        sol::protected_function>())
    {
        m_OnUpdate =
            onUpdate.as<
            sol::protected_function
            >();
    }

    sol::object onDestroy =
        (*m_Environment)["OnDestroy"];

    if (onDestroy.is<
        sol::protected_function>())
    {
        m_OnDestroy =
            onDestroy.as<
            sol::protected_function
            >();
    }

    sol::object onInteract =
        (*m_Environment)["OnInteract"];

    if (onInteract.is<
        sol::protected_function>())
    {
        m_OnInteract =
            onInteract.as<
            sol::protected_function
            >();
    }

    return true;
}

bool LuaScript::Create()
{
    if (!m_OnCreate.valid())
    {
        return true;
    }

    sol::protected_function_result result =
        m_OnCreate();

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnCreate error: "
            ) +
            error.what()
        );

        return false;
    }

    return true;
}

bool LuaScript::Update(
    float deltaTime)
{
    m_DeltaTime =
        deltaTime;

    if (!m_OnUpdate.valid())
    {
        return true;
    }

    sol::protected_function_result result =
        m_OnUpdate(deltaTime);

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnUpdate error: "
            ) +
            error.what()
        );

        return false;
    }

    return true;
}

void LuaScript::Interact()
{
    if (!m_OnInteract.valid())
    {
        return;
    }

    sol::protected_function_result result =
        m_OnInteract();

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnInteract error: "
            ) +
            error.what()
        );
    }
}

bool LuaScript::Invoke(const std::string& functionName)
{
    if (functionName.empty() || m_Environment == nullptr) return false;
    sol::object object = (*m_Environment)[functionName];
    if (!object.is<sol::protected_function>()) return false;
    sol::protected_function callback = object.as<sol::protected_function>();
    sol::protected_function_result result = callback();
    if (!result.valid())
    {
        sol::error error = result;
        Logger::Error("Lua UI event error in " + functionName + ": " + error.what());
        return false;
    }
    return true;
}

bool LuaScript::Destroy()
{
    if (!m_OnDestroy.valid())
    {
        return true;
    }

    sol::protected_function_result result =
        m_OnDestroy();

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnDestroy error: "
            ) +
            error.what()
        );

        return false;
    }

    return true;
}

