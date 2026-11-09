#include "Framebuffer.h"
#include "RHI/RHI.h"
#include <algorithm>
#include "../Core/Logger.h"

Framebuffer::Framebuffer() = default;
Framebuffer::~Framebuffer() { Shutdown(); }

bool Framebuffer::Initialize(unsigned int width, unsigned int height, unsigned int samples, bool hdr)
{
    m_Width=width; m_Height=height; m_Samples=std::clamp(samples,1u,8u); m_HDR=hdr;
    return CreateTargets();
}

bool Framebuffer::CreateTargets()
{
    auto* device=Velcryn::RHI::GetDevice();
    if(!device || !m_Width || !m_Height) return false;


    Velcryn::RHI::TextureDesc color{};
    color.width=m_Width; color.height=m_Height;
    color.format=m_HDR?Velcryn::RHI::TextureFormat::RGBA16_Float:Velcryn::RHI::TextureFormat::RGBA8_UNorm;
    color.usage=Velcryn::RHI::TextureUsage::RenderTarget|Velcryn::RHI::TextureUsage::Sampled|
                Velcryn::RHI::TextureUsage::TransferSource|Velcryn::RHI::TextureUsage::TransferDestination;
    color.debugName="SceneColor";
    m_Color=device->CreateTexture(color);

    Velcryn::RHI::TextureDesc normal=color;
    normal.format=Velcryn::RHI::TextureFormat::RGBA16_Float;
    normal.debugName="SceneNormalRoughness";
    m_Normal=device->CreateTexture(normal);

    Velcryn::RHI::TextureDesc depth{};
    depth.width=m_Width; depth.height=m_Height;
    depth.format=Velcryn::RHI::TextureFormat::D32_Float;
    depth.usage=Velcryn::RHI::TextureUsage::DepthStencil|Velcryn::RHI::TextureUsage::Sampled;
    depth.debugName="SceneDepth";
    m_Depth=device->CreateTexture(depth);

    if(m_Samples>1) {
        color.sampleCount=normal.sampleCount=depth.sampleCount=m_Samples;
        m_MSColor=device->CreateTexture(color);m_MSNormal=device->CreateTexture(normal);m_MSDepth=device->CreateTexture(depth);
        if(!m_MSColor||!m_MSNormal||!m_MSDepth){Shutdown();return false;}
    }
    if(!m_Color || !m_Normal || !m_Depth){Shutdown();return false;}
    return true;
}

void Framebuffer::Bind(){}
void Framebuffer::Unbind(){}
void Framebuffer::Resolve(){}

bool Framebuffer::Resize(unsigned int width,unsigned int height)
{
    if(!width||!height)return false;
    if(width==m_Width&&height==m_Height)return true;
    Shutdown();m_Width=width;m_Height=height;return CreateTargets();
}

bool Framebuffer::SetSamples(unsigned int samples)
{
    samples=samples>=8?8:samples>=4?4:samples>=2?2:1;
    if(samples==m_Samples)return true;
    Shutdown();m_Samples=samples;return CreateTargets();
}

void Framebuffer::Shutdown()
{
    if(auto* device=Velcryn::RHI::GetDevice())
    {
        for(auto handle:{m_MSColor,m_MSNormal,m_MSDepth})if(handle)device->DestroyTexture(handle);
        if(m_Color)device->DestroyTexture(m_Color);
        if(m_Normal)device->DestroyTexture(m_Normal);
        if(m_Depth)device->DestroyTexture(m_Depth);
    }
    m_Color={};m_Normal={};m_Depth={};m_MSColor={};m_MSNormal={};m_MSDepth={};
}
