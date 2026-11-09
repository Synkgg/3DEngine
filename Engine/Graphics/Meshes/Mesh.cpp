#include "Mesh.h"
#include "../ModelAsset.h"
#include "../RHI/RHI.h"
Mesh::Mesh(const std::vector<Vertex>& vertices,const std::vector<std::uint32_t>& indices):m_Vertices(vertices),m_Indices(indices)
{
    std::vector<MeshVertex> gpu(vertices.size());
    for(size_t i=0;i<vertices.size();++i)gpu[i].vertex=vertices[i];
    m_VertexBuffer.Initialize(gpu.data(),unsigned(gpu.size()*sizeof(MeshVertex)));
    m_IndexBuffer.Initialize(indices.data(),uint32_t(indices.size()));
}
Mesh::~Mesh()=default;
void Mesh::Bind(){} void Mesh::Unbind(){}
std::uint32_t Mesh::GetIndexCount()const{return m_IndexBuffer.GetCount();}
const std::vector<Vertex>& Mesh::GetVertices()const{return m_Vertices;}
const std::vector<uint32_t>& Mesh::GetIndices()const{return m_Indices;}
void Mesh::SetSkinWeights(const std::vector<BoneWeight>& weights)
{
    if(weights.size()!=m_Vertices.size())return;
    std::vector<MeshVertex> gpu(weights.size());
    for(size_t i=0;i<weights.size();++i){gpu[i].vertex=m_Vertices[i];for(int j=0;j<4;++j){gpu[i].joints[j]=float(weights[i].joints[j]);gpu[i].weights[j]=weights[i].weights[j];}}
    m_VertexBuffer.Initialize(gpu.data(),unsigned(gpu.size()*sizeof(MeshVertex)));
}
