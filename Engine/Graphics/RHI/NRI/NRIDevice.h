#pragma once

#include "../IRHIDevice.h"

#include <NRI.h>
#include <Extensions/NRIDeviceCreation.h>
#include <Extensions/NRIHelper.h>

#include <vector>

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
        struct BufferSlot
        {
            nri::Buffer* resource = nullptr;
            std::vector<nri::Memory*> allocations;
            std::uint32_t generation = 1;
        };

        struct TextureSlot
        {
            nri::Texture* resource = nullptr;
            std::vector<nri::Memory*> allocations;
            std::uint32_t generation = 1;
        };

        struct CommandSlot
        {
            nri::CommandAllocator* allocator = nullptr;
            nri::CommandBuffer* commandBuffer = nullptr;
            std::uint32_t generation = 1;
            bool recording = false;
        };

        nri::Queue* ResolveQueue(QueueType queue) const;
        static nri::Format ToNRIFormat(TextureFormat format);
        static nri::BufferUsageBits ToNRIBufferUsage(BufferUsage usage);
        static nri::TextureUsageBits ToNRITextureUsage(TextureUsage usage);

        nri::Device* m_Device = nullptr;
        nri::Queue* m_GraphicsQueue = nullptr;
        nri::Queue* m_ComputeQueue = nullptr;
        nri::Queue* m_CopyQueue = nullptr;
        nri::CoreInterface m_Core{};
        nri::HelperInterface m_Helper{};
        RHICapabilities m_Capabilities{};

        std::vector<BufferSlot> m_Buffers;
        std::vector<TextureSlot> m_Textures;
        std::vector<CommandSlot> m_CommandLists;
    };
}
