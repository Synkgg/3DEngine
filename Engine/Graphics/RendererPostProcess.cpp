#include "Renderer.h"
#include "RHI/RHI.h"
#include <Post.vert.h>
#include <Post.frag.h>
#include <TAA.frag.h>
#include <BloomExtract.frag.h>
#include <BloomBlur.frag.h>
#include <Sky.frag.h>
#include <Copy.frag.h>
#include <algorithm>
#include <cmath>
#include <cstring>
using namespace Velcryn::RHI;

bool Renderer::CreateFullscreenPipelines()
{
    auto* device=GetDevice();
    const float vertices[]={-1,-1, 3,-1, -1,3};const uint32_t indices[]={0,1,2};
    BufferDesc buffer{};buffer.size=sizeof(vertices);buffer.usage=BufferUsage::Vertex;
    m_FullscreenVertices=device->CreateBuffer(buffer,vertices);buffer.size=sizeof(indices);buffer.usage=BufferUsage::Index;
    m_FullscreenIndices=device->CreateBuffer(buffer,indices);
    const VertexAttribute attribute{0,0,VertexFormat::Float2};
    GraphicsPipelineDesc desc{};desc.vertexShader=Post_vert;desc.attributes=std::span(&attribute,1);desc.vertexStride=sizeof(float)*2;
    desc.depthFormat=TextureFormat::Unknown;desc.depthTest=false;desc.depthWrite=false;desc.cullBackFaces=false;
    desc.sampledTexture=true;desc.textureCount=4;desc.uniformSize=sizeof(PostDrawData);desc.clampSampler=true;
    auto make=[&](std::span<const uint32_t> shader,const char* name){desc.fragmentShader=shader;desc.debugName=name;return device->CreateGraphicsPipeline(desc);};
    m_PostPipeline=make(Post_frag,"HDRPostProcess");m_TAAPipeline=make(TAA_frag,"TemporalResolve");
    m_BloomExtractPipeline=make(BloomExtract_frag,"BloomExtract");m_BloomBlurPipeline=make(BloomBlur_frag,"BloomBlur");m_CopyPipeline=make(Copy_frag,"CopyResolvedColor");
    desc.sampleCount=m_Framebuffer.GetSamples();
    desc.depthFormat=TextureFormat::D32_Float;desc.depthTest=true;desc.depthLessEqual=true;desc.secondColor=true;
    m_SkyPipeline=make(Sky_frag,"ProceduralSky");
    return m_FullscreenVertices&&m_FullscreenIndices&&m_PostPipeline&&m_TAAPipeline&&m_BloomExtractPipeline&&m_BloomBlurPipeline&&m_CopyPipeline&&m_SkyPipeline;
}
void Renderer::DestroyFullscreenPipelines()
{
    if(auto* device=GetDevice()) {
        for(auto pipeline:{m_PostPipeline,m_TAAPipeline,m_BloomExtractPipeline,m_BloomBlurPipeline,m_CopyPipeline,m_SkyPipeline})device->DestroyPipeline(pipeline);
        device->DestroyBuffer(m_FullscreenVertices);device->DestroyBuffer(m_FullscreenIndices);
    }
    m_PostPipeline={};m_TAAPipeline={};m_BloomExtractPipeline={};m_BloomBlurPipeline={};m_CopyPipeline={};m_SkyPipeline={};
    m_FullscreenVertices={};m_FullscreenIndices={};
}
PostDrawData Renderer::GetPostDrawData() const
{
    PostDrawData data{};
    auto vector=[](float* target,Vec3 value){target[0]=value.x;target[1]=value.y;target[2]=value.z;};
    vector(data.cameraForward,m_Camera.GetForward());vector(data.cameraRight,m_Camera.GetRight());
    vector(data.cameraUp,Vec3::Cross(m_Camera.GetRight(),m_Camera.GetForward()).Normalized());vector(data.cameraPosition,m_Camera.GetPosition());
    vector(data.sunDirection,m_LightDirection);vector(data.sunColor,m_LightColor);data.sunColor[3]=m_LightIntensity;
    data.params0[0]=m_RenderSettings.exposure;data.params0[1]=m_RenderSettings.bloom?m_RenderSettings.bloomStrength:0;
    data.params0[2]=m_RenderSettings.atmosphereStrength;data.params0[3]=m_RenderSettings.skyIntensity;
    data.params1[0]=m_RenderSettings.colorSaturation;data.params1[1]=m_RenderSettings.contrast;
    data.params1[2]=m_RenderSettings.screenSpaceReflections?m_RenderSettings.screenSpaceReflectionStrength:0;data.params1[3]=m_RenderSettings.giStrength;
    data.params2[0]=1/m_Camera.GetProjectionMatrix().elements[5];data.params2[1]=float(m_ViewportWidth)/std::max(m_ViewportHeight,1u);
    data.params2[2]=m_Camera.GetNearPlane();data.params2[3]=m_Camera.GetFarPlane();
    data.params3[0]=float(m_DebugView);data.params3[1]=m_HistoryValid?1.f:0.f;
    std::memcpy(data.previousForward,m_PreviousPostData.cameraForward,16);std::memcpy(data.previousRight,m_PreviousPostData.cameraRight,16);
    std::memcpy(data.previousUp,m_PreviousPostData.cameraUp,16);std::memcpy(data.previousPosition,m_PreviousPostData.cameraPosition,16);
    std::memcpy(data.previousParams,m_PreviousPostData.params2,16);
    return data;
}
void Renderer::DrawFullscreen(PipelineHandle pipeline,const PostDrawData& data,TextureHandle first,std::span<const TextureHandle> others)
{
    if(auto* device=GetDevice())device->DrawIndexed(pipeline,m_FullscreenVertices,m_FullscreenIndices,3,nullptr,0,1,first,std::as_bytes(std::span{&data,1}),others);
}
bool Renderer::CreatePostProcessTarget(){DestroyPostProcessTarget();auto*d=Velcryn::RHI::GetDevice();if(!d||!m_ViewportWidth||!m_ViewportHeight)return false;auto mk=[&](const char*n){Velcryn::RHI::TextureDesc x{};x.width=m_ViewportWidth;x.height=m_ViewportHeight;x.format=Velcryn::RHI::TextureFormat::RGBA16_Float;x.usage=Velcryn::RHI::TextureUsage::RenderTarget|Velcryn::RHI::TextureUsage::Sampled|Velcryn::RHI::TextureUsage::TransferSource|Velcryn::RHI::TextureUsage::TransferDestination;x.debugName=n;return d->CreateTexture(x);};m_PostColorTexture=mk("PostColor");m_BloomTexture[0]=mk("BloomPing");m_BloomTexture[1]=mk("BloomPong");m_HistoryTexture[0]=mk("TAA0");m_HistoryTexture[1]=mk("TAA1");m_HistoryReadIndex=0;m_HistoryValid=false;return m_PostColorTexture&&m_BloomTexture[0]&&m_BloomTexture[1]&&m_HistoryTexture[0]&&m_HistoryTexture[1];}
void Renderer::DestroyPostProcessTarget(){if(auto*d=Velcryn::RHI::GetDevice()){if(m_PostColorTexture)d->DestroyTexture(m_PostColorTexture);for(auto&t:m_BloomTexture)if(t)d->DestroyTexture(t);for(auto&t:m_HistoryTexture)if(t)d->DestroyTexture(t);}m_PostColorTexture={};for(auto&t:m_BloomTexture)t={};for(auto&t:m_HistoryTexture)t={};m_HistoryReadIndex=0;m_HistoryValid=false;}

