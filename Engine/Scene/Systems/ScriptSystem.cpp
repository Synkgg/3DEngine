#include "ScriptSystem.h"

#include "../Scene.h"
#include "../Components/ScriptComponent.h"

void ScriptSystem::Register(
    const std::string& name,
    ScriptFactory factory)
{
    m_Factories[name] =
        std::move(factory);
}

void ScriptSystem::Start(
    Scene& scene)
{
    m_Instances.clear();

    for (const Entity& entity :
        scene.GetEntities())
    {
        ScriptComponent* scriptComponent =
            scene.GetComponent<ScriptComponent>(
                entity
            );

        if (scriptComponent == nullptr)
        {
            continue;
        }

        std::vector<ScriptInstance>& instances =
            m_Instances[entity.GetID()];

        for (const std::string& scriptName :
            scriptComponent->scriptNames)
        {
            const auto factory =
                m_Factories.find(
                    scriptName
                );

            if (factory == m_Factories.end())
            {
                continue;
            }

            std::unique_ptr<Script> script =
                factory->second();

            if (script == nullptr)
            {
                continue;
            }

            script->Initialize(
                entity,
                scene
            );

            script->OnCreate();

            ScriptInstance instance;

            instance.entity =
                entity;

            instance.script =
                std::move(script);

            instances.push_back(
                std::move(instance)
            );
        }
    }
}

void ScriptSystem::Update(
    Scene& scene,
    float deltaTime)
{
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

            instance.script->OnUpdate(
                deltaTime
            );
        }
    }
}

void ScriptSystem::Stop()
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
                instance.script->OnDestroy();
            }
        }
    }

    m_Instances.clear();
}