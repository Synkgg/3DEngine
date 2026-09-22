#pragma once

#include "Vec3.h"

class Mat4
{
public:
    float elements[16];

    Mat4();

    static Mat4 Identity();
    static Mat4 Translation(const Vec3& position);
    static Mat4 Scale(const Vec3& scale);

    static Mat4 RotationX(float angle);
    static Mat4 RotationY(float angle);
    static Mat4 RotationZ(float angle);

    static Mat4 Perspective(
        float fov,
        float aspect,
        float nearPlane,
        float farPlane
    );

    static Mat4 LookAt(
        const Vec3& position,
        const Vec3& target,
        const Vec3& up
    );

    Mat4 operator*(const Mat4& other) const;
};