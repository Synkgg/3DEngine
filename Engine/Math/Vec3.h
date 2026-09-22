#pragma once

class Vec3
{
public:
    float x;
    float y;
    float z;

    Vec3();
    Vec3(float x, float y, float z);

    float Length() const;
    Vec3 Normalized() const;

    static float Dot(const Vec3& a, const Vec3& b);
    static Vec3 Cross(const Vec3& a, const Vec3& b);

    Vec3 operator-(const Vec3& other) const;
    Vec3 operator+(const Vec3& other) const;
    Vec3 operator*(float scalar) const;
    Vec3 operator/(float scalar) const;
};