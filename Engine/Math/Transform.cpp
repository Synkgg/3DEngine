#include "Transform.h"

Transform::Transform()
    : position(0.0f, 0.0f, 0.0f),
    rotation(0.0f, 0.0f, 0.0f),
    scale(1.0f, 1.0f, 1.0f)
{
}

Mat4 Transform::GetMatrix() const
{
    return Mat4::Translation(position)
        * Mat4::RotationZ(rotation.z)
        * Mat4::RotationY(rotation.y)
        * Mat4::RotationX(rotation.x)
        * Mat4::Scale(scale);
}