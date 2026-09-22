#pragma once

#include <memory>
#include <string>
#include <unordered_map>

class Texture2D;

class TextureManager
{
public:
    TextureManager() = default;
    ~TextureManager() = default;

    Texture2D* Load(
        const std::string& filepath
    );

    Texture2D* Get(
        const std::string& filepath
    );

    void Clear();

private:
    std::unordered_map<
        std::string,
        std::unique_ptr<Texture2D>
    > m_Textures;
};