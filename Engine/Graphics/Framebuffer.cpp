#include "Framebuffer.h"
#include <glad/gl.h>
#include <algorithm>
#include "../Core/Logger.h"

Framebuffer::Framebuffer() = default;
Framebuffer::~Framebuffer() { Shutdown(); }

bool Framebuffer::Initialize(unsigned int width, unsigned int height, unsigned int samples, bool hdr)
{
    m_Width = width; m_Height = height; m_Samples = std::clamp(samples, 1u, 8u); m_HDR = hdr;
    return CreateTargets();
}

bool Framebuffer::CreateTargets()
{
    glGenFramebuffers(1, &m_FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
    const GLenum internalFormat = m_HDR ? GL_RGBA16F : GL_RGBA8;

    if (m_Samples > 1)
    {
        glGenTextures(1, &m_ColorTextureID);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_ColorTextureID);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_Samples, internalFormat, m_Width, m_Height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_ColorTextureID, 0);

        glGenRenderbuffers(1, &m_DepthStencilID);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthStencilID);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_Samples, GL_DEPTH24_STENCIL8, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthStencilID);

        glGenFramebuffers(1, &m_ResolveFramebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_ResolveFramebufferID);
        glGenTextures(1, &m_ResolveColorTextureID);
        glBindTexture(GL_TEXTURE_2D, m_ResolveColorTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, GL_RGBA, m_HDR ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ResolveColorTextureID, 0);

        // The resolve target is a separate framebuffer and must be validated
        // independently from the multisampled render target.
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            Shutdown();
            return false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
    }
    else
    {
        glGenTextures(1, &m_ColorTextureID);
        glBindTexture(GL_TEXTURE_2D, m_ColorTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, GL_RGBA, m_HDR ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorTextureID, 0);

        glGenRenderbuffers(1, &m_DepthStencilID);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthStencilID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthStencilID);
    }

    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) { Shutdown(); return false; }
    return true;
}

void Framebuffer::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID); }
void Framebuffer::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void Framebuffer::Resolve()
{
    if (m_Samples <= 1) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FramebufferID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_ResolveFramebufferID);
    glBlitFramebuffer(0,0,m_Width,m_Height,0,0,m_Width,m_Height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Framebuffer::Resize(unsigned int width, unsigned int height)
{
    if (!width || !height) return false;
    if (width == m_Width && height == m_Height) return true;

    const unsigned int oldWidth = m_Width;
    const unsigned int oldHeight = m_Height;
    m_Width = width;
    m_Height = height;
    Shutdown();
    m_Width = width;
    m_Height = height;

    if (CreateTargets()) return true;

    Logger::Error("Failed to resize framebuffer; restoring previous size.");
    m_Width = oldWidth;
    m_Height = oldHeight;
    return CreateTargets();
}

bool Framebuffer::SetSamples(unsigned int samples)
{
    samples = std::clamp(samples, 1u, 8u);
    if (samples == m_Samples) return true;

    const unsigned int oldSamples = m_Samples;
    const unsigned int width = m_Width;
    const unsigned int height = m_Height;
    m_Samples = samples;
    Shutdown();
    m_Width = width;
    m_Height = height;

    if (CreateTargets()) return true;

    Logger::Error("Failed to change MSAA sample count; restoring previous setting.");
    m_Samples = oldSamples;
    return CreateTargets();
}

void Framebuffer::Shutdown()
{
    if(m_DepthStencilID)glDeleteRenderbuffers(1,&m_DepthStencilID);
    if(m_ColorTextureID)glDeleteTextures(1,&m_ColorTextureID);
    if(m_ResolveColorTextureID)glDeleteTextures(1,&m_ResolveColorTextureID);
    if(m_FramebufferID)glDeleteFramebuffers(1,&m_FramebufferID);
    if(m_ResolveFramebufferID)glDeleteFramebuffers(1,&m_ResolveFramebufferID);
    m_DepthStencilID=m_ColorTextureID=m_ResolveColorTextureID=m_FramebufferID=m_ResolveFramebufferID=0;
}

unsigned int Framebuffer::GetColorTexture() const { return m_Samples>1 ? m_ResolveColorTextureID : m_ColorTextureID; }
unsigned int Framebuffer::GetSamples() const { return m_Samples; }
