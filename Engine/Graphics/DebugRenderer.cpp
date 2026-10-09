#include "DebugRenderer.h"
#include "RHI/RHI.h"
#include "../Math/Transform.h"
#include <Debug.vert.h>
#include <Debug.frag.h>
#include <cmath>
using namespace Velcryn::RHI;
DebugRenderer::DebugRenderer()=default;
DebugRenderer::~DebugRenderer(){Shutdown();}
bool DebugRenderer::Initialize(unsigned int samples){
    auto* d=GetDevice();if(!d)return false;
    const float v[]={0,0,1,0};const uint32_t i[]={0,1};BufferDesc b{};b.size=sizeof(v);b.usage=BufferUsage::Vertex;m_VertexBuffer=d->CreateBuffer(b,v);
    b.size=sizeof(i);b.usage=BufferUsage::Index;m_IndexBuffer=d->CreateBuffer(b,i);
    const VertexAttribute a{0,0,VertexFormat::Float2};GraphicsPipelineDesc p{}; p.sampleCount=samples;p.vertexShader=Debug_vert;p.fragmentShader=Debug_frag;p.attributes=std::span(&a,1);p.vertexStride=8;p.constantSize=112;p.lineList=true;p.depthWrite=false;p.cullBackFaces=false;p.debugName="DebugLines";m_Pipeline=d->CreateGraphicsPipeline(p);
    return m_VertexBuffer&&m_IndexBuffer&&m_Pipeline;
}
void DebugRenderer::Shutdown(){if(auto*d=GetDevice()){d->DestroyBuffer(m_VertexBuffer);d->DestroyBuffer(m_IndexBuffer);d->DestroyPipeline(m_Pipeline);}m_VertexBuffer={};m_IndexBuffer={};m_Pipeline={};}
void DebugRenderer::DrawLine(const Mat4& view,const Mat4& projection,const Vec3& a,const Vec3& b,const Vec3& color){
    struct Constants{Mat4 vp;float color[4],start[4],end[4];};const Constants c{projection*view,{color.x,color.y,color.z,1},{a.x,a.y,a.z,1},{b.x,b.y,b.z,1}};
    if(auto*d=GetDevice())d->DrawIndexed(m_Pipeline,m_VertexBuffer,m_IndexBuffer,2,&c,sizeof(c));
}
void DebugRenderer::DrawDirectionalLight(const Mat4& view,const Mat4& projection,const Vec3& position,const Vec3& direction){
    if(direction.Length()<0.001f)return;const Vec3 n=direction.Normalized(),end=position+n*2.5f;
    const Vec3 helper=std::abs(n.y)>.99f?Vec3(1,0,0):Vec3(0,1,0);const Vec3 side=Vec3::Cross(n,helper).Normalized(),up=Vec3::Cross(side,n).Normalized(),color(1,.75f,.1f);
    DrawLine(view,projection,position,end,color);
    for(auto axis:{side,side*-1.f,up,up*-1.f})DrawLine(view,projection,end,end-n*.25f+axis*.25f,color);
}
void DebugRenderer::DrawBox(const Mat4& view,const Mat4& projection,const Vec3& position,const Vec3& half,const Vec3& rotation){
    Transform t;t.position=position;t.rotation=rotation;const Mat4 m=t.GetMatrix();Vec3 corners[8];
    for(int i=0;i<8;++i){Vec3 v((i&1)?half.x:-half.x,(i&2)?half.y:-half.y,(i&4)?half.z:-half.z);corners[i]=Vec3(m.elements[0]*v.x+m.elements[4]*v.y+m.elements[8]*v.z+m.elements[12],m.elements[1]*v.x+m.elements[5]*v.y+m.elements[9]*v.z+m.elements[13],m.elements[2]*v.x+m.elements[6]*v.y+m.elements[10]*v.z+m.elements[14]);}
    for(int i=0;i<8;++i)for(int axis:{1,2,4})if(!(i&axis))DrawLine(view,projection,corners[i],corners[i|axis],Vec3(.2f,1,.35f));
}
