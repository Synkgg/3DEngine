#include "DebugRenderer.h"

#include <glad/gl.h>

#include <cmath>
#include <vector>

namespace
{
    static const char* vertexShaderSource = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;

        uniform mat4 u_Transform;

        void main()
        {
            gl_Position =
                u_Transform *
                vec4(a_Position, 1.0);
        }
    )";

    static const char* fragmentShaderSource = R"(
        #version 450 core

        uniform vec4 u_Color;

        out vec4 FragColor;

        void main()
        {
            FragColor = u_Color;
        }
    )";

    unsigned int CompileShader(
        unsigned int type,
        const char* source)
    {
        unsigned int shader =
            glCreateShader(type);

        glShaderSource(
            shader,
            1,
            &source,
            nullptr
        );

        glCompileShader(shader);

        int success = 0;

        glGetShaderiv(
            shader,
            GL_COMPILE_STATUS,
            &success
        );

        if (!success)
        {
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }
}

DebugRenderer::DebugRenderer()
    : m_VertexArray(0),
    m_VertexBuffer(0),
    m_ShaderProgram(0)
{
}

DebugRenderer::~DebugRenderer()
{
    Shutdown();
}

bool DebugRenderer::Initialize()
{
    unsigned int vertexShader =
        CompileShader(
            GL_VERTEX_SHADER,
            vertexShaderSource
        );

    if (vertexShader == 0)
    {
        return false;
    }

    unsigned int fragmentShader =
        CompileShader(
            GL_FRAGMENT_SHADER,
            fragmentShaderSource
        );

    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return false;
    }

    m_ShaderProgram =
        glCreateProgram();

    glAttachShader(
        m_ShaderProgram,
        vertexShader
    );

    glAttachShader(
        m_ShaderProgram,
        fragmentShader
    );

    glLinkProgram(
        m_ShaderProgram
    );

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int linked = 0;

    glGetProgramiv(
        m_ShaderProgram,
        GL_LINK_STATUS,
        &linked
    );

    if (!linked)
    {
        glDeleteProgram(
            m_ShaderProgram
        );

        m_ShaderProgram = 0;

        return false;
    }

    glGenVertexArrays(
        1,
        &m_VertexArray
    );

    glGenBuffers(
        1,
        &m_VertexBuffer
    );

    if (m_VertexArray == 0 ||
        m_VertexBuffer == 0)
    {
        Shutdown();
        return false;
    }

    glBindVertexArray(
        m_VertexArray
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VertexBuffer
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float) * 72,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        nullptr
    );

    glEnableVertexAttribArray(0);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        0
    );

    glBindVertexArray(0);

    return true;
}

void DebugRenderer::Shutdown()
{
    if (m_VertexBuffer != 0)
    {
        glDeleteBuffers(
            1,
            &m_VertexBuffer
        );

        m_VertexBuffer = 0;
    }

    if (m_VertexArray != 0)
    {
        glDeleteVertexArrays(
            1,
            &m_VertexArray
        );

        m_VertexArray = 0;
    }

    if (m_ShaderProgram != 0)
    {
        glDeleteProgram(
            m_ShaderProgram
        );

        m_ShaderProgram = 0;
    }
}

void DebugRenderer::DrawDirectionalLight(
    const Mat4& view,
    const Mat4& projection,
    const Vec3& position,
    const Vec3& direction)
{
    if (m_VertexArray == 0 ||
        m_VertexBuffer == 0 ||
        m_ShaderProgram == 0)
    {
        return;
    }

    Vec3 normalizedDirection =
        direction.Normalized();

    if (normalizedDirection.Length() == 0.0f)
    {
        return;
    }

    constexpr float length = 2.5f;
    constexpr float arrowSize = 0.25f;

    Vec3 end =
        position +
        normalizedDirection * length;

    Vec3 up(0.0f, 1.0f, 0.0f);

    Vec3 side =
        Vec3::Cross(
            normalizedDirection,
            up
        );

    if (side.Length() < 0.001f)
    {
        up =
            Vec3(
                1.0f,
                0.0f,
                0.0f
            );

        side =
            Vec3::Cross(
                normalizedDirection,
                up
            );
    }

    side =
        side.Normalized();

    Vec3 arrowUp =
        Vec3::Cross(
            side,
            normalizedDirection
        ).Normalized();

    Vec3 arrowLeft =
        end -
        normalizedDirection * arrowSize +
        side * arrowSize;

    Vec3 arrowRight =
        end -
        normalizedDirection * arrowSize -
        side * arrowSize;

    Vec3 arrowTop =
        end -
        normalizedDirection * arrowSize +
        arrowUp * arrowSize;

    Vec3 arrowBottom =
        end -
        normalizedDirection * arrowSize -
        arrowUp * arrowSize;

    float vertices[] =
    {
        position.x, position.y, position.z,
        end.x,      end.y,      end.z,

        end.x,        end.y,        end.z,
        arrowLeft.x,  arrowLeft.y,  arrowLeft.z,

        end.x,        end.y,        end.z,
        arrowRight.x, arrowRight.y, arrowRight.z,

        end.x,       end.y,       end.z,
        arrowTop.x,  arrowTop.y,  arrowTop.z,

        end.x,          end.y,          end.z,
        arrowBottom.x,  arrowBottom.y,  arrowBottom.z
    };

    Mat4 transform =
        projection * view;

    glUseProgram(
        m_ShaderProgram
    );

    GLint transformLocation =
        glGetUniformLocation(
            m_ShaderProgram,
            "u_Transform"
        );

    GLint colorLocation =
        glGetUniformLocation(
            m_ShaderProgram,
            "u_Color"
        );

    glUniformMatrix4fv(
        transformLocation,
        1,
        GL_FALSE,
        transform.elements
    );

    // Yellow editor/debug color.
    glUniform4f(
        colorLocation,
        1.0f,
        0.75f,
        0.1f,
        1.0f
    );

    glBindVertexArray(
        m_VertexArray
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VertexBuffer
    );

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );

    // Make the light indicator visible
    // even when geometry is in front of it.
    glDisable(GL_DEPTH_TEST);

    glLineWidth(3.0f);

    glDrawArrays(
        GL_LINES,
        0,
        10
    );

    glEnable(GL_DEPTH_TEST);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        0
    );

    glBindVertexArray(0);
}

