#pragma once
#include "../RHI/RHITypes.h"

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();
    bool Initialize(unsigned int width, unsigned int height, unsigned int samples=1, bool hdr=true);
    void Bind();
    void Unbind();
    void Resolve();
    bool Resize(unsigned int width, unsigned int height);
    bool SetSamples(unsigned int samples);
    void Shutdown();

    Velcryn::RHI::TextureHandle GetColorTexture() const { return m_Color; }
    Velcryn::RHI::TextureHandle GetDepthTexture() const { return m_Depth; }
    Velcryn::RHI::TextureHandle GetNormalTexture() const { return m_Normal; }
    Velcryn::RHI::TextureHandle GetRenderColor() const { return m_Samples>1?m_MSColor:m_Color; }
    Velcryn::RHI::TextureHandle GetRenderNormal() const { return m_Samples>1?m_MSNormal:m_Normal; }
    Velcryn::RHI::TextureHandle GetRenderDepth() const { return m_Samples>1?m_MSDepth:m_Depth; }
    unsigned int GetSamples() const { return m_Samples; }

private:
    bool CreateTargets();
    Velcryn::RHI::TextureHandle m_MSColor{},m_MSNormal{},m_MSDepth{};
    Velcryn::RHI::TextureHandle m_Color{};
    Velcryn::RHI::TextureHandle m_Normal{};
    Velcryn::RHI::TextureHandle m_Depth{};
    unsigned int m_Width=0, m_Height=0, m_Samples=1;
    bool m_HDR=true;
};
