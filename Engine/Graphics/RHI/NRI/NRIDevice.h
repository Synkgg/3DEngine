#pragma once

#include "../IRHIDevice.h"

#include <NRI.h>
#include <Extensions/NRIDeviceCreation.h>

namespace Velcryn::RHI
{
    class NRIDevice final : public IRHIDevice
    {
    public:
        NRIDevice() = default;
        ~NRIDevice() override;

        bool Initialize(Window& window) override;
        void Shutdown() override;

        GraphicsAPI GetAPI() const override { return GraphicsAPI::Vulkan; }
        const RHICapabilities& GetCapabilities() const override { return m_Capabilities; }

        BufferHandle CreateBuffer(const BufferDesc& desc, const void* initialData) override;
        void DestroyBuffer(BufferHandle buffer) override;
        TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData, std::size_t initialDataSize) override;
        void DestroyTexture(TextureHandle texture) override;

        CommandListHandle BeginCommandList(QueueType queue) override;
        void EndCommandList(CommandListHandle commandList) override;
        void Submit(CommandListHandle commandList) override;

        void BeginFrame() override;
        void EndFrame() override;
        void WaitIdle() override;

    private:
        nri::Device* m_Device = nullptr;
        nri::Queue* m_GraphicsQueue = nullptr;
        nri::CoreInterface m_Core{};
        RHICapabilities m_Capabilities{};
    };
}
