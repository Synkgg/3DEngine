#include "Texture2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb_image.h"

#include "../Core/Logger.h"
#include "RHI/RHI.h"
#include <vector>
#include <algorithm>

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

    // The legacy renderer generated UNORM mipmaps. Preserve that working space
    // for existing scene/data textures; the RHI still exposes an sRGB view when needed.
    std::vector<std::uint8_t> mipData(pixels,pixels+byteSize);
    unsigned int width=desc.width,height=desc.height;
    std::size_t previousOffset=0;
    while(width>1 || height>1) {
        const unsigned int nextWidth=std::max(1u,width/2),nextHeight=std::max(1u,height/2);
        const std::size_t nextOffset=mipData.size();
        mipData.resize(nextOffset+std::size_t(nextWidth)*nextHeight*4);
        for(unsigned int y=0;y<nextHeight;++y)for(unsigned int x=0;x<nextWidth;++x)for(unsigned int c=0;c<4;++c) {
            unsigned int sum=0,count=0;
            for(unsigned int sy=y*height/nextHeight;sy<(y+1)*height/nextHeight;++sy)
                for(unsigned int sx=x*width/nextWidth;sx<(x+1)*width/nextWidth;++sx) {
                    sum+=mipData[previousOffset+(std::size_t(sy)*width+sx)*4+c];++count;
                }
            mipData[nextOffset+(std::size_t(y)*nextWidth+x)*4+c]=static_cast<std::uint8_t>((sum+count/2)/count);
        }
        previousOffset=nextOffset;width=nextWidth;height=nextHeight;++desc.mipLevels;
    }
    m_Handle = device->CreateTexture(desc, mipData.data(), mipData.size());
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
std::uint64_t Texture2D::GetID() const { if (auto* device = Velcryn::RHI::GetDevice()) return device->GetImGuiTextureID(m_Handle); return 0; }
int Texture2D::GetWidth() const { return m_Width; }
int Texture2D::GetHeight() const { return m_Height; }
int Texture2D::GetChannels() const { return m_Channels; }
