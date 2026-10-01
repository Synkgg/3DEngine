#pragma once

#include "Entity.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

class IComponentStorage
{
public:
    virtual ~IComponentStorage() = default;

    virtual void Remove(Entity entity) = 0;
    virtual void Clear() = 0;
    virtual void Copy(Entity source, Entity destination) = 0;
    virtual void CopyTo(Entity source, IComponentStorage& destinationStorage, Entity destination) const = 0;

    virtual std::unique_ptr<IComponentStorage>
        Clone() const = 0;
};

template<typename T>
class ComponentStorage : public IComponentStorage
{
public:
    T* Get(Entity entity)
    {
        auto it =
            m_Components.find(
                entity.GetID()
            );

        if (it == m_Components.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const T* Get(Entity entity) const
    {
        auto it =
            m_Components.find(
                entity.GetID()
            );

        if (it == m_Components.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    void Add(
        Entity entity,
        const T& component = T()
    )
    {
        if (!entity.IsValid())
        {
            return;
        }

        m_Components[entity.GetID()] =
            component;
    }

    void Remove(Entity entity) override
    {
        if (!entity.IsValid())
        {
            return;
        }

        m_Components.erase(
            entity.GetID()
        );
    }

    bool Has(Entity entity) const
    {
        return m_Components.find(
            entity.GetID()
        ) != m_Components.end();
    }

    void Clear() override
    {
        m_Components.clear();
    }

    void Copy(Entity source, Entity destination) override
    {
        const auto it = m_Components.find(source.GetID());
        if (it != m_Components.end() && destination.IsValid())
            m_Components[destination.GetID()] = it->second;
    }

    void CopyTo(Entity source, IComponentStorage& destinationStorage, Entity destination) const override
    {
        const auto it = m_Components.find(source.GetID());
        if (it == m_Components.end() || !destination.IsValid()) return;
        auto* typedDestination = dynamic_cast<ComponentStorage<T>*>(&destinationStorage);
        if (typedDestination) typedDestination->Add(destination, it->second);
    }

    // PUT Clone() HERE
    std::unique_ptr<IComponentStorage>
        Clone() const override
    {
        return std::make_unique<
            ComponentStorage<T>
        >(*this);
    }

private:
    std::unordered_map<
        std::uint32_t,
        T
    > m_Components;
};