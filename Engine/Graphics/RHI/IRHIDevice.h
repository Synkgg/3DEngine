#pragma once

#include "RHITypes.h"
#include <cstddef>

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
        virtual TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData = nullptr, std::size_t initialDataSize = 0) = 0;
        virtual void DestroyTexture(TextureHandle texture) = 0;
        virtual std::uint64_t GetImGuiTextureID(TextureHandle texture) const = 0;

        virtual CommandListHandle BeginCommandList(QueueType queue = QueueType::Graphics) = 0;
        virtual void EndCommandList(CommandListHandle commandList) = 0;
        virtual void Submit(CommandListHandle commandList) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
        virtual void WaitIdle() = 0;
    };
}
