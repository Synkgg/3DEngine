#pragma once
#include <cstdint>
#include <vector>
#include "../Core/VertexBuffer.h"
#include "../Core/IndexBuffer.h"
#include "../RHI/RHITypes.h"
struct BoneWeight;struct Vertex{float position[3];float normal[3];float uv[2];};class Mesh{public:Mesh()=default;~Mesh();Mesh(const Mesh&)=delete;Mesh&operator=(const Mesh&)=delete;Mesh(const std::vector<Vertex>&,const std::vector<std::uint32_t>&);void Bind();void Unbind();std::uint32_t GetIndexCount()const;const std::vector<Vertex>&GetVertices()const;const std::vector<std::uint32_t>&GetIndices()const;void SetSkinWeights(const std::vector<BoneWeight>&);Velcryn::RHI::BufferHandle GetVertexBuffer()const{return m_VertexBuffer.GetHandle();}Velcryn::RHI::BufferHandle GetIndexBuffer()const{return m_IndexBuffer.GetHandle();}private:std::vector<Vertex>m_Vertices;std::vector<std::uint32_t>m_Indices;VertexBuffer m_VertexBuffer;IndexBuffer m_IndexBuffer;Velcryn::RHI::BufferHandle m_SkinBuffer{};};