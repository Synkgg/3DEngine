#include "CharacterControllerSystem.h"

#include "../Scene.h"

#include "../../Graphics/Renderer.h"
#include "../../Platform/SDL/Input.h"

#include "../Components/TransformComponent.h"
#include "../Components/CharacterControllerComponent.h"

void CharacterControllerSystem::Update(
    Scene& scene,
    Renderer& renderer,
    Input& input,
    float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    for (const Entity& entity :
        scene.GetEntities())
    {
        CharacterControllerComponent* controller =
            scene.GetComponent<
            CharacterControllerComponent
            >(entity);

        if (controller == nullptr)
        {
            continue;
        }

        TransformComponent* transform =
            scene.GetComponent<
            TransformComponent
            >(entity);

        if (transform == nullptr)
        {
            continue;
        }

        /*
         * Remember whether we were grounded
         * before this frame's movement.
         */
        const bool wasGrounded =
            controller->grounded;

        /*
         * CollisionSystem will establish
         * grounded again after movement.
         */
        controller->grounded = false;

        /*
         * Jump requested by Lua.
         */
        if (controller->jumpRequested &&
            wasGrounded)
        {
            controller->verticalVelocity =
                controller->jumpForce;
        }

        controller->jumpRequested = false;

        /*
         * Gravity.
         *
         * A grounded character stays at zero
         * vertical velocity unless jumping.
         */
        if (wasGrounded &&
            controller->verticalVelocity <= 0.0f)
        {
            controller->verticalVelocity =
                0.0f;
        }
        else
        {
            controller->verticalVelocity -=
                controller->gravity *
                deltaTime;
        }

        /*
         * Horizontal movement requested by Lua.
         */
        transform->transform.position.x +=
            controller->horizontalVelocityX *
            deltaTime;

        transform->transform.position.z +=
            controller->horizontalVelocityZ *
            deltaTime;

        /*
         * Vertical movement.
         */
        transform->transform.position.y +=
            controller->verticalVelocity *
            deltaTime;

        /*
         * Horizontal movement is a per-frame
         * request from Lua.
         */
        controller->horizontalVelocityX =
            0.0f;

        controller->horizontalVelocityZ =
            0.0f;
    }
}