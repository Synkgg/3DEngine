#include "Script.h"

void Script::Initialize(
    Entity entity,
    Scene& scene)
{
    m_Entity = entity;
    m_Scene = &scene;
}

Entity Script::GetEntity() const
{
    return m_Entity;
}

Scene* Script::GetScene() const
{
    return m_Scene;
}