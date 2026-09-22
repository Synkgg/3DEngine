#include "Scene.h"

#include "Components/TransformComponent.h"
#include "Components/NameComponent.h"

#include "../Core/Logger.h"

#include <algorithm>
#include <limits>
#include <string>

Scene::Scene(const Scene& other)
{
    *this = other;
}

Scene& Scene::operator=(
    const Scene& other)
{
    if (this == &other)
    {
        return *this;
    }

    m_Entities =
        other.m_Entities;

    m_NextEntityID =
        other.m_NextEntityID;

    m_ComponentStorages.clear();

    for (const auto& [type, storage] :
        other.m_ComponentStorages)
    {
        m_ComponentStorages.emplace(
            type,
            storage->Clone()
        );
    }

    return *this;
}

Entity Scene::CreateEntity()
{
    return CreateEntityWithID(
        m_NextEntityID
    );
}

Entity Scene::CreateEntityWithID(
    std::uint32_t id)
{
    if (id == 0)
    {
        return Entity();
    }

    for (const Entity& entity :
        m_Entities)
    {
        if (entity.GetID() == id)
        {
            return Entity();
        }
    }

    Entity entity(id);

    m_Entities.push_back(entity);

    /*
     * Every entity gets its base components.
     */
    AddComponent<TransformComponent>(
        entity
    );

    AddComponent<NameComponent>(
        entity
    );

    NameComponent* name =
        GetComponent<NameComponent>(
            entity
        );

    if (name != nullptr)
    {
        name->name =
            "Entity " +
            std::to_string(id);
    }

    /*
     * Keep the next generated ID ahead of
     * explicitly supplied entity IDs.
     */
    if (id >= m_NextEntityID)
    {
        if (id ==
            std::numeric_limits<
            std::uint32_t
            >::max())
        {
            m_NextEntityID = id;
        }
        else
        {
            m_NextEntityID =
                id + 1;
        }
    }

    return entity;
}

void Scene::DestroyEntity(
    Entity entity)
{
    if (!entity.IsValid())
    {
        return;
    }

    auto it =
        std::find_if(
            m_Entities.begin(),
            m_Entities.end(),
            [&](const Entity& current)
            {
                return current.GetID() ==
                    entity.GetID();
            }
        );

    if (it == m_Entities.end())
    {
        return;
    }

    /*
     * Remove the entity from every
     * registered component storage.
     */
    for (auto& storage :
        m_ComponentStorages)
    {
        storage.second->Remove(
            entity
        );
    }

    m_Entities.erase(it);
}

void Scene::Clear()
{
    m_Entities.clear();

    /*
     * Clear every registered component
     * storage automatically.
     */
    for (auto& storage :
        m_ComponentStorages)
    {
        storage.second->Clear();
    }

    m_NextEntityID = 1;
}