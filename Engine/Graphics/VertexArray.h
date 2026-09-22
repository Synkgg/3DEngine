#pragma once

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();

    bool Initialize();

    void Bind();
    void Unbind();
    void Shutdown();

private:
    unsigned int m_RendererID;
};