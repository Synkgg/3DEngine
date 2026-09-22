#pragma once

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();

    bool Initialize(unsigned int width, unsigned int height);

    void Bind();
    void Unbind();

    void Resize(unsigned int width, unsigned int height);

    void Shutdown();

    unsigned int GetColorTexture() const;

private:
    unsigned int m_FramebufferID;
    unsigned int m_ColorTextureID;
    unsigned int m_DepthStencilID;

    unsigned int m_Width;
    unsigned int m_Height;
};