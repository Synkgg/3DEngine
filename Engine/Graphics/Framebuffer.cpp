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

    // Resolve/MSAA is moved into the render graph. Keep editor targets sampleable.
    m_Samples=1;

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
    // Vulkan MSAA resolve will be a render-graph concern; editor targets remain single-sample.
    m_Samples=std::clamp(samples,1u,8u);
    return true;
}

void Framebuffer::Shutdown()
{
    if(auto* device=Velcryn::RHI::GetDevice())
    {
        if(m_Color)device->DestroyTexture(m_Color);
        if(m_Normal)device->DestroyTexture(m_Normal);
        if(m_Depth)device->DestroyTexture(m_Depth);
    }
    m_Color={};m_Normal={};m_Depth={};
}
