#pragma once

#include "../Math/Vec3.h"
#include "../Math/Mat4.h"

class Camera
{
public:
    Camera();

    Mat4 GetViewMatrix() const;
    Mat4 GetProjectionMatrix() const;

    Vec3 GetPosition() const;
    float GetYaw() const;
    float GetPitch() const;
    Vec3 GetForward() const;
    Vec3 GetRight() const;
    Vec3 GetRayDirection(float ndcX, float ndcY) const;

    void SetPosition(const Vec3& position);
    void SetRotation(float yaw, float pitch);
    void SetAspectRatio(float aspectRatio);
    void SetMoveSpeed(float speed);
    void SetFarPlane(float farPlane);
    void SetFovDegrees(float degrees);
    float GetFovDegrees() const;
    float GetFarPlane() const;

    void Rotate(float yawDelta, float pitchDelta);
    void Move(float forward, float right, float up, float deltaTime);

    void Reset();

private:
    Vec3 m_Position;

    float m_Fov;
    float m_AspectRatio;
    float m_NearPlane;
    float m_FarPlane;

    float m_Yaw;
    float m_Pitch;

    float m_MoveSpeed;
};