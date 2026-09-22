#include "Framebuffer.h"

#include <glad/gl.h>

Framebuffer::Framebuffer()
    : m_FramebufferID(0),
    m_ColorTextureID(0),
    m_DepthStencilID(0),
    m_Width(0),
    m_Height(0)
{
}

Framebuffer::~Framebuffer()
{
    Shutdown();
}

bool Framebuffer::Initialize(unsigned int width, unsigned int height)
{
    m_Width = width;
    m_Height = height;

    glGenFramebuffers(1, &m_FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

    glGenTextures(1, &m_ColorTextureID);
    glBindTexture(GL_TEXTURE_2D, m_ColorTextureID);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        m_Width,
        m_Height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_ColorTextureID,
        0
    );

    glGenRenderbuffers(1, &m_DepthStencilID);
    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthStencilID);

    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH24_STENCIL8,
        m_Width,
        m_Height
    );

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        m_DepthStencilID
    );

    bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete)
    {
        Shutdown();
        return false;
    }

    return true;
}

void Framebuffer::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
}

void Framebuffer::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(unsigned int width, unsigned int height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    if (width == m_Width && height == m_Height)
    {
        return;
    }

    Shutdown();

    Initialize(width, height);
}

void Framebuffer::Shutdown()
{
    if (m_DepthStencilID != 0)
    {
        glDeleteRenderbuffers(1, &m_DepthStencilID);
        m_DepthStencilID = 0;
    }

    if (m_ColorTextureID != 0)
    {
        glDeleteTextures(1, &m_ColorTextureID);
        m_ColorTextureID = 0;
    }

    if (m_FramebufferID != 0)
    {
        glDeleteFramebuffers(1, &m_FramebufferID);
        m_FramebufferID = 0;
    }

    m_Width = 0;
    m_Height = 0;
}

unsigned int Framebuffer::GetColorTexture() const
{
    return m_ColorTextureID;
}