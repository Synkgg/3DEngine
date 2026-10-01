#include "IndexBuffer.h"

#include <glad/gl.h>

IndexBuffer::IndexBuffer()
    : m_RendererID(0),
    m_Count(0)
{
}

IndexBuffer::~IndexBuffer()
{
    Shutdown();
}

bool IndexBuffer::Initialize(
    const std::uint32_t* indices,
    std::uint32_t count)
{
    if (indices == nullptr || count == 0)
    {
        return false;
    }

    glGenBuffers(1, &m_RendererID);

    if (m_RendererID == 0)
    {
        return false;
    }

    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        m_RendererID
    );

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            count * sizeof(std::uint32_t)
            ),
        indices,
        GL_STATIC_DRAW
    );

    m_Count = count;

    return true;
}

void IndexBuffer::Bind()
{
    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        m_RendererID
    );
}

void IndexBuffer::Unbind()
{
    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        0
    );
}

void IndexBuffer::Shutdown()
{
    if (m_RendererID != 0)
    {
        glDeleteBuffers(
            1,
            &m_RendererID
        );

        m_RendererID = 0;
        m_Count = 0;
    }
}

std::uint32_t IndexBuffer::GetCount() const
{
    return m_Count;
}