#pragma once

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();

    bool Initialize(unsigned int width, unsigned int height, unsigned int samples = 1, bool hdr = true);
    void Bind();
    void Unbind();
    void Resolve();
    void Resize(unsigned int width, unsigned int height);
    void SetSamples(unsigned int samples);
    void Shutdown();

    unsigned int GetColorTexture() const;
    unsigned int GetSamples() const;

private:
    bool CreateTargets();

    unsigned int m_FramebufferID = 0;
    unsigned int m_ColorTextureID = 0;
    unsigned int m_DepthStencilID = 0;
    unsigned int m_ResolveFramebufferID = 0;
    unsigned int m_ResolveColorTextureID = 0;

    unsigned int m_Width = 0;
    unsigned int m_Height = 0;
    unsigned int m_Samples = 1;
    bool m_HDR = true;
};
