#pragma once

#include "RHITypes.h"
#include <cstddef>
#include <vector>

class Window;

namespace Velcryn::RHI
{
    class IRHIDevice
    {
    public:
        virtual ~IRHIDevice() = default;

        virtual bool Initialize(Window& window) = 0;
        virtual void Shutdown() = 0;

        virtual GraphicsAPI GetAPI() const = 0;
        virtual const RHICapabilities& GetCapabilities() const = 0;

        virtual BufferHandle CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
        virtual void DestroyBuffer(BufferHandle buffer) = 0;
        // Replace the complete contents; previously recorded draws retain their resource version.
        virtual bool UpdateBuffer(BufferHandle buffer, const void* data, std::size_t size) = 0;
        virtual TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr, std::size_t initialDataSize = 0) = 0;
        virtual void DestroyTexture(TextureHandle texture) = 0;
        virtual std::uint64_t GetImGuiTextureID(TextureHandle texture) const = 0;
        // Synchronous diagnostic readback of mip 0/layer 0, tightly packed in the texture's format.
        virtual bool ReadTexture(TextureHandle texture, std::vector<std::uint8_t>& bytes) = 0;

        virtual PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual void DestroyPipeline(PipelineHandle pipeline) = 0;
        // These commands record into the current graphics frame.
        virtual bool BeginRendering(TextureHandle color, TextureHandle depth, const float clearColor[4], bool clear = true, TextureHandle secondColor = {}, TextureHandle resolveColor = {}, TextureHandle resolveDepth = {}, TextureHandle resolveNormal = {}) = 0;
        virtual void EndRendering() = 0;
        virtual bool DrawIndexed(PipelineHandle pipeline, BufferHandle vertices, BufferHandle indices,
            std::uint32_t indexCount, const void* constants, std::uint32_t constantSize,
            std::uint32_t instanceCount = 1, TextureHandle texture = {}, std::span<const std::byte> uniforms = {}, std::span<const TextureHandle> additionalTextures = {}) = 0;

        virtual CommandListHandle BeginCommandList(QueueType queue = QueueType::Graphics) = 0;
        virtual void EndCommandList(CommandListHandle commandList) = 0;
        virtual void Submit(CommandListHandle commandList) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void WaitIdle() = 0;
    };
}
