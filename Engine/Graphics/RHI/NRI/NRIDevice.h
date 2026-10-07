#pragma once

#include "../IRHIDevice.h"

#include <NRI.h>
#include <Extensions/NRIDeviceCreation.h>
#include <Extensions/NRIHelper.h>
#include <Extensions/NRISwapChain.h>
#include <Extensions/NRIStreamer.h>
#include <Extensions/NRIImgui.h>

#include <vector>
#include <functional>

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
        bool UpdateBuffer(BufferHandle buffer, const void* data, std::size_t size) override;
        TextureHandle CreateTexture(const TextureDesc& desc, const void* initialData, std::size_t initialDataSize) override;
        void DestroyTexture(TextureHandle texture) override;
        std::uint64_t GetImGuiTextureID(TextureHandle texture) const override;
        bool ReadTexture(TextureHandle texture, std::vector<std::uint8_t>& bytes) override;
        PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
        void DestroyPipeline(PipelineHandle pipeline) override;
        bool BeginRendering(TextureHandle color, TextureHandle depth, const float clearColor[4]) override;
        void EndRendering() override;
        bool DrawIndexed(PipelineHandle pipeline, BufferHandle vertices, BufferHandle indices,
            std::uint32_t indexCount, const void* constants, std::uint32_t constantSize,
            std::uint32_t instanceCount = 1) override;

        CommandListHandle BeginCommandList(QueueType queue) override;
        void EndCommandList(CommandListHandle commandList) override;
        void Submit(CommandListHandle commandList) override;

        void BeginFrame() override;
        void EndFrame() override;
        void WaitIdle() override;

        bool IsPresentReady() const { return m_SwapChain != nullptr; }

    private:
        struct BufferSlot
        {
            nri::Buffer* resource = nullptr;
            std::vector<nri::Memory*> allocations;
            std::uint32_t generation = 1;
            BufferDesc desc{};
        };

        struct TextureSlot
        {
            nri::Texture* resource = nullptr;
            std::vector<nri::Memory*> allocations;
            nri::Descriptor* shaderResource = nullptr;
            nri::Descriptor* imguiResource = nullptr;
            nri::Descriptor* attachment = nullptr;
            nri::AccessLayoutStage state{};
            TextureDesc desc{};
            std::uint32_t generation = 1;
        };

        struct CommandSlot
        {
            nri::CommandAllocator* allocator = nullptr;
            nri::CommandBuffer* commandBuffer = nullptr;
            std::uint32_t generation = 1;
            bool recording = false;
            nri::Queue* queue = nullptr;
            bool submitted = false;
        };

        struct PipelineSlot
        {
            nri::Pipeline* resource = nullptr;
            nri::PipelineLayout* layout = nullptr;
            std::uint32_t generation = 1, stride = 0, constantSize = 0;
        };
        void TransitionTexture(TextureSlot& texture, nri::AccessLayoutStage after);
        void CollectGarbage();
        bool CreateSwapChain(Window& window);
        void DestroySwapChain();
        Window* m_Window = nullptr;
        std::uint32_t m_SwapChainWidth = 0, m_SwapChainHeight = 0;
        bool m_RebuildSwapChain = false, m_DeviceFailed = false;
        std::vector<bool> m_Presented;
        std::vector<PipelineSlot> m_Pipelines;
        std::vector<std::function<void()>> m_Garbage;
        TextureHandle m_RenderColor{}, m_RenderDepth{};
        bool m_Rendering = false;

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
        nri::SwapChainInterface m_SwapChainInterface{};
        nri::StreamerInterface m_StreamerInterface{};
        nri::ImguiInterface m_ImguiInterface{};
        nri::Streamer* m_Streamer = nullptr;
        nri::Imgui* m_Imgui = nullptr;
        nri::SwapChain* m_SwapChain = nullptr;
        std::vector<nri::Texture*> m_SwapChainTextures;
        std::vector<nri::Descriptor*> m_SwapChainViews;
        std::vector<nri::Fence*> m_AcquireSemaphores;
        std::vector<nri::Fence*> m_ReleaseSemaphores;
        nri::Fence* m_FrameFence = nullptr;
        nri::CommandAllocator* m_FrameAllocator = nullptr;
        nri::CommandBuffer* m_FrameCommandBuffer = nullptr;
        uint64_t m_FrameIndex = 0;
        uint32_t m_BackBufferIndex = 0;
        uint32_t m_AcquireIndex = 0;
        bool m_FrameOpen = false;
        nri::Format m_SwapChainFormat = nri::Format::UNKNOWN;
        RHICapabilities m_Capabilities{};

        std::vector<BufferSlot> m_Buffers;
        std::vector<TextureSlot> m_Textures;
        std::vector<CommandSlot> m_CommandLists;
    };
}
