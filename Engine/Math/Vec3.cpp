#include "Vec3.h"

#include <cmath>

Vec3::Vec3()
    : x(0.0f),
    y(0.0f),
    z(0.0f)
{
}

Vec3::Vec3(float x, float y, float z)
    : x(x),
    y(y),
    z(z)
{
}

float Vec3::Length() const
{
    return std::sqrt(
        x * x +
        y * y +
        z * z
    );
}

Vec3 Vec3::Normalized() const
{
    float length = Length();

    if (length == 0.0f)
    {
        return Vec3();
    }

    return Vec3(
        x / length,
        y / length,
        z / length
    );
}

float Vec3::Dot(const Vec3& a, const Vec3& b)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

Vec3 Vec3::Cross(const Vec3& a, const Vec3& b)
{
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

Vec3 Vec3::operator-(const Vec3& other) const
{
    return Vec3(
        x - other.x,
        y - other.y,
        z - other.z
    );
}

Vec3 Vec3::operator+(const Vec3& other) const
{
    return Vec3(
        x + other.x,
        y + other.y,
        z + other.z
    );
}

Vec3 Vec3::operator*(float scalar) const
{
    return Vec3(
        x * scalar,
        y * scalar,
        z * scalar
    );
}

Vec3 Vec3::operator/(float scalar) const
{
    if (scalar == 0.0f)
    {
        return Vec3();
    }

    return Vec3(
        x / scalar,
        y / scalar,
        z / scalar
    );
}