#pragma once

#include <string>
#include "RHI/RHITypes.h"

class Texture2D
{
public:
    Texture2D();
    ~Texture2D();

    bool Load(const std::string& filepath);
    void Unload();

    // Binding is performed by the RHI descriptor/pipeline path.
    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    bool IsLoaded() const;

    // Legacy compatibility only. OpenGL texture IDs no longer exist.
    unsigned int GetID() const;
    Velcryn::RHI::TextureHandle GetHandle() const { return m_Handle; }
    int GetWidth() const;
    int GetHeight() const;
    int GetChannels() const;

private:
    Velcryn::RHI::TextureHandle m_Handle{};
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    bool m_Loaded = false;
};
