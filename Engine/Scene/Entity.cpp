#include "Entity.h"

Entity::Entity()
    : m_ID(0)
{
}

Entity::Entity(std::uint32_t id)
    : m_ID(id)
{
}

std::uint32_t Entity::GetID() const
{
    return m_ID;
}

bool Entity::IsValid() const
{
    return m_ID != 0;
}