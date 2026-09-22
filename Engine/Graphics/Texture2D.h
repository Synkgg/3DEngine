#pragma once

#include <string>

class Texture2D
{
public:
    Texture2D();
    ~Texture2D();

    bool Load(const std::string& filepath);
    void Unload();

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    bool IsLoaded() const;

    unsigned int GetID() const;
    int GetWidth() const;
    int GetHeight() const;
    int GetChannels() const;

private:
    unsigned int m_ID;

    int m_Width;
    int m_Height;
    int m_Channels;

    bool m_Loaded;
};