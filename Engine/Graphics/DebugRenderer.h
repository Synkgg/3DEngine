#pragma once

#include "../Math/Vec3.h"
#include "../Math/Mat4.h"

class DebugRenderer
{
public:
    DebugRenderer();
    ~DebugRenderer();

    bool Initialize();
    void Shutdown();

    void DrawDirectionalLight(
        const Mat4& view,
        const Mat4& projection,
        const Vec3& position,
        const Vec3& direction
    );

    void DrawBox(
        const Mat4& view,
        const Mat4& projection,
        const Vec3& center,
        const Vec3& halfExtents,
        const Vec3& rotation
    );

private:
    unsigned int m_VertexArray;
    unsigned int m_VertexBuffer;
    unsigned int m_ShaderProgram;
};