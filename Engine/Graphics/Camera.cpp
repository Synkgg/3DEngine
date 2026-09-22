#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float DegreesToRadians = 0.017453292519943295f;

    constexpr float DefaultFov =
        60.0f * DegreesToRadians;

    constexpr float DefaultYaw =
        -90.0f * DegreesToRadians;

    constexpr float DefaultPitch = 0.0f;

    constexpr float DefaultMoveSpeed = 5.0f;

    constexpr float PitchLimit =
        89.0f * DegreesToRadians;
}

Camera::Camera()
    : m_Position(0.0f, 0.0f, 3.0f),
    m_Fov(DefaultFov),
    m_AspectRatio(1280.0f / 720.0f),
    m_NearPlane(0.1f),
    m_FarPlane(1000.0f),
    m_Yaw(DefaultYaw),
    m_Pitch(DefaultPitch),
    m_MoveSpeed(DefaultMoveSpeed)
{
}

Vec3 Camera::GetPosition() const
{
    return m_Position;
}

float Camera::GetYaw() const
{
    return m_Yaw;
}

float Camera::GetPitch() const
{
    return m_Pitch;
}

Vec3 Camera::GetForward() const
{
    const float cosPitch = std::cos(m_Pitch);
    const float sinPitch = std::sin(m_Pitch);

    const float cosYaw = std::cos(m_Yaw);
    const float sinYaw = std::sin(m_Yaw);

    return Vec3(
        cosYaw * cosPitch,
        sinPitch,
        sinYaw * cosPitch
    ).Normalized();
}

Vec3 Camera::GetRight() const
{
    const Vec3 worldUp(
        0.0f,
        1.0f,
        0.0f
    );

    return Vec3::Cross(
        GetForward(),
        worldUp
    ).Normalized();
}

Vec3 Camera::GetRayDirection(
    float ndcX,
    float ndcY) const
{
    const Vec3 forward =
        GetForward();

    const Vec3 right =
        GetRight();

    const Vec3 worldUp(
        0.0f,
        1.0f,
        0.0f
    );

    const Vec3 up =
        Vec3::Cross(
            right,
            forward
        ).Normalized();

    const float tanHalfFov =
        std::tan(m_Fov * 0.5f);

    const float x =
        ndcX *
        tanHalfFov *
        m_AspectRatio;

    const float y =
        ndcY *
        tanHalfFov;

    Vec3 ray =
        forward +
        right * x +
        up * y;

    return ray.Normalized();
}

Mat4 Camera::GetViewMatrix() const
{
    const Vec3 forward =
        GetForward();

    const Vec3 worldUp(
        0.0f,
        1.0f,
        0.0f
    );

    const Vec3 right =
        Vec3::Cross(
            forward,
            worldUp
        ).Normalized();

    const Vec3 up =
        Vec3::Cross(
            right,
            forward
        ).Normalized();

    Mat4 view =
        Mat4::Identity();

    view.elements[0] =
        right.x;

    view.elements[1] =
        up.x;

    view.elements[2] =
        -forward.x;

    view.elements[4] =
        right.y;

    view.elements[5] =
        up.y;

    view.elements[6] =
        -forward.y;

    view.elements[8] =
        right.z;

    view.elements[9] =
        up.z;

    view.elements[10] =
        -forward.z;

    view.elements[12] =
        -Vec3::Dot(
            right,
            m_Position
        );

    view.elements[13] =
        -Vec3::Dot(
            up,
            m_Position
        );

    view.elements[14] =
        Vec3::Dot(
            forward,
            m_Position
        );

    return view;
}

Mat4 Camera::GetProjectionMatrix() const
{
    return Mat4::Perspective(
        m_Fov,
        m_AspectRatio,
        m_NearPlane,
        m_FarPlane
    );
}

void Camera::SetPosition(const Vec3& position)
{
    m_Position = position;
}

void Camera::SetRotation(float yaw, float pitch)
{
    m_Yaw = yaw;
    m_Pitch = pitch;
}

void Camera::SetAspectRatio(float aspectRatio)
{
    if (aspectRatio <= 0.0f)
    {
        return;
    }

    m_AspectRatio = aspectRatio;
}

void Camera::SetMoveSpeed(float speed)
{
    if (speed < 0.0f)
    {
        return;
    }

    m_MoveSpeed = speed;
}

void Camera::Rotate(
    float yawDelta,
    float pitchDelta)
{
    // Horizontal rotation.
    // Completely unlimited.
    m_Yaw += yawDelta;

    // Vertical rotation.
    m_Pitch += pitchDelta;

    // Prevent the camera from flipping over.
    m_Pitch = std::clamp(
        m_Pitch,
        -PitchLimit,
        PitchLimit
    );
}

void Camera::Move(
    float forward,
    float right,
    float up,
    float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const Vec3 worldUp(
        0.0f,
        1.0f,
        0.0f
    );

    // Move exactly in the direction the camera is looking.
    Vec3 forwardDirection = GetForward();

    Vec3 rightDirection =
        Vec3::Cross(
            forwardDirection,
            worldUp
        ).Normalized();

    Vec3 movement =
        forwardDirection * forward +
        rightDirection * right +
        worldUp * up;

    const float length = movement.Length();

    if (length > 0.0f)
    {
        movement =
            movement * (1.0f / length);
    }

    m_Position =
        m_Position +
        movement *
        (m_MoveSpeed * deltaTime);
}

void Camera::Reset()
{
    m_Position = Vec3(
        0.0f,
        0.0f,
        3.0f
    );

    m_Yaw = DefaultYaw;
    m_Pitch = DefaultPitch;
}

void Camera::SetFarPlane(float farPlane)
{
    m_FarPlane = std::clamp(farPlane, 25.0f, 10000.0f);
}

float Camera::GetFarPlane() const
{
    return m_FarPlane;
}
