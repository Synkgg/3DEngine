#include "Grid.h"

#include <glad/gl.h>
#include <vector>

Grid::Grid()
    : m_VertexArray(0),
    m_VertexBuffer(0),
    m_VertexCount(0)
{
}

Grid::~Grid()
{
    Shutdown();
}

bool Grid::Initialize()
{
    std::vector<float> vertices;

    constexpr int gridSize = 10;

    for (int i = -gridSize; i <= gridSize; ++i)
    {
        // Lines parallel to Z.
        vertices.push_back(static_cast<float>(i));
        vertices.push_back(0.0f);
        vertices.push_back(-static_cast<float>(gridSize));

        vertices.push_back(static_cast<float>(i));
        vertices.push_back(0.0f);
        vertices.push_back(static_cast<float>(gridSize));

        // Lines parallel to X.
        vertices.push_back(-static_cast<float>(gridSize));
        vertices.push_back(0.0f);
        vertices.push_back(static_cast<float>(i));

        vertices.push_back(static_cast<float>(gridSize));
        vertices.push_back(0.0f);
        vertices.push_back(static_cast<float>(i));
    }

    m_VertexCount = static_cast<unsigned int>(vertices.size() / 3);

    glGenVertexArrays(1, &m_VertexArray);
    glGenBuffers(1, &m_VertexBuffer);

    if (m_VertexArray == 0 || m_VertexBuffer == 0)
    {
        Shutdown();
        return false;
    }

    glBindVertexArray(m_VertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            vertices.size() * sizeof(float)
            ),
        vertices.data(),
        GL_STATIC_DRAW
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

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void Grid::Shutdown()
{
    if (m_VertexBuffer != 0)
    {
        glDeleteBuffers(1, &m_VertexBuffer);
        m_VertexBuffer = 0;
    }

    if (m_VertexArray != 0)
    {
        glDeleteVertexArrays(1, &m_VertexArray);
        m_VertexArray = 0;
    }

    m_VertexCount = 0;
}

void Grid::Draw()
{
    glBindVertexArray(m_VertexArray);

    glDrawArrays(
        GL_LINES,
        0,
        static_cast<GLsizei>(m_VertexCount)
    );

    glBindVertexArray(0);
}