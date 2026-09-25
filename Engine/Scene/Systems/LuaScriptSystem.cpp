#include "LuaScriptSystem.h"

#include "../Scene.h"
#include "../Components/ScriptComponent.h"

#include "../../Platform/SDL/Input.h"

#include "../../Core/Logger.h"
#include "../../Graphics/Renderer.h"
#include "../../UI/UICanvas.h"

#include <filesystem>

namespace fs = std::filesystem;

void LuaScriptSystem::Start(
    Scene& scene,
    Input& input,
    Renderer& renderer,
    UICanvas& uiCanvas,
    Runtime* runtime,
    ProjectSettings* projectSettings)
{
    m_Instances.clear();
    m_Scene = &scene;

    m_Lua =
        std::make_unique<
        sol::state
        >();

    m_Lua->open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::table,
        sol::lib::string,
        sol::lib::package
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

    // Project/game modules are loaded explicitly by authored Lua through require().
    // The engine must never know the name of a particular game or game script.
    sol::table package = (*m_Lua)["package"];
    std::string packagePath = package["path"].get_or(std::string());
    packagePath += ";./?.lua;./?/init.lua";
    package["path"] = packagePath;

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

        // Do not keep a reference into m_Instances while Lua OnCreate runs.
        // OnCreate is allowed to touch engine state, and retaining a reference
        // across callbacks makes this startup path unnecessarily fragile.
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
                *m_Lua,
                runtime,
                projectSettings
            );

            const auto propertyIt = scriptComponent->properties.find(scriptPath);
            const std::unordered_map<std::string, ScriptPropertyValue>* propertyOverrides =
                propertyIt == scriptComponent->properties.end() ? nullptr : &propertyIt->second;

            if (!script->Load(
                fullPath.string(),
                propertyOverrides))
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

            m_Instances[entity.GetID()].push_back(
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

    ProcessPendingDestructions();
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

    ProcessPendingDestructions();
}

void LuaScriptSystem::ProcessPendingDestructions()
{
    if (m_Scene == nullptr) return;

    const std::vector<Entity> roots = m_Scene->ConsumePendingDestroyEntities();
    for (Entity root : roots)
    {
        std::vector<Entity> hierarchy;
        hierarchy.push_back(root);
        for (std::size_t i = 0; i < hierarchy.size(); ++i)
        {
            const std::vector<Entity> children = m_Scene->GetChildren(hierarchy[i]);
            hierarchy.insert(hierarchy.end(), children.begin(), children.end());
        }

        for (Entity entity : hierarchy)
        {
            auto it = m_Instances.find(entity.GetID());
            if (it == m_Instances.end()) continue;
            for (ScriptInstance& instance : it->second)
                if (instance.script) instance.script->Destroy();
            m_Instances.erase(it);
        }

        m_Scene->DestroyEntityHierarchy(root);
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
    m_Scene = nullptr;
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