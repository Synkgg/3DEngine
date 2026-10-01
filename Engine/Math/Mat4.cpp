#include "Mat4.h"

#include <cmath>

Mat4::Mat4()
    : elements{}
{
}

Mat4 Mat4::Identity()
{
    Mat4 result;

    result.elements[0] = 1.0f;
    result.elements[5] = 1.0f;
    result.elements[10] = 1.0f;
    result.elements[15] = 1.0f;

    return result;
}

Mat4 Mat4::Translation(const Vec3& position)
{
    Mat4 result = Mat4::Identity();

    result.elements[12] = position.x;
    result.elements[13] = position.y;
    result.elements[14] = position.z;

    return result;
}

Mat4 Mat4::Scale(const Vec3& scale)
{
    Mat4 result = Mat4::Identity();

    result.elements[0] = scale.x;
    result.elements[5] = scale.y;
    result.elements[10] = scale.z;

    return result;
}

Mat4 Mat4::RotationX(float angle)
{
    Mat4 result = Mat4::Identity();

    float cosine = std::cos(angle);
    float sine = std::sin(angle);

    result.elements[5] = cosine;
    result.elements[6] = sine;
    result.elements[9] = -sine;
    result.elements[10] = cosine;

    return result;
}

Mat4 Mat4::RotationY(float angle)
{
    Mat4 result = Mat4::Identity();

    float cosine = std::cos(angle);
    float sine = std::sin(angle);

    result.elements[0] = cosine;
    result.elements[2] = -sine;
    result.elements[8] = sine;
    result.elements[10] = cosine;

    return result;
}

Mat4 Mat4::RotationZ(float angle)
{
    Mat4 result = Mat4::Identity();

    float cosine = std::cos(angle);
    float sine = std::sin(angle);

    result.elements[0] = cosine;
    result.elements[1] = sine;
    result.elements[4] = -sine;
    result.elements[5] = cosine;

    return result;
}

Mat4 Mat4::Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    Mat4 result = Mat4::Identity();
    result.elements[0] = 2.0f / (right - left);
    result.elements[5] = 2.0f / (top - bottom);
    result.elements[10] = -2.0f / (farPlane - nearPlane);
    result.elements[12] = -(right + left) / (right - left);
    result.elements[13] = -(top + bottom) / (top - bottom);
    result.elements[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    return result;
}

Mat4 Mat4::Perspective(
    float fov,
    float aspect,
    float nearPlane,
    float farPlane)
{
    Mat4 result;

    float tanHalfFov = std::tan(fov * 0.5f);

    result.elements[0] = 1.0f / tanHalfFov;
    result.elements[5] = aspect / tanHalfFov;

    result.elements[10] =
        (farPlane + nearPlane) /
        (nearPlane - farPlane);

    result.elements[11] = -1.0f;

    result.elements[14] =
        (2.0f * farPlane * nearPlane) /
        (nearPlane - farPlane);

    return result;
}

Mat4 Mat4::LookAt(
    const Vec3& position,
    const Vec3& target,
    const Vec3& up)
{
    Vec3 forward = (target - position).Normalized();
    Vec3 right = Vec3::Cross(forward, up).Normalized();
    Vec3 cameraUp = Vec3::Cross(right, forward);

    Mat4 result = Mat4::Identity();

    result.elements[0] = right.x;
    result.elements[1] = right.y;
    result.elements[2] = right.z;

    result.elements[4] = cameraUp.x;
    result.elements[5] = cameraUp.y;
    result.elements[6] = cameraUp.z;

    result.elements[8] = -forward.x;
    result.elements[9] = -forward.y;
    result.elements[10] = -forward.z;

    result.elements[12] = -Vec3::Dot(right, position);
    result.elements[13] = -Vec3::Dot(cameraUp, position);
    result.elements[14] = Vec3::Dot(forward, position);

    return result;
}

Mat4 Mat4::operator*(const Mat4& other) const
{
    Mat4 result;

    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.elements[column * 4 + row] =
                elements[0 * 4 + row] * other.elements[column * 4 + 0] +
                elements[1 * 4 + row] * other.elements[column * 4 + 1] +
                elements[2 * 4 + row] * other.elements[column * 4 + 2] +
                elements[3 * 4 + row] * other.elements[column * 4 + 3];
        }
    }

    return result;
}