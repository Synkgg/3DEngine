#include "Grid.h"
#include "RHI/RHI.h"
#include <vector>
Grid::Grid()=default;Grid::~Grid(){Shutdown();}bool Grid::Initialize(){std::vector<float>v;for(int i=-20;i<=20;++i){float f=(float)i;v.insert(v.end(),{f,0,-20,f,0,20,-20,0,f,20,0,f});}m_VertexCount=(unsigned)v.size()/3;auto*d=Velcryn::RHI::GetDevice();if(!d)return false;Velcryn::RHI::BufferDesc b{};b.size=v.size()*sizeof(float);b.usage=Velcryn::RHI::BufferUsage::Vertex;b.debugName="EditorGrid";m_VertexBuffer=d->CreateBuffer(b,v.data());return(bool)m_VertexBuffer;}void Grid::Shutdown(){if(m_VertexBuffer)if(auto*d=Velcryn::RHI::GetDevice())d->DestroyBuffer(m_VertexBuffer);m_VertexBuffer={};}void Grid::Draw(){}
