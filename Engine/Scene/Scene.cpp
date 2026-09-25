#include "Scene.h"

#include "Components/TransformComponent.h"
#include "Components/NameComponent.h"
#include "Components/MeshComponent.h"
#include "Components/ColorComponent.h"
#include "Components/PlayerComponent.h"
#include "Components/CharacterControllerComponent.h"
#include "Components/LightComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/TextureComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/InteractableComponent.h"
#include "Components/ScriptComponent.h"

#include "../Core/Logger.h"

#include <algorithm>
#include <limits>
#include <string>
#include <cmath>
#include <unordered_set>

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

    m_Parents = other.m_Parents;

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

    const std::uint32_t deletedID = entity.GetID();
    m_Parents.erase(deletedID);
    for (auto parentIt = m_Parents.begin(); parentIt != m_Parents.end(); )
    {
        if (parentIt->second == deletedID) parentIt = m_Parents.erase(parentIt);
        else ++parentIt;
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
    m_Parents.clear();
}

bool Scene::SetParent(Entity child, Entity parent, bool keepWorldTransform)
{
    if (!child.IsValid() || !parent.IsValid() || child.GetID() == parent.GetID()) return false;
    if (IsDescendant(parent, child)) return false;

    Transform worldBefore;
    if (keepWorldTransform) worldBefore = GetWorldTransform(child);
    m_Parents[child.GetID()] = parent.GetID();

    if (keepWorldTransform)
    {
        TransformComponent* local = GetComponent<TransformComponent>(child);
        const Transform parentWorld = GetWorldTransform(parent);
        if (local)
        {
            local->transform = worldBefore;
            local->transform.position.x -= parentWorld.position.x;
            local->transform.position.y -= parentWorld.position.y;
            local->transform.position.z -= parentWorld.position.z;
            local->transform.rotation.x -= parentWorld.rotation.x;
            local->transform.rotation.y -= parentWorld.rotation.y;
            local->transform.rotation.z -= parentWorld.rotation.z;
            if (parentWorld.scale.x != 0.0f) local->transform.scale.x /= parentWorld.scale.x;
            if (parentWorld.scale.y != 0.0f) local->transform.scale.y /= parentWorld.scale.y;
            if (parentWorld.scale.z != 0.0f) local->transform.scale.z /= parentWorld.scale.z;

            const float cy = std::cos(-parentWorld.rotation.y), sy = std::sin(-parentWorld.rotation.y);
            const float x = local->transform.position.x, z = local->transform.position.z;
            local->transform.position.x = (x * cy + z * sy) / (parentWorld.scale.x == 0.0f ? 1.0f : parentWorld.scale.x);
            local->transform.position.z = (-x * sy + z * cy) / (parentWorld.scale.z == 0.0f ? 1.0f : parentWorld.scale.z);
            local->transform.position.y /= (parentWorld.scale.y == 0.0f ? 1.0f : parentWorld.scale.y);
        }
    }
    return true;
}

void Scene::ClearParent(Entity child, bool keepWorldTransform)
{
    if (!child.IsValid()) return;
    Transform worldBefore;
    if (keepWorldTransform) worldBefore = GetWorldTransform(child);
    m_Parents.erase(child.GetID());
    if (keepWorldTransform)
    {
        TransformComponent* local = GetComponent<TransformComponent>(child);
        if (local) local->transform = worldBefore;
    }
}

std::vector<Entity> Scene::GetChildren(Entity parent) const
{
    std::vector<Entity> result;
    if (!parent.IsValid()) return result;
    for (const Entity& entity : m_Entities)
    {
        auto it = m_Parents.find(entity.GetID());
        if (it != m_Parents.end() && it->second == parent.GetID()) result.push_back(entity);
    }
    return result;
}

