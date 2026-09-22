#include "LuaScriptSystem.h"

#include "../Scene.h"
#include "../Components/ScriptComponent.h"

#include "../../Platform/SDL/Input.h"

#include "../../Core/Logger.h"
#include "../../Graphics/Renderer.h"
#include "../../UI/UICanvas.h"

#include <filesystem>

void LuaScriptSystem::Start(
    Scene& scene,
    Input& input,
    Renderer& renderer,
    UICanvas& uiCanvas)
{
    m_Instances.clear();

    m_Lua =
        std::make_unique<
        sol::state
        >();

    m_Lua->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string
    );

    m_Lua->set_function(
        "print",
        [](const std::string& message)
        {
            Logger::Info(
                message
            );
        }
    );

    if (!LoadGlobalScript(
        "Assets\\Scripts\\Systems\\InventoryManager.lua"))
    {
        Logger::Error(
            "Failed to load InventoryManager.lua."
        );
    }

    namespace fs =
        std::filesystem;

    for (const Entity& entity :
        scene.GetEntities())
    {
        ScriptComponent* scriptComponent =
            scene.GetComponent<
            ScriptComponent
            >(entity);

        if (scriptComponent == nullptr)
        {
            continue;
        }

        std::vector<ScriptInstance>& instances =
            m_Instances[
                entity.GetID()
            ];

        for (const std::string& scriptPath :
            scriptComponent->scriptNames)
        {
            if (scriptPath.empty())
            {
                continue;
            }

            fs::path fullPath =
                fs::current_path() /
                scriptPath;

            if (!fs::exists(
                fullPath))
            {
                Logger::Error(
                    std::string(
                        "Lua script not found: "
                    ) +
                    fullPath.string()
                );

                continue;
            }

            std::unique_ptr<LuaScript> script =
                std::make_unique<LuaScript>();

            script->Initialize(
                entity,
                scene,
                input,
                renderer,
                uiCanvas,
                *m_Lua
            );

            if (!script->Load(
                fullPath.string()))
            {
                continue;
            }

            if (!script->Create())
            {
                continue;
            }

            ScriptInstance instance;

            instance.path =
                scriptPath;

            instance.script =
                std::move(script);

            instances.push_back(
                std::move(instance)
            );

            Logger::Info(
                std::string(
                    "Loaded Lua script: "
                ) +
                scriptPath
            );
        }
    }
}

void LuaScriptSystem::Update(
    Scene& scene,
    float deltaTime)
{
    (void)scene;

    for (auto& entityPair :
        m_Instances)
    {
        std::vector<ScriptInstance>& instances =
            entityPair.second;

        for (ScriptInstance& instance :
            instances)
        {
            if (instance.script == nullptr)
            {
                continue;
            }

            instance.script->Update(
                deltaTime
            );
        }
    }
}

void LuaScriptSystem::Interact(
    Entity entity)
{
    auto iterator =
        m_Instances.find(
            entity.GetID()
        );

    if (iterator ==
        m_Instances.end())
    {
        return;
    }

    for (ScriptInstance& instance :
        iterator->second)
    {
        if (instance.script)
        {
            instance.script->Interact();
        }
    }
}

void LuaScriptSystem::Stop()
{
    for (auto& entityPair :
        m_Instances)
    {
        std::vector<ScriptInstance>& instances =
            entityPair.second;

        for (ScriptInstance& instance :
            instances)
        {
            if (instance.script != nullptr)
            {
                instance.script->Destroy();
            }
        }
    }

    m_Instances.clear();

    m_Lua.reset();
}

bool LuaScriptSystem::LoadGlobalScript(
    const std::string& filepath)
{
    if (m_Lua == nullptr)
    {
        return false;
    }

    namespace fs =
        std::filesystem;

    fs::path fullPath =
        fs::current_path() /
        filepath;

    if (!fs::exists(fullPath))
    {
        Logger::Error(
            std::string(
                "Global Lua script not found: "
            ) +
            fullPath.string()
        );

        return false;
    }

    sol::load_result result =
        m_Lua->load_file(
            fullPath.string()
        );

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Failed to load global Lua script: "
            ) +
            filepath +
            " - " +
            error.what()
        );

        return false;
    }

    sol::protected_function script =
        result;

    sol::protected_function_result execution =
        script();

    if (!execution.valid())
    {
        sol::error error =
            execution;

        Logger::Error(
            std::string(
                "Failed to execute global Lua script: "
            ) +
            filepath +
            " - " +
            error.what()
        );

        return false;
    }

    Logger::Info(
        std::string(
            "Loaded global Lua script: "
        ) +
        filepath
    );

    return true;
}