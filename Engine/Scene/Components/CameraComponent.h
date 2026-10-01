#pragma once

// Marks an entity as a renderable camera. Its TransformComponent supplies the
// local pose and Scene hierarchy composition supplies the world pose.
struct CameraComponent
{
    float fieldOfView = 60.0f;
    float nearClip = 0.1f;
    float farClip = 1000.0f;
    bool active = false;
};
