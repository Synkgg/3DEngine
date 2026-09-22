#include "PlayerSystem.h"

#include "../Scene.h"

#include "../../Graphics/Renderer.h"
#include "../../Platform/SDL/Input.h"

#include "../Components/PlayerComponent.h"
#include "../Components/TransformComponent.h"

#include "../../Core/Logger.h"

void PlayerSystem::Update(
    Scene& scene,
    Renderer& renderer,
    Input& input,
    float deltaTime)
{
    static bool logged = false;

    if (!logged)
    {
        Logger::Info("PlayerSystem is running.");
        logged = true;
    }

    if (deltaTime <= 0.0f)
    {
        return;
    }

    for (const Entity& entity : scene.GetEntities())
    {
        PlayerComponent* player =
            scene.GetComponent<PlayerComponent>(entity);

        if (player == nullptr)
        {
            continue;
        }

        TransformComponent* transform =
            scene.GetComponent<TransformComponent>(entity);

        if (transform == nullptr)
        {
            continue;
        }

        renderer.SetCameraPosition(
            transform->transform.position
        );

        renderer.RotateCamera(
            input.GetMouseDeltaX() *
            player->lookSensitivity,

            -input.GetMouseDeltaY() *
            player->lookSensitivity
        );

        Vec3 forward =
            renderer.GetCameraRayDirection(
                0.0f,
                0.0f
            );

        // Keep player movement on the ground.
        forward.y = 0.0f;

        if (forward.Length() > 0.0f)
        {
            forward =
                forward.Normalized();
        }

        const Vec3 worldUp(
            0.0f,
            1.0f,
            0.0f
        );

        Vec3 right =
            Vec3::Cross(
                forward,
                worldUp
            ).Normalized();

        float moveForward = 0.0f;
        float moveRight = 0.0f;

        if (input.IsKeyDown(SDL_SCANCODE_W))
        {
            moveForward += 1.0f;
        }

        if (input.IsKeyDown(SDL_SCANCODE_S))
        {
            moveForward -= 1.0f;
        }

        if (input.IsKeyDown(SDL_SCANCODE_D))
        {
            moveRight += 1.0f;
        }

        if (input.IsKeyDown(SDL_SCANCODE_A))
        {
            moveRight -= 1.0f;
        }

        Vec3 movement =
            forward * moveForward +
            right * moveRight;

        if (movement.Length() > 0.0f)
        {
            movement =
                movement.Normalized();

            transform->transform.position =
                transform->transform.position +
                movement *
                (player->moveSpeed * deltaTime);
        }

        // For now we only use the first Player entity.
        break;
    }
}