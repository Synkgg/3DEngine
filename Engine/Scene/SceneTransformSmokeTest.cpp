#include "Scene.h"
#include "Components/TransformComponent.h"
#include "../Core/Logger.h"

#include <cmath>

// Exercises the three-level hierarchy used by FPS weapon prefabs:
// world-space root -> rotated/scaled mesh -> local muzzle socket.
// This runs without a window, a graphics device, or project assets.
int RunSceneTransformSmokeTest()
{
    Scene scene;
    const Entity root = scene.CreateEntity();
    const Entity mesh = scene.CreateEntity();
    const Entity socket = scene.CreateEntity();
    if (!scene.SetParent(mesh, root, false) ||
        !scene.SetParent(socket, mesh, false))
        return 1;

    constexpr float halfPi = 1.57079632679f;
    auto* rootTransform = scene.GetComponent<TransformComponent>(root);
    auto* meshTransform = scene.GetComponent<TransformComponent>(mesh);
    auto* socketTransform = scene.GetComponent<TransformComponent>(socket);
    if (!rootTransform || !meshTransform || !socketTransform) return 2;

    rootTransform->transform.position = Vec3(10.0f, 1.0f, 2.0f);
    rootTransform->transform.rotation = Vec3(0.0f, halfPi, 0.0f);
    meshTransform->transform.position = Vec3(0.0f, 0.0f, 1.0f);
    meshTransform->transform.rotation = Vec3(0.0f, halfPi, 0.0f);
    meshTransform->transform.scale = Vec3(0.5f, 0.5f, 0.5f);
    socketTransform->transform.position = Vec3(2.0f, 0.0f, 0.0f);

    // Socket +X -> mesh -Z (after scale), offset by mesh +Z, then root
    // rotates the resulting origin and translates it into world space.
    const Transform world = scene.GetWorldTransform(socket);
    const auto near = [](float a, float b) { return std::fabs(a - b) < 0.0005f; };
    if (!near(world.position.x, 10.0f) ||
        !near(world.position.y, 1.0f) ||
        !near(world.position.z, 2.0f) ||
        !near(world.scale.x, 0.5f))
    {
        Logger::Error("Nested muzzle socket world transform is incorrect.");
        return 3;
    }

    Logger::Info("Nested muzzle socket world transform passed.");
    return 0;
}
