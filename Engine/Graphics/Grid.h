#pragma once

class Grid
{
public:
    Grid();
    ~Grid();

    bool Initialize();
    void Shutdown();

    void Draw();

private:
    unsigned int m_VertexArray;
    unsigned int m_VertexBuffer;
    unsigned int m_VertexCount;
};