Entity Scene::FindEntityByName(const std::string& name) const
{
    for (const Entity& entity : m_Entities)
    {
        const NameComponent* component = GetComponent<NameComponent>(entity);
        if (component && component->name == name) return entity;
    }
    return Entity();
}

Entity Scene::GetParent(Entity child) const
{
    auto it = m_Parents.find(child.GetID());
    if (it == m_Parents.end()) return Entity();
    for (const Entity& entity : m_Entities)
        if (entity.GetID() == it->second) return entity;
    return Entity();
}

bool Scene::IsDescendant(Entity entity, Entity possibleAncestor) const
{
    if (!entity.IsValid() || !possibleAncestor.IsValid()) return false;
    std::unordered_set<std::uint32_t> visited;
    Entity current = GetParent(entity);
    while (current.IsValid() && visited.insert(current.GetID()).second)
    {
        if (current.GetID() == possibleAncestor.GetID()) return true;
        current = GetParent(current);
    }
    return false;
}

Transform Scene::GetWorldTransform(Entity entity) const
{
    const TransformComponent* component = GetComponent<TransformComponent>(entity);
    Transform result;
    if (!component) return result;
    result = component->transform;

    std::vector<const Transform*> chain;
    Entity current = GetParent(entity);
    std::unordered_set<std::uint32_t> visited;
    while (current.IsValid() && visited.insert(current.GetID()).second)
    {
        const TransformComponent* parentTransform = GetComponent<TransformComponent>(current);
        if (!parentTransform) break;
        chain.push_back(&parentTransform->transform);
        current = GetParent(current);
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const Transform& p = **it;
        const float cy = std::cos(p.rotation.y), sy = std::sin(p.rotation.y);
        const float x = result.position.x * p.scale.x;
        const float y = result.position.y * p.scale.y;
        const float z = result.position.z * p.scale.z;
        result.position.x = p.position.x + x * cy + z * sy;
        result.position.y = p.position.y + y;
        result.position.z = p.position.z - x * sy + z * cy;
        result.rotation.x += p.rotation.x;
        result.rotation.y += p.rotation.y;
        result.rotation.z += p.rotation.z;
        result.scale.x *= p.scale.x;
        result.scale.y *= p.scale.y;
        result.scale.z *= p.scale.z;
    }
    return result;
}

Entity Scene::DuplicateEntity(Entity source, bool duplicateChildren)
{
    if (!source.IsValid()) return Entity();

    Entity duplicate = CreateEntity();
    if (!duplicate.IsValid()) return Entity();

    if (const TransformComponent* value = GetComponent<TransformComponent>(source))
        if (TransformComponent* target = GetComponent<TransformComponent>(duplicate))
            target->transform = value->transform;

    if (const NameComponent* value = GetComponent<NameComponent>(source))
        if (NameComponent* target = GetComponent<NameComponent>(duplicate))
            target->name = value->name + " Copy";

#define COPY_COMPONENT(Type) \
    if (const Type* value = GetComponent<Type>(source)) AddComponent<Type>(duplicate, *value)

    COPY_COMPONENT(MeshComponent);
    COPY_COMPONENT(ColorComponent);
    COPY_COMPONENT(PlayerComponent);
    COPY_COMPONENT(CharacterControllerComponent);
    COPY_COMPONENT(LightComponent);
    COPY_COMPONENT(ColliderComponent);
    COPY_COMPONENT(TextureComponent);
    COPY_COMPONENT(MaterialComponent);
    COPY_COMPONENT(InteractableComponent);
    COPY_COMPONENT(ScriptComponent);

#undef COPY_COMPONENT

    Entity sourceParent = GetParent(source);
    if (sourceParent.IsValid()) SetParent(duplicate, sourceParent, false);

    if (duplicateChildren)
    {
        for (Entity child : GetChildren(source))
        {
            Entity childCopy = DuplicateEntity(child, true);
            if (childCopy.IsValid()) SetParent(childCopy, duplicate, false);
        }
    }

    return duplicate;
}
