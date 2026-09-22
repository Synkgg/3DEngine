#pragma once

#include <cstdint>

class Entity
{
public:
    Entity();
    explicit Entity(std::uint32_t id);

    std::uint32_t GetID() const;

    bool IsValid() const;

private:
    std::uint32_t m_ID;
};