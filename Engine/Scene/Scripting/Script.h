#pragma once

#include "../../Scene/Entity.h"

class Scene;

class Script
{
public:
    virtual ~Script() = default;

    virtual void OnCreate() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    Entity GetEntity() const;
    Scene* GetScene() const;

private:
    friend class ScriptSystem;

    void Initialize(
        Entity entity,
        Scene& scene
    );

    Entity m_Entity;
    Scene* m_Scene = nullptr;
};