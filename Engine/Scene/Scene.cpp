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
    m_Environment = other.m_Environment;
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
    m_Environment = SceneEnvironment();
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

    const Transform worldBefore = keepWorldTransform ? GetWorldTransform(child) : Transform();
    m_Parents[child.GetID()] = parent.GetID();

    if (keepWorldTransform)
    {
        TransformComponent* local = GetComponent<TransformComponent>(child);
        const Transform parentWorld = GetWorldTransform(parent);
        if (local)
        {
            Vec3 p = worldBefore.position - parentWorld.position;
            auto rotateX=[](Vec3 v,float a){ const float c=std::cos(a),q=std::sin(a); return Vec3(v.x,v.y*c-v.z*q,v.y*q+v.z*c); };
            auto rotateY=[](Vec3 v,float a){ const float c=std::cos(a),q=std::sin(a); return Vec3(v.x*c+v.z*q,v.y,-v.x*q+v.z*c); };
            auto rotateZ=[](Vec3 v,float a){ const float c=std::cos(a),q=std::sin(a); return Vec3(v.x*c-v.y*q,v.x*q+v.y*c,v.z); };
            p=rotateZ(p,-parentWorld.rotation.z);
            p=rotateY(p,-parentWorld.rotation.y);
            p=rotateX(p,-parentWorld.rotation.x);
            if (std::abs(parentWorld.scale.x)>0.000001f) p.x/=parentWorld.scale.x;
            if (std::abs(parentWorld.scale.y)>0.000001f) p.y/=parentWorld.scale.y;
            if (std::abs(parentWorld.scale.z)>0.000001f) p.z/=parentWorld.scale.z;
            local->transform.position=p;
            local->transform.rotation=worldBefore.rotation-parentWorld.rotation;
            local->transform.scale=worldBefore.scale;
            if (std::abs(parentWorld.scale.x)>0.000001f) local->transform.scale.x/=parentWorld.scale.x;
            if (std::abs(parentWorld.scale.y)>0.000001f) local->transform.scale.y/=parentWorld.scale.y;
            if (std::abs(parentWorld.scale.z)>0.000001f) local->transform.scale.z/=parentWorld.scale.z;
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

Entity Scene::FindEntityByID(std::uint32_t id) const
{
    for (const Entity& entity : m_Entities)
        if (entity.GetID() == id) return entity;
    return Entity();
}

std::vector<Entity> Scene::GetRootEntities() const
{
    std::vector<Entity> roots;
    for (const Entity& entity : m_Entities)
        if (!GetParent(entity).IsValid()) roots.push_back(entity);
    return roots;
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

    auto rotateX=[](Vec3 v,float a){ const float c=std::cos(a),q=std::sin(a); return Vec3(v.x,v.y*c-v.z*q,v.y*q+v.z*c); };
    auto rotateY=[](Vec3 v,float a){ const float c=std::cos(a),q=std::sin(a); return Vec3(v.x*c+v.z*q,v.y,-v.x*q+v.z*c); };
    auto rotateZ=[](Vec3 v,float a){ const float c=std::cos(a),q=std::sin(a); return Vec3(v.x*c-v.y*q,v.x*q+v.y*c,v.z); };

    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const Transform& p = **it;
        Vec3 position(result.position.x*p.scale.x,result.position.y*p.scale.y,result.position.z*p.scale.z);
        position=rotateX(position,p.rotation.x);
        position=rotateY(position,p.rotation.y);
        position=rotateZ(position,p.rotation.z);
        result.position=p.position+position;
        result.rotation=result.rotation+p.rotation;
        result.scale=Vec3(result.scale.x*p.scale.x,result.scale.y*p.scale.y,result.scale.z*p.scale.z);
    }
    return result;
}

Entity Scene::DuplicateEntity(Entity source, bool duplicateChildren)
{
    if (!source.IsValid()) return Entity();

    Entity duplicate = CreateEntity();
    if (!duplicate.IsValid()) return Entity();

    // ComponentStorage owns the copy operation, so new component types are
    // automatically included without teaching the editor about them.
    for (auto& [type, storage] : m_ComponentStorages)
        storage->Copy(source, duplicate);

    if (const NameComponent* sourceName = GetComponent<NameComponent>(source))
    {
        if (NameComponent* duplicateName = GetComponent<NameComponent>(duplicate))
            duplicateName->name = sourceName->name + " Copy";
    }

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


Entity Scene::CloneEntityTo(Entity source, Scene& destination, bool cloneChildren) const
{
    if (!source.IsValid()) return Entity();
    Entity copy = destination.CreateEntity();
    if (!copy.IsValid()) return Entity();

    for (const auto& [type, sourceStorage] : m_ComponentStorages)
    {
        auto destinationIt = destination.m_ComponentStorages.find(type);
        if (destinationIt == destination.m_ComponentStorages.end())
        {
            destination.m_ComponentStorages.emplace(type, sourceStorage->Clone());
            destinationIt = destination.m_ComponentStorages.find(type);
            destinationIt->second->Clear();
        }
        sourceStorage->CopyTo(source, *destinationIt->second, copy);
    }

    if (cloneChildren)
    {
        for (Entity child : GetChildren(source))
        {
            Entity childCopy = CloneEntityTo(child, destination, true);
            if (childCopy.IsValid()) destination.SetParent(childCopy, copy, false);
        }
    }
    return copy;
}

void Scene::DestroyEntityHierarchy(Entity root)
{
    if (!root.IsValid()) return;
    const std::vector<Entity> children = GetChildren(root);
    for (Entity child : children) DestroyEntityHierarchy(child);
    DestroyEntity(root);
}