void DebugRenderer::DrawBox(
    const Mat4& view,
    const Mat4& projection,
    const Vec3& center,
    const Vec3& halfExtents,
    const Vec3& rotation)
{
    if (m_VertexArray == 0 ||
        m_VertexBuffer == 0 ||
        m_ShaderProgram == 0)
    {
        return;
    }

    const float cx = std::cos(rotation.x);
    const float sx = std::sin(rotation.x);

    const float cy = std::cos(rotation.y);
    const float sy = std::sin(rotation.y);

    const float cz = std::cos(rotation.z);
    const float sz = std::sin(rotation.z);

    auto RotateVector =
        [&](const Vec3& vector) -> Vec3
        {
            // X rotation
            Vec3 result(
                vector.x,
                vector.y * cx - vector.z * sx,
                vector.y * sx + vector.z * cx
            );

            // Y rotation
            result =
                Vec3(
                    result.x * cy + result.z * sy,
                    result.y,
                    -result.x * sy + result.z * cy
                );

            // Z rotation
            result =
                Vec3(
                    result.x * cz - result.y * sz,
                    result.x * sz + result.y * cz,
                    result.z
                );

            return result;
        };

    const Vec3 localCorners[8] =
    {
        Vec3(
            -halfExtents.x,
            -halfExtents.y,
            -halfExtents.z
        ),

        Vec3(
            halfExtents.x,
            -halfExtents.y,
            -halfExtents.z
        ),

        Vec3(
            halfExtents.x,
            halfExtents.y,
            -halfExtents.z
        ),

        Vec3(
            -halfExtents.x,
            halfExtents.y,
            -halfExtents.z
        ),

        Vec3(
            -halfExtents.x,
            -halfExtents.y,
            halfExtents.z
        ),

        Vec3(
            halfExtents.x,
            -halfExtents.y,
            halfExtents.z
        ),

        Vec3(
            halfExtents.x,
            halfExtents.y,
            halfExtents.z
        ),

        Vec3(
            -halfExtents.x,
            halfExtents.y,
            halfExtents.z
        )
    };

    Vec3 corners[8];

    for (int i = 0; i < 8; ++i)
    {
        corners[i] =
            center +
            RotateVector(localCorners[i]);
    }

    const unsigned int edges[24] =
    {
        // Bottom
        0, 1,
        1, 2,
        2, 3,
        3, 0,

        // Top
        4, 5,
        5, 6,
        6, 7,
        7, 4,

        // Vertical
        0, 4,
        1, 5,
        2, 6,
        3, 7
    };

    float vertices[72];

    for (int i = 0; i < 24; ++i)
    {
        const Vec3& vertex =
            corners[edges[i]];

        vertices[i * 3 + 0] =
            vertex.x;

        vertices[i * 3 + 1] =
            vertex.y;

        vertices[i * 3 + 2] =
            vertex.z;
    }

    Mat4 transform =
        projection * view;

    glUseProgram(
        m_ShaderProgram
    );

    GLint transformLocation =
        glGetUniformLocation(
            m_ShaderProgram,
            "u_Transform"
        );

    GLint colorLocation =
        glGetUniformLocation(
            m_ShaderProgram,
            "u_Color"
        );

    glUniformMatrix4fv(
        transformLocation,
        1,
        GL_FALSE,
        transform.elements
    );

    glUniform4f(
        colorLocation,
        0.1f,
        1.0f,
        0.2f,
        1.0f
    );

    glBindVertexArray(
        m_VertexArray
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VertexBuffer
    );

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );

    glDisable(GL_DEPTH_TEST);

    glLineWidth(2.0f);

    glDrawArrays(
        GL_LINES,
        0,
        24
    );

    glEnable(GL_DEPTH_TEST);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        0
    );

    glBindVertexArray(0);
}