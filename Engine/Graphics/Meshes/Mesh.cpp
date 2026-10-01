#include "Mesh.h"
#include "../ModelAsset.h"

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
void Mesh::SetSkinWeights(const std::vector<BoneWeight>& weights)
{
    if(weights.size()!=m_Vertices.size()||weights.empty()) return;
    struct GPUWeight{std::uint32_t joints[4];float weights[4];};
    std::vector<GPUWeight> gpu(weights.size());
    for(std::size_t i=0;i<weights.size();++i)for(int k=0;k<4;++k){gpu[i].joints[k]=weights[i].joints[k];gpu[i].weights[k]=weights[i].weights[k];}
    m_VertexArray.Bind();
    unsigned int buffer=0;glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,gpu.size()*sizeof(GPUWeight),gpu.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(3);glVertexAttribIPointer(3,4,GL_UNSIGNED_INT,sizeof(GPUWeight),(void*)offsetof(GPUWeight,joints));
    glEnableVertexAttribArray(4);glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(GPUWeight),(void*)offsetof(GPUWeight,weights));
    m_SkinBuffer=buffer;m_VertexArray.Unbind();
}
