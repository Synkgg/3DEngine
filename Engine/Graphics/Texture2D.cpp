#include "Texture2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb_image.h"

#include "../Core/Logger.h"
#include "RHI/RHI.h"

Texture2D::Texture2D() = default;
Texture2D::~Texture2D() { Unload(); }

bool Texture2D::Load(const std::string& filepath)
{
    Unload();

    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load(
        filepath.c_str(), &m_Width, &m_Height, &m_Channels, 4);

    if (!pixels)
    {
        const char* reason = stbi_failure_reason();
        Logger::Error(std::string("Failed to load texture: ") + filepath + " - " +
            (reason ? reason : "Unknown error"));
        return false;
    }

    auto* device = Velcryn::RHI::GetDevice();
    if (!device)
    {
        stbi_image_free(pixels);
        Logger::Error("Texture2D: RHI device is not initialized.");
        return false;
    }

    Velcryn::RHI::TextureDesc desc{};
    desc.width = static_cast<std::uint32_t>(m_Width);
    desc.height = static_cast<std::uint32_t>(m_Height);
    desc.depth = 1;
    desc.mipLevels = 1;
    desc.arrayLayers = 1;
    desc.sampleCount = 1;
    desc.format = Velcryn::RHI::TextureFormat::RGBA8_sRGB;
    desc.usage = Velcryn::RHI::TextureUsage::Sampled |
                 Velcryn::RHI::TextureUsage::TransferDestination;
    desc.debugName = filepath.c_str();

    const std::size_t byteSize =
        static_cast<std::size_t>(m_Width) *
        static_cast<std::size_t>(m_Height) * 4u;

    m_Handle = device->CreateTexture(desc, pixels, byteSize);
    stbi_image_free(pixels);

    if (!m_Handle)
    {
        Logger::Error(std::string("Texture2D: failed to create RHI texture: ") + filepath);
        return false;
    }

    m_Channels = 4;
    m_Loaded = true;
    return true;
}

void Texture2D::Unload()
{
    if (m_Handle)
    {
        if (auto* device = Velcryn::RHI::GetDevice())
            device->DestroyTexture(m_Handle);
        m_Handle = {};
    }

    m_Width = 0;
    m_Height = 0;
    m_Channels = 0;
    m_Loaded = false;
}

void Texture2D::Bind(unsigned int) const
{
    // Descriptor binding is owned by the Vulkan RHI pipeline.
}

void Texture2D::Unbind() const
{
}

bool Texture2D::IsLoaded() const { return m_Loaded; }
unsigned int Texture2D::GetID() const { return 0; }
int Texture2D::GetWidth() const { return m_Width; }
int Texture2D::GetHeight() const { return m_Height; }
int Texture2D::GetChannels() const { return m_Channels; }
