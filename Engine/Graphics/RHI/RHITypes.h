#pragma once

#include <cstdint>

namespace Velcryn::RHI
{
    enum class GraphicsAPI : std::uint8_t { Unknown, Vulkan, D3D12, Metal };
    enum class QueueType : std::uint8_t { Graphics, Compute, Copy };
    enum class BufferUsage : std::uint8_t { Vertex, Index, Constant, Storage, Upload, Readback };
    enum class TextureFormat : std::uint8_t { Unknown, RGBA8_UNorm, RGBA8_sRGB, RGBA16_Float, D24S8, D32_Float };

    enum class TextureUsage : std::uint32_t
    {
        None = 0,
        Sampled = 1u << 0,
        RenderTarget = 1u << 1,
        DepthStencil = 1u << 2,
        Storage = 1u << 3,
        TransferSource = 1u << 4,
        TransferDestination = 1u << 5
    };

    inline TextureUsage operator|(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }

    template<typename Tag>
    struct Handle
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;
        explicit operator bool() const { return index != 0; }
        bool operator==(const Handle&) const = default;
    };

    struct BufferTag {};
    struct TextureTag {};
    struct SamplerTag {};
    struct PipelineTag {};
    struct CommandListTag {};

    using BufferHandle = Handle<BufferTag>;
    using TextureHandle = Handle<TextureTag>;
    using SamplerHandle = Handle<SamplerTag>;
    using PipelineHandle = Handle<PipelineTag>;
    using CommandListHandle = Handle<CommandListTag>;

    struct BufferDesc
    {
        std::uint64_t size = 0;
        BufferUsage usage = BufferUsage::Vertex;
        bool cpuVisible = false;
        const char* debugName = nullptr;
    };

    struct TextureDesc
    {
        std::uint32_t width = 1, height = 1, depth = 1;
        std::uint32_t mipLevels = 1, arrayLayers = 1, sampleCount = 1;
        TextureFormat format = TextureFormat::RGBA8_UNorm;
        TextureUsage usage = TextureUsage::Sampled;
        const char* debugName = nullptr;
    };

    struct RHICapabilities
    {
        GraphicsAPI api = GraphicsAPI::Unknown;
        bool bindlessResources = false;
        bool asyncCompute = false;
        bool rayTracing = false;
        bool meshShaders = false;
        bool variableRateShading = false;
        bool timelineSemaphores = false;
        std::uint32_t maxTextureDimension2D = 0;
        std::uint32_t maxColorAttachments = 0;
    };
}
