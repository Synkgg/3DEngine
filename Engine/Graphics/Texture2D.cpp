#include "Texture2D.h"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb_image.h"

#include "../Core/Logger.h"

Texture2D::Texture2D()
    : m_ID(0),
    m_Width(0),
    m_Height(0),
    m_Channels(0),
    m_Loaded(false)
{
}

Texture2D::~Texture2D()
{
    Unload();
}

bool Texture2D::Load(
    const std::string& filepath)
{
    Unload();

    stbi_set_flip_vertically_on_load(true);

    unsigned char* pixels =
        stbi_load(
            filepath.c_str(),
            &m_Width,
            &m_Height,
            &m_Channels,
            4
        );

    if (pixels == nullptr)
    {
        const char* reason =
            stbi_failure_reason();

        Logger::Error(
            std::string("Failed to load texture: ") +
            filepath +
            " - " +
            (
                reason != nullptr
                ? reason
                : "Unknown error"
                )
        );

        return false;
    }

    glGenTextures(
        1,
        &m_ID
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_ID
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR_MIPMAP_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_REPEAT
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_REPEAT
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        m_Width,
        m_Height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );

    glGenerateMipmap(
        GL_TEXTURE_2D
    );

    glBindTexture(
        GL_TEXTURE_2D,
        0
    );

    stbi_image_free(
        pixels
    );

    m_Loaded = true;

    return true;
}

void Texture2D::Unload()
{
    if (m_ID != 0)
    {
        glDeleteTextures(
            1,
            &m_ID
        );

        m_ID = 0;
    }

    m_Width = 0;
    m_Height = 0;
    m_Channels = 0;
    m_Loaded = false;
}

void Texture2D::Bind(
    unsigned int slot) const
{
    if (!m_Loaded)
    {
        return;
    }

    glActiveTexture(
        GL_TEXTURE0 + slot
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_ID
    );
}

void Texture2D::Unbind() const
{
    glBindTexture(
        GL_TEXTURE_2D,
        0
    );
}

bool Texture2D::IsLoaded() const
{
    return m_Loaded;
}

unsigned int Texture2D::GetID() const
{
    return m_ID;
}

int Texture2D::GetWidth() const
{
    return m_Width;
}

int Texture2D::GetHeight() const
{
    return m_Height;
}

int Texture2D::GetChannels() const
{
    return m_Channels;
}