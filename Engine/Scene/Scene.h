#pragma once

#include "Entity.h"
#include "ComponentStorage.h"
#include "../Math/Transform.h"

#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <string>

struct SceneEnvironment
{
    bool antiAliasing = true;
    int antiAliasingSamples = 4;
    bool shadows = true;
    bool fog = false;
    bool bloom = true;
    float viewDistance = 1000.0f;
    float exposure = 1.0f;
    float fogDensity = 0.003f;
    float bloomStrength = 0.32f;
    int shadowQuality = 2;
    float shadowDistance = 80.0f;
};

class Scene
{
public:
    Scene() = default;

    Scene(const Scene& other);

    Scene& operator=(
        const Scene& other
        );

    Entity CreateEntity();

    Entity CreateEntityWithID(
        std::uint32_t id
    );

    void DestroyEntity(
        Entity entity
    );

    void Clear();

    bool SetParent(Entity child, Entity parent, bool keepWorldTransform = false);
    void ClearParent(Entity child, bool keepWorldTransform = false);
    std::vector<Entity> GetChildren(Entity parent) const;
    Entity FindEntityByName(const std::string& name) const;
    Entity FindEntityByID(std::uint32_t id) const;
    std::vector<Entity> GetRootEntities() const;
    Entity GetParent(Entity child) const;
    bool IsDescendant(Entity entity, Entity possibleAncestor) const;
    Transform GetWorldTransform(Entity entity) const;
    Entity DuplicateEntity(Entity source, bool duplicateChildren = true);
    Entity CloneEntityTo(Entity source, Scene& destination, bool cloneChildren = true) const;
    void DestroyEntityHierarchy(Entity root);
    void QueueDestroyEntityHierarchy(Entity root);
    std::vector<Entity> ConsumePendingDestroyEntities();

    SceneEnvironment& GetEnvironment() { return m_Environment; }
    const SceneEnvironment& GetEnvironment() const { return m_Environment; }

    void SetPrefabSource(Entity entity, const std::string& source);
    void ClearPrefabSource(Entity entity);
    std::string GetPrefabSource(Entity entity) const;

    const std::vector<Entity>& GetEntities() const
    {
        return m_Entities;
    }

    template<typename T>
    T* GetComponent(Entity entity)
    {
        const auto type =
            std::type_index(typeid(T));

        auto it =
            m_ComponentStorages.find(type);

        if (it == m_ComponentStorages.end())
        {
            return nullptr;
        }

        auto* storage =
            static_cast<ComponentStorage<T>*>(
                it->second.get()
                );

        return storage->Get(entity);
    }

    template<typename T>
    const T* GetComponent(Entity entity) const
    {
        const auto type =
            std::type_index(typeid(T));

        auto it =
            m_ComponentStorages.find(type);

        if (it == m_ComponentStorages.end())
        {
            return nullptr;
        }

        const auto* storage =
            static_cast<
            const ComponentStorage<T>*
            >(
                it->second.get()
                );

        return storage->Get(entity);
    }

    template<typename T>
    void AddComponent(
        Entity entity,
        const T& component = T()
    )
    {
        if (!entity.IsValid())
        {
            return;
        }

        const auto type =
            std::type_index(typeid(T));

        auto it =
            m_ComponentStorages.find(type);

        if (it == m_ComponentStorages.end())
        {
            auto storage =
                std::make_unique<
                ComponentStorage<T>
                >();

            storage->Add(
                entity,
                component
            );

            m_ComponentStorages.emplace(
                type,
                std::move(storage)
            );

            return;
        }

        auto* storage =
            static_cast<ComponentStorage<T>*>(
                it->second.get()
                );

        storage->Add(
            entity,
            component
        );
    }

    template<typename T>
    void RemoveComponent(Entity entity)
    {
        const auto type =
            std::type_index(typeid(T));

        auto it =
            m_ComponentStorages.find(type);

        if (it == m_ComponentStorages.end())
        {
            return;
        }

        it->second->Remove(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const
    {
        const auto type =
            std::type_index(typeid(T));

        auto it =
            m_ComponentStorages.find(type);

        if (it == m_ComponentStorages.end())
        {
            return false;
        }

        const auto* storage =
            static_cast<
            const ComponentStorage<T>*
            >(
                it->second.get()
                );

        return storage->Has(entity);
    }

private:
    SceneEnvironment m_Environment;
    std::vector<Entity> m_Entities;

    std::uint32_t m_NextEntityID = 1;

    std::unordered_map<std::uint32_t, std::uint32_t> m_Parents;
    std::unordered_map<std::uint32_t, std::string> m_PrefabSources;
    std::vector<std::uint32_t> m_PendingDestroyEntities;

    std::unordered_map<
        std::type_index,
        std::unique_ptr<IComponentStorage>
    > m_ComponentStorages;
};