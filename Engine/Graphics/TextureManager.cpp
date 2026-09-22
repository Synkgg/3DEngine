#include "TextureManager.h"

#include "Texture2D.h"

Texture2D* TextureManager::Load(
    const std::string& filepath)
{
    auto it =
        m_Textures.find(filepath);

    if (it != m_Textures.end())
    {
        return it->second.get();
    }

    auto texture =
        std::make_unique<Texture2D>();

    if (!texture->Load(filepath))
    {
        return nullptr;
    }

    Texture2D* result =
        texture.get();

    m_Textures.emplace(
        filepath,
        std::move(texture)
    );

    return result;
}

Texture2D* TextureManager::Get(
    const std::string& filepath)
{
    auto it =
        m_Textures.find(filepath);

    if (it == m_Textures.end())
    {
        return nullptr;
    }

    return it->second.get();
}

void TextureManager::Clear()
{
    m_Textures.clear();
}