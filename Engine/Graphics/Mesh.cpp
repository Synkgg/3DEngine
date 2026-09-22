#include "Mesh.h"

#include <glad/gl.h>
#include <cstddef>

Mesh::Mesh(
    const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
    : m_Vertices(vertices),
    m_Indices(indices)
{
    m_VertexArray.Initialize();
    m_VertexArray.Bind();

    m_VertexBuffer.Initialize(
        m_Vertices.data(),
        static_cast<unsigned int>(
            m_Vertices.size() * sizeof(Vertex)
            )
    );

    m_IndexBuffer.Initialize(
        m_Indices.data(),
        static_cast<std::uint32_t>(
            m_Indices.size()
            )
    );

    m_VertexBuffer.Bind();

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, position)
            )
    );

    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, normal)
            )
    );

    glEnableVertexAttribArray(2);

    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(
            offsetof(Vertex, uv)
            )
    );

    m_VertexArray.Unbind();
}

void Mesh::Bind()
{
    m_VertexArray.Bind();
}

void Mesh::Unbind()
{
    m_VertexArray.Unbind();
}

std::uint32_t Mesh::GetIndexCount() const
{
    return m_IndexBuffer.GetCount();
}

const std::vector<Vertex>& Mesh::GetVertices() const
{
    return m_Vertices;
}

const std::vector<std::uint32_t>& Mesh::GetIndices() const
{
    return m_Indices;
}