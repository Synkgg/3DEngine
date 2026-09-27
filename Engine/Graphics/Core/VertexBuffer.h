#pragma once

class VertexBuffer
{
public:
    VertexBuffer();
    ~VertexBuffer();

    bool Initialize(const void* data, unsigned int size);
    void Bind();
    void Unbind();
    void Shutdown();

private:
    unsigned int m_RendererID;
};