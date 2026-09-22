#include "InteractionSystem.h"

#include "../Scene.h"

#include "../Components/TransformComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/InteractableComponent.h"
#include "../Components/NameComponent.h"

#include "../Systems/LuaScriptSystem.h"

#include <SDL3/SDL.h>

#include "../../Graphics/Renderer.h"
#include "../../Platform/SDL/Input.h"

#include "../../Core/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool InteractionSystem::RayIntersectsAABB(
    const Vec3& rayOrigin,
    const Vec3& rayDirection,
    const Vec3& boxCenter,
    const Vec3& boxHalfExtents,
    float& distance) const
{
    const Vec3 boxMin(
        boxCenter.x - boxHalfExtents.x,
        boxCenter.y - boxHalfExtents.y,
        boxCenter.z - boxHalfExtents.z
    );

    const Vec3 boxMax(
        boxCenter.x + boxHalfExtents.x,
        boxCenter.y + boxHalfExtents.y,
        boxCenter.z + boxHalfExtents.z
    );

    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

    const float epsilon = 0.000001f;

    // X
    if (std::abs(rayDirection.x) < epsilon)
    {
        if (rayOrigin.x < boxMin.x ||
            rayOrigin.x > boxMax.x)
        {
            return false;
        }
    }
    else
    {
        float t1 =
            (boxMin.x - rayOrigin.x) /
            rayDirection.x;

        float t2 =
            (boxMax.x - rayOrigin.x) /
            rayDirection.x;

        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        if (tMin > tMax)
            return false;
    }

    // Y
    if (std::abs(rayDirection.y) < epsilon)
    {
        if (rayOrigin.y < boxMin.y ||
            rayOrigin.y > boxMax.y)
        {
            return false;
        }
    }
    else
    {
        float t1 =
            (boxMin.y - rayOrigin.y) /
            rayDirection.y;

        float t2 =
            (boxMax.y - rayOrigin.y) /
            rayDirection.y;

        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        if (tMin > tMax)
            return false;
    }

    // Z
    if (std::abs(rayDirection.z) < epsilon)
    {
        if (rayOrigin.z < boxMin.z ||
            rayOrigin.z > boxMax.z)
        {
            return false;
        }
    }
    else
    {
        float t1 =
            (boxMin.z - rayOrigin.z) /
            rayDirection.z;

        float t2 =
            (boxMax.z - rayOrigin.z) /
            rayDirection.z;

        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        if (tMin > tMax)
            return false;
    }

    if (tMax < 0.0f)
        return false;

    distance = tMin;

    if (distance < 0.0f)
        distance = tMax;

    return distance >= 0.0f;
}

void InteractionSystem::Update(
    Scene& scene,
    Renderer& renderer,
    Input& input,
    LuaScriptSystem& luaScriptSystem)
{
    m_CurrentPrompt.clear();

    constexpr float interactionDistance = 3.0f;

    const Vec3 rayOrigin =
        renderer.GetCameraPosition();

    const Vec3 rayDirection =
        renderer.GetCameraRayDirection(
            0.0f,
            0.0f
        );

    Entity closestEntity;
    bool foundEntity = false;

    float closestDistance =
        std::numeric_limits<float>::max();

    for (const Entity& entity :
        scene.GetEntities())
    {
        InteractableComponent* interactable =
            scene.GetComponent<
            InteractableComponent
            >(entity);

        if (interactable == nullptr ||
            !interactable->enabled)
        {
            continue;
        }

        TransformComponent* transform =
            scene.GetComponent<
            TransformComponent
            >(entity);

        ColliderComponent* collider =
            scene.GetComponent<
            ColliderComponent
            >(entity);

        if (transform == nullptr ||
            collider == nullptr)
        {
            continue;
        }

        const Vec3 scale =
            transform->transform.scale;

        const Vec3 halfExtents(
            collider->width *
            std::abs(scale.x) *
            0.5f,

            collider->height *
            std::abs(scale.y) *
            0.5f,

            collider->depth *
            std::abs(scale.z) *
            0.5f
        );

        float distance = 0.0f;

        if (!RayIntersectsAABB(
            rayOrigin,
            rayDirection,
            transform->transform.position,
            halfExtents,
            distance))
        {
            continue;
        }

        if (distance > interactionDistance)
            continue;

        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestEntity = entity;
            foundEntity = true;
        }
    }

    if (!foundEntity)
        return;

    InteractableComponent* interactable =
        scene.GetComponent<
        InteractableComponent
        >(closestEntity);

    if (interactable != nullptr)
    {
        m_CurrentPrompt =
            interactable->prompt;
    }

    if (!input.IsKeyPressed(SDL_SCANCODE_E))
        return;

    NameComponent* name =
        scene.GetComponent<
        NameComponent
        >(closestEntity);

    if (name != nullptr)
    {
        Logger::Info(
            std::string("Interacted with ") +
            name->name +
            " - " +
            interactable->prompt
        );
    }
    else
    {
        Logger::Info(
            std::string("Interacted - ") +
            interactable->prompt
        );
    }

    luaScriptSystem.Interact(
        closestEntity
    );
}