TextureHandle Renderer::RenderBloom()
{
    if(!m_RenderSettings.bloom)return m_WhiteTexture;
    auto* device=GetDevice();PostDrawData data=GetPostDrawData();
    const TextureHandle fallback[]={m_WhiteTexture,m_WhiteTexture,m_WhiteTexture};
    device->BeginRendering(m_BloomTexture[0],{},m_ClearColor);
    DrawFullscreen(m_BloomExtractPipeline,data,m_Framebuffer.GetColorTexture(),fallback);device->EndRendering();
    TextureHandle source=m_BloomTexture[0];
    for(int pass=0;pass<6;++pass) {
        const int target=(pass+1)%2;data.params3[2]=(pass%2)==0?1.f:0.f;
        device->BeginRendering(m_BloomTexture[target],{},m_ClearColor);
        DrawFullscreen(m_BloomBlurPipeline,data,source,fallback);device->EndRendering();source=m_BloomTexture[target];
    }
    return source;
}
void Renderer::RenderPostProcess()
{
    auto* device=GetDevice();if(!device||!m_PostPipeline)return;
    const TextureHandle bloom=RenderBloom();const auto data=GetPostDrawData();
    const TextureHandle textures[]={bloom,m_Framebuffer.GetDepthTexture(),m_Framebuffer.GetNormalTexture()};
    device->BeginRendering(m_PostColorTexture,{},m_ClearColor);
    DrawFullscreen(m_PostPipeline,data,m_Framebuffer.GetColorTexture(),textures);device->EndRendering();
    if(m_RenderSettings.antiAliasing&&m_DebugView==RenderDebugView::Lit)ResolveTAA();else m_HistoryValid=false;
}
void Renderer::ResolveTAA()
{
    auto* device=GetDevice();const int writeIndex=1-m_HistoryReadIndex;auto data=GetPostDrawData();
    const TextureHandle textures[]={m_HistoryValid?m_HistoryTexture[m_HistoryReadIndex]:m_PostColorTexture,m_Framebuffer.GetDepthTexture(),m_Framebuffer.GetNormalTexture()};
    device->BeginRendering(m_HistoryTexture[writeIndex],{},m_ClearColor);
    DrawFullscreen(m_TAAPipeline,data,m_PostColorTexture,textures);device->EndRendering();
    const TextureHandle fallback[]={m_WhiteTexture,m_WhiteTexture,m_WhiteTexture};
    device->BeginRendering(m_PostColorTexture,{},m_ClearColor);
    DrawFullscreen(m_CopyPipeline,data,m_HistoryTexture[writeIndex],fallback);device->EndRendering();
    m_HistoryReadIndex=writeIndex;m_PreviousPostData=data;m_HistoryValid=true;
}
