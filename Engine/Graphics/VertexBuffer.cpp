#include "VertexBuffer.h"

#include <glad/gl.h>

VertexBuffer::VertexBuffer()
    : m_RendererID(0)
{
}

VertexBuffer::~VertexBuffer()
{
    Shutdown();
}

bool VertexBuffer::Initialize(const void* data, unsigned int size)
{
    glGenBuffers(1, &m_RendererID);

    if (m_RendererID == 0)
    {
        return false;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);

    glBufferData(
        GL_ARRAY_BUFFER,
        size,
        data,
        GL_STATIC_DRAW
    );

    return true;
}

void VertexBuffer::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
}

void VertexBuffer::Unbind()
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::Shutdown()
{
    if (m_RendererID != 0)
    {
        glDeleteBuffers(1, &m_RendererID);
        m_RendererID = 0;
    }
}