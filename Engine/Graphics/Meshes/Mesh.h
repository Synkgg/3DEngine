#pragma once

#include <cstdint>
#include <vector>

#include "../Core/VertexArray.h"
#include "../Core/VertexBuffer.h"
#include "../Core/IndexBuffer.h"

struct BoneWeight;

struct Vertex
{
    float position[3];
    float normal[3];
    float uv[2];
};

class Mesh
{
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices
    );

    void Bind();
    void Unbind();

    std::uint32_t GetIndexCount() const;

    const std::vector<Vertex>& GetVertices() const;
    const std::vector<std::uint32_t>& GetIndices() const;
    void SetSkinWeights(const std::vector<BoneWeight>& weights);

private:
    std::vector<Vertex> m_Vertices;
    std::vector<std::uint32_t> m_Indices;

    VertexArray m_VertexArray;
    VertexBuffer m_VertexBuffer;
    IndexBuffer m_IndexBuffer;
    unsigned int m_SkinBuffer = 0;
};