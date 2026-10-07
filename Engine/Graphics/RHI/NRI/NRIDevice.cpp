#include "NRIDevice.h"

#include "../../../Core/Logger.h"
#include "../../../Platform/SDL/Window.h"

#include <SDL3/SDL.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#include <cstring>
#include <algorithm>
#include <imgui.h>

namespace Velcryn::RHI
{
    NRIDevice::~NRIDevice()
    {
        Shutdown();
    }

    bool NRIDevice::Initialize(Window& window)
    {
        if (m_Device)
            return true;

        nri::DeviceCreationDesc desc{};
        desc.graphicsAPI = nri::GraphicsAPI::VK;
#if defined(ENGINE_DEBUG)
        desc.enableNRIValidation = true;
        desc.enableGraphicsAPIValidation = true;
#endif

        if (nri::nriCreateDevice(desc, m_Device) != nri::Result::SUCCESS || !m_Device)
        {
            Logger::Error("RHI: NRI failed to create the Vulkan device.");
            return false;
        }

        if (nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::CoreInterface), &m_Core) != nri::Result::SUCCESS ||
            nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::HelperInterface), &m_Helper) != nri::Result::SUCCESS ||
            nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::SwapChainInterface), &m_SwapChainInterface) != nri::Result::SUCCESS ||
            nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::StreamerInterface), &m_StreamerInterface) != nri::Result::SUCCESS ||
            nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::ImguiInterface), &m_ImguiInterface) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to acquire required NRI interfaces.");
            Shutdown();
            return false;
        }

        if (m_Core.GetQueue(*m_Device, nri::QueueType::GRAPHICS, 0, m_GraphicsQueue) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to acquire the graphics queue.");
            Shutdown();
            return false;
        }

        const nri::DeviceDesc& deviceDesc = m_Core.GetDeviceDesc(*m_Device);

        if (deviceDesc.adapterDesc.queueNum[static_cast<uint32_t>(nri::QueueType::COMPUTE)] > 0)
            m_Core.GetQueue(*m_Device, nri::QueueType::COMPUTE, 0, m_ComputeQueue);

        if (deviceDesc.adapterDesc.queueNum[static_cast<uint32_t>(nri::QueueType::COPY)] > 0)
            m_Core.GetQueue(*m_Device, nri::QueueType::COPY, 0, m_CopyQueue);

        m_Capabilities.api = GraphicsAPI::Vulkan;
        m_Capabilities.bindlessResources = deviceDesc.tiers.bindless > 0;
        m_Capabilities.asyncCompute = m_ComputeQueue != nullptr;
        m_Capabilities.rayTracing = deviceDesc.tiers.rayTracing > 0;
        m_Capabilities.meshShaders = deviceDesc.features.meshShader;
        m_Capabilities.variableRateShading = deviceDesc.tiers.shadingRate > 0;
        m_Capabilities.timelineSemaphores = true;
        m_Capabilities.maxTextureDimension2D = deviceDesc.dimensions.texture2DMaxDim;
        m_Capabilities.maxColorAttachments = deviceDesc.shaderStage.fragment.attachmentMaxNum;

        m_Window = &window;
        if (!CreateSwapChain(window)) { Shutdown(); return false; }
        if (m_Core.CreateFence(*m_Device, 0, m_FrameFence) != nri::Result::SUCCESS ||
            m_Core.CreateCommandAllocator(*m_GraphicsQueue, m_FrameAllocator) != nri::Result::SUCCESS ||
            m_Core.CreateCommandBuffer(*m_FrameAllocator, m_FrameCommandBuffer) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to create frame command resources.");
            Shutdown(); return false;
        }
        nri::StreamerDesc streamerDesc{};
        // NRI validates this optional field even when the static constant ring is unused.
        // Zero-initialization maps to DEVICE here, which is invalid for Streamer constants.
        streamerDesc.constantBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
        streamerDesc.constantBufferSize = 0;
        streamerDesc.dynamicBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
        streamerDesc.dynamicBufferDesc.usage = nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;
        streamerDesc.queuedFrameNum = 2;
        if (m_StreamerInterface.CreateStreamer(*m_Device, streamerDesc, m_Streamer) != nri::Result::SUCCESS) { Logger::Error("RHI: failed to create NRI streamer."); Shutdown(); return false; }
        nri::DescriptorPoolDesc descriptorPoolDesc{};
        descriptorPoolDesc.descriptorSetMaxNum = 4096;
        descriptorPoolDesc.textureMaxNum = 4096;
        if (m_Core.CreateDescriptorPool(*m_Device, descriptorPoolDesc, m_DescriptorPool) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to create the graphics descriptor pool.");
            Shutdown(); return false;
        }
        nri::ImguiDesc imguiDesc{}; imguiDesc.descriptorPoolSize = 2048;
        if (m_ImguiInterface.CreateImgui(*m_Device, imguiDesc, m_Imgui) != nri::Result::SUCCESS) { Logger::Error("RHI: failed to create NRI ImGui renderer."); Shutdown(); return false; }
        Logger::Info(std::string("RHI: NRI Vulkan device + swapchain + ImGui initialized on ") + deviceDesc.adapterDesc.name);
        return true;
    }

    bool NRIDevice::CreateSwapChain(Window& window)
    {
        #if defined(_WIN32)
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window.GetNativeWindow());
        void* hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (!hwnd)
        {
            Logger::Error("RHI: SDL did not expose a Win32 window handle.");
            DestroySwapChain();
            return false;
        }

        nri::SwapChainDesc swapDesc{};
        swapDesc.window.windows.hwnd = hwnd;
        swapDesc.queue = m_GraphicsQueue;
        int pixelWidth = 0, pixelHeight = 0;
        SDL_GetWindowSizeInPixels(window.GetNativeWindow(), &pixelWidth, &pixelHeight);
        if (pixelWidth <= 0 || pixelHeight <= 0) return false;
        m_SwapChainWidth = static_cast<uint32_t>(pixelWidth); m_SwapChainHeight = static_cast<uint32_t>(pixelHeight);
        swapDesc.width = static_cast<nri::Dim_t>(pixelWidth);
        swapDesc.height = static_cast<nri::Dim_t>(pixelHeight);
        swapDesc.textureNum = 3;
        swapDesc.format = nri::SwapChainFormat::BT709_G22_8BIT;
        swapDesc.flags = nri::SwapChainBits::NONE;
        swapDesc.queuedFrameNum = 2;

        if (m_SwapChainInterface.CreateSwapChain(*m_Device, swapDesc, m_SwapChain) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to create the Vulkan swapchain.");
            DestroySwapChain();
            return false;
        }

        uint32_t textureNum = 0;
        nri::Texture* const* textures = m_SwapChainInterface.GetSwapChainTextures(*m_SwapChain, textureNum);
        m_SwapChainTextures.assign(textures, textures + textureNum);
        if (m_SwapChainTextures.empty())
        {
            Logger::Error("RHI: swapchain returned no backbuffers.");
            DestroySwapChain();
            return false;
        }

        m_SwapChainFormat = m_Core.GetTextureDesc(*m_SwapChainTextures[0]).format;
        m_Presented.assign(textureNum, false);
        m_SwapChainViews.resize(textureNum);
        m_AcquireSemaphores.resize(textureNum);
        m_ReleaseSemaphores.resize(textureNum);

        for (uint32_t i = 0; i < textureNum; ++i)
        {
            nri::TextureViewDesc viewDesc{};
            viewDesc.texture = m_SwapChainTextures[i];
            viewDesc.type = nri::TextureView::COLOR_ATTACHMENT;
            viewDesc.format = m_SwapChainFormat;
            viewDesc.mipNum = 1;
            viewDesc.layerNum = 1;
            viewDesc.sliceNum = 1;
            viewDesc.planes = nri::PlaneBits::COLOR;

            if (m_Core.CreateTextureView(viewDesc, m_SwapChainViews[i]) != nri::Result::SUCCESS ||
                m_Core.CreateFence(*m_Device, nri::SWAPCHAIN_SEMAPHORE, m_AcquireSemaphores[i]) != nri::Result::SUCCESS ||
                m_Core.CreateFence(*m_Device, nri::SWAPCHAIN_SEMAPHORE, m_ReleaseSemaphores[i]) != nri::Result::SUCCESS)
            {
                Logger::Error("RHI: failed to create swapchain frame resources.");
                DestroySwapChain();
                return false;
            }
        }

        m_RebuildSwapChain = false;
        Logger::Info(std::string("RHI: SDR BT709/G22 swapchain format ") + nri::nriGetFormatProps(m_SwapChainFormat)->name);
        return true;
#else
        Logger::Error("RHI: swapchain window binding is only implemented on Windows.");
        return false;
#endif
    }

    void NRIDevice::DestroySwapChain()
    {
        for (auto* fence : m_AcquireSemaphores) if (fence) m_Core.DestroyFence(fence);
        for (auto* fence : m_ReleaseSemaphores) if (fence) m_Core.DestroyFence(fence);
        for (auto* view : m_SwapChainViews) if (view) m_Core.DestroyDescriptor(view);
        if (m_SwapChain) m_SwapChainInterface.DestroySwapChain(m_SwapChain);
        m_SwapChain = nullptr;
        m_AcquireSemaphores.clear(); m_ReleaseSemaphores.clear();
        m_SwapChainViews.clear(); m_SwapChainTextures.clear(); m_Presented.clear();
    }
    void NRIDevice::Shutdown()
    {
        if (!m_Device)
            return;

        WaitIdle();
        CollectGarbage();
        for (auto& p : m_Pipelines) { if(p.resource) m_Core.DestroyPipeline(p.resource); if(p.layout) m_Core.DestroyPipelineLayout(p.layout); }
        m_Pipelines.clear();

        if (m_Imgui) m_ImguiInterface.DestroyImgui(m_Imgui);
        if (m_Streamer) m_StreamerInterface.DestroyStreamer(m_Streamer);
        if (m_DescriptorPool) m_Core.DestroyDescriptorPool(m_DescriptorPool);
        m_Imgui=nullptr; m_Streamer=nullptr; m_DescriptorPool=nullptr;
        if (m_FrameCommandBuffer) m_Core.DestroyCommandBuffer(m_FrameCommandBuffer);
        if (m_FrameAllocator) m_Core.DestroyCommandAllocator(m_FrameAllocator);
        if (m_FrameFence) m_Core.DestroyFence(m_FrameFence);
        DestroySwapChain();
        m_FrameCommandBuffer = nullptr; m_FrameAllocator = nullptr; m_FrameFence = nullptr;
        m_Window = nullptr;

        for (CommandSlot& slot : m_CommandLists)
        {
            if (slot.commandBuffer)
                m_Core.DestroyCommandBuffer(slot.commandBuffer);
            if (slot.allocator)
                m_Core.DestroyCommandAllocator(slot.allocator);
        }

        for (TextureSlot& slot : m_Textures)
        {
            if (slot.imguiResource) m_Core.DestroyDescriptor(slot.imguiResource);
            if (slot.attachment) m_Core.DestroyDescriptor(slot.attachment);
            if (slot.shaderResource)
                m_Core.DestroyDescriptor(slot.shaderResource);
            if (slot.resource)
                m_Core.DestroyTexture(slot.resource);
            for (nri::Memory* memory : slot.allocations)
                m_Core.FreeMemory(memory);
        }

        for (BufferSlot& slot : m_Buffers)
        {
            if (slot.resource)
                m_Core.DestroyBuffer(slot.resource);
            for (nri::Memory* memory : slot.allocations)
                m_Core.FreeMemory(memory);
        }

        m_CommandLists.clear();
        m_Textures.clear();
        m_Buffers.clear();

        nri::nriDestroyDevice(m_Device);
        m_Device = nullptr;
        m_GraphicsQueue = nullptr;
        m_ComputeQueue = nullptr;
        m_CopyQueue = nullptr;
        m_FrameIndex = 0; m_FrameOpen = false; m_Rendering = false;
        m_Core = {};
        m_Helper = {};
        m_Capabilities = {};
    }

    BufferHandle NRIDevice::CreateBuffer(const BufferDesc& desc, const void* initialData)
    {
        if (!m_Device || desc.size == 0)
            return {};

        nri::BufferDesc nriDesc{};
        nriDesc.size = desc.size;
        nriDesc.usage = ToNRIBufferUsage(desc.usage);

        BufferSlot slot{};
        slot.desc = desc;
        if (m_Core.CreateBuffer(*m_Device, nriDesc, slot.resource) != nri::Result::SUCCESS)
            return {};

        nri::Buffer* resources[] = {slot.resource};
        nri::ResourceGroupDesc group{};
        group.memoryLocation = desc.usage == BufferUsage::Readback ? nri::MemoryLocation::HOST_READBACK : desc.cpuVisible ? nri::MemoryLocation::HOST_UPLOAD : nri::MemoryLocation::DEVICE;
        group.buffers = resources;
        group.bufferNum = 1;

        const uint32_t allocationNum = m_Helper.CalculateAllocationNumber(*m_Device, group);
        slot.allocations.resize(allocationNum);

        if (m_Helper.AllocateAndBindMemory(*m_Device, group, slot.allocations.data()) != nri::Result::SUCCESS)
        {
            m_Core.DestroyBuffer(slot.resource);
            return {};
        }

        if (initialData)
        {
            if (desc.cpuVisible)
            {
                void* mapped = m_Core.MapBuffer(*slot.resource, 0, desc.size);
                if (!mapped) {
                    m_Core.DestroyBuffer(slot.resource);
                    for (auto* memory : slot.allocations) m_Core.FreeMemory(memory);
                    return {};
                }
                std::memcpy(mapped, initialData, static_cast<size_t>(desc.size));
                m_Core.UnmapBuffer(*slot.resource);
            }
            else
            {
                nri::BufferUploadDesc upload{};
                upload.data = initialData;
                upload.buffer = slot.resource;
                upload.after = {desc.usage == BufferUsage::Vertex ? nri::AccessBits::VERTEX_BUFFER :
                    desc.usage == BufferUsage::Index ? nri::AccessBits::INDEX_BUFFER :
                    desc.usage == BufferUsage::Constant ? nri::AccessBits::CONSTANT_BUFFER : nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::ALL};
                if (m_Helper.UploadData(*m_GraphicsQueue, nullptr, 0, &upload, 1) != nri::Result::SUCCESS) {
                    m_Core.DestroyBuffer(slot.resource);
                    for (auto* memory : slot.allocations) m_Core.FreeMemory(memory);
                    return {};
                }
            }
        }

        if (desc.debugName) m_Core.SetDebugName(slot.resource, desc.debugName);
        for (uint32_t i = 0; i < m_Buffers.size(); ++i)
            if (!m_Buffers[i].resource) {
                slot.generation = m_Buffers[i].generation; m_Buffers[i] = std::move(slot);
                return {i + 1, m_Buffers[i].generation};
            }
        m_Buffers.push_back(std::move(slot));
        return {static_cast<uint32_t>(m_Buffers.size()), m_Buffers.back().generation};
    }

    bool NRIDevice::UpdateBuffer(BufferHandle handle, const void* data, std::size_t size)
    {
        if (!handle || !data || handle.index > m_Buffers.size()) return false;
        const auto& current = m_Buffers[handle.index - 1];
        if (current.generation != handle.generation || !current.resource || current.desc.size != size) return false;
        // Dynamic CPU-visible buffers are updated in place. Recreating the
        // allocation for every UI quad/glyph was forcing allocation, binding,
        // deferred destruction and vector churn hundreds of times per frame.
        if (current.desc.cpuVisible)
        {
            void* mapped = m_Core.MapBuffer(*current.resource, 0, size);
            if (!mapped) return false;
            std::memcpy(mapped, data, size);
            m_Core.UnmapBuffer(*current.resource);
            return true;
        }

        auto desc = current.desc;
        desc.debugName = "UpdatedBuffer";
        const auto replacement = CreateBuffer(desc, data);
        if (!replacement) return false;
        DestroyBuffer(handle);
        auto& source = m_Buffers[replacement.index - 1];
        auto& destination = m_Buffers[handle.index - 1];
        destination = std::move(source);
        destination.generation = handle.generation;
        const auto nextGeneration = source.generation + 1;
        source = {}; source.generation = nextGeneration;
        return true;
    }

    void NRIDevice::DestroyBuffer(BufferHandle handle)
    {
        if (!handle || handle.index > m_Buffers.size())
            return;

        BufferSlot& slot = m_Buffers[handle.index - 1];
        if (slot.generation != handle.generation || !slot.resource)
            return;

        m_Garbage.push_back([this, retired = std::move(slot)] {
            m_Core.DestroyBuffer(retired.resource);
            for (auto* memory : retired.allocations) m_Core.FreeMemory(memory);
        });
        const auto generation = slot.generation + 1;
        slot = {}; slot.generation = generation;
    }
    TextureHandle NRIDevice::CreateTexture(const TextureDesc& desc, const void* initialData, std::size_t initialDataSize)
    {
        if (!m_Device || desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.arrayLayers == 0 ||
            desc.mipLevels == 0 || desc.mipLevels > 16 || desc.width > m_Capabilities.maxTextureDimension2D ||
            desc.height > m_Capabilities.maxTextureDimension2D || desc.depth > 65535 || desc.arrayLayers > 65535 ||
            desc.sampleCount == 0 || desc.sampleCount > 8 || (desc.sampleCount & (desc.sampleCount - 1)) ||
            desc.format == TextureFormat::Unknown)
            return {};

        nri::TextureDesc nriDesc{};
        nriDesc.type = desc.depth > 1 ? nri::TextureType::TEXTURE_3D : nri::TextureType::TEXTURE_2D;
        nriDesc.usage = ToNRITextureUsage(desc.usage);
        nriDesc.format = ToNRIFormat(desc.format);
        nriDesc.width = static_cast<nri::Dim_t>(desc.width);
        nriDesc.height = static_cast<nri::Dim_t>(desc.height);
        nriDesc.depth = static_cast<nri::Dim_t>(desc.depth);
        nriDesc.mipNum = static_cast<nri::Dim_t>(desc.mipLevels);
        nriDesc.layerNum = static_cast<nri::Dim_t>(desc.arrayLayers);
        nriDesc.sampleNum = static_cast<nri::Sample_t>(desc.sampleCount);

        TextureSlot slot{};
        slot.desc = desc;
        if (m_Core.CreateTexture(*m_Device, nriDesc, slot.resource) != nri::Result::SUCCESS)
            return {};

        nri::Texture* resources[] = {slot.resource};
        nri::ResourceGroupDesc group{};
        group.memoryLocation = nri::MemoryLocation::DEVICE;
        group.textures = resources;
        group.textureNum = 1;

        const uint32_t allocationNum = m_Helper.CalculateAllocationNumber(*m_Device, group);
        slot.allocations.resize(allocationNum);

        if (m_Helper.AllocateAndBindMemory(*m_Device, group, slot.allocations.data()) != nri::Result::SUCCESS)
        {
            m_Core.DestroyTexture(slot.resource);
            return {};
        }

        if ((static_cast<std::uint32_t>(desc.usage) & static_cast<std::uint32_t>(TextureUsage::Sampled)) != 0) {
            nri::TextureViewDesc view{}; view.texture=slot.resource; view.type=desc.arrayLayers==6?nri::TextureView::TEXTURE_CUBE:nri::TextureView::TEXTURE;
            view.format=nriDesc.format; view.mipNum=static_cast<nri::Dim_t>(desc.mipLevels); view.layerNum=static_cast<nri::Dim_t>(desc.arrayLayers); view.sliceNum=1;
            view.planes=(desc.format==TextureFormat::D24S8||desc.format==TextureFormat::D32_Float)?nri::PlaneBits::DEPTH:nri::PlaneBits::COLOR;
            if(m_Core.CreateTextureView(view,slot.shaderResource)!=nri::Result::SUCCESS){m_Core.DestroyTexture(slot.resource);for(auto* m:slot.allocations)m_Core.FreeMemory(m);return{};}
        }
        auto cleanup = [&] {
            if (slot.imguiResource) m_Core.DestroyDescriptor(slot.imguiResource);
            if (slot.attachment) m_Core.DestroyDescriptor(slot.attachment);
            if (slot.shaderResource) m_Core.DestroyDescriptor(slot.shaderResource);
            m_Core.DestroyTexture(slot.resource);
            for (auto* memory : slot.allocations) m_Core.FreeMemory(memory);
        };
        const bool isDepth = desc.format == TextureFormat::D32_Float || desc.format == TextureFormat::D24S8;
        const auto usage = static_cast<uint32_t>(desc.usage);
        if (usage & static_cast<uint32_t>(TextureUsage::RenderTarget | TextureUsage::DepthStencil))
        {
            nri::TextureViewDesc view{};
            view.texture = slot.resource; view.format = nriDesc.format;
            view.type = isDepth ? nri::TextureView::DEPTH_STENCIL_ATTACHMENT : nri::TextureView::COLOR_ATTACHMENT;
            view.mipNum = 1; view.layerNum = 1; view.sliceNum = 1;
            view.planes = isDepth ? nri::PlaneBits::DEPTH : nri::PlaneBits::COLOR;
            if (m_Core.CreateTextureView(view, slot.attachment) != nri::Result::SUCCESS) { cleanup(); return {}; }
        }
        // ImGui's SDR UNORM target expects encoded image samples, while scene shaders need linear samples.
        if (desc.format == TextureFormat::RGBA8_sRGB && slot.shaderResource && desc.arrayLayers == 1)
        {
            nri::TextureViewDesc view{};
            view.texture = slot.resource; view.format = nri::Format::RGBA8_UNORM; view.type = nri::TextureView::TEXTURE;
            view.mipNum = static_cast<nri::Dim_t>(desc.mipLevels); view.layerNum = 1; view.sliceNum = 1; view.planes = nri::PlaneBits::COLOR;
            if (m_Core.CreateTextureView(view, slot.imguiResource) != nri::Result::SUCCESS) { cleanup(); return {}; }
        }
        if (initialData)
        {
            const auto& props = *nri::nriGetFormatProps(nriDesc.format);
            std::vector<nri::TextureSubresourceUploadDesc> subs;
            size_t offset = 0;
            for (uint32_t layer = 0; layer < desc.arrayLayers; ++layer)
                for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
                {
                    const auto width = (std::max)(1u, desc.width >> mip);
                    const auto height = (std::max)(1u, desc.height >> mip);
                    const auto depth = (std::max)(1u, desc.depth >> mip);
                    nri::TextureSubresourceUploadDesc sub{};
                    sub.rowPitch = width * props.stride; sub.slicePitch = sub.rowPitch * height; sub.sliceNum = depth;
                    const size_t bytes = static_cast<size_t>(sub.slicePitch) * depth;
                    if (offset > initialDataSize || bytes > initialDataSize - offset) { cleanup(); return {}; }
                    sub.slices = static_cast<const uint8_t*>(initialData) + offset;
                    subs.push_back(sub); offset += bytes;
                }
            nri::TextureUploadDesc upload{};
            upload.subresources = subs.data(); upload.texture = slot.resource;
            upload.after = {nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL};
            upload.planes = isDepth ? nri::PlaneBits::DEPTH : nri::PlaneBits::COLOR;
            if (m_Helper.UploadData(*m_GraphicsQueue, &upload, 1, nullptr, 0) != nri::Result::SUCCESS) { cleanup(); return {}; }
            slot.state = upload.after;
        }
        if (desc.debugName) m_Core.SetDebugName(slot.resource, desc.debugName);
        for (uint32_t i = 0; i < m_Textures.size(); ++i)
            if (!m_Textures[i].resource) {
                slot.generation = m_Textures[i].generation; m_Textures[i] = std::move(slot);
                return {i + 1, m_Textures[i].generation};
            }
        m_Textures.push_back(std::move(slot));
        return {static_cast<uint32_t>(m_Textures.size()), m_Textures.back().generation};
    }

    void NRIDevice::DestroyTexture(TextureHandle handle)
    {
        if (!handle || handle.index > m_Textures.size())
            return;

        TextureSlot& slot = m_Textures[handle.index - 1];
        if (slot.generation != handle.generation || !slot.resource)
            return;

        m_Garbage.push_back([this, retired = std::move(slot)] {
            if (retired.imguiResource) m_Core.DestroyDescriptor(retired.imguiResource);
            if (retired.attachment) m_Core.DestroyDescriptor(retired.attachment);
            if (retired.shaderResource) m_Core.DestroyDescriptor(retired.shaderResource);
            m_Core.DestroyTexture(retired.resource);
            for (auto* memory : retired.allocations) m_Core.FreeMemory(memory);
        });
        const auto generation = slot.generation + 1;
        slot = {}; slot.generation = generation;
    }
    std::uint64_t NRIDevice::GetImGuiTextureID(TextureHandle handle) const {
        if(!handle||handle.index>m_Textures.size())return 0;const TextureSlot& s=m_Textures[handle.index-1];
        return (s.generation==handle.generation&&s.shaderResource)?reinterpret_cast<std::uint64_t>(s.imguiResource ? s.imguiResource : s.shaderResource):0;
    }

    CommandListHandle NRIDevice::BeginCommandList(QueueType queueType)
    {
        nri::Queue* queue = ResolveQueue(queueType);
        if (!queue)
            return {};

        CommandSlot slot{};
        if (m_Core.CreateCommandAllocator(*queue, slot.allocator) != nri::Result::SUCCESS ||
            m_Core.CreateCommandBuffer(*slot.allocator, slot.commandBuffer) != nri::Result::SUCCESS)
        {
            if (slot.commandBuffer)
                m_Core.DestroyCommandBuffer(slot.commandBuffer);
            if (slot.allocator)
                m_Core.DestroyCommandAllocator(slot.allocator);
            return {};
        }

        if (m_Core.BeginCommandBuffer(*slot.commandBuffer, nullptr) != nri::Result::SUCCESS)
        {
            m_Core.DestroyCommandBuffer(slot.commandBuffer);
            m_Core.DestroyCommandAllocator(slot.allocator);
            return {};
        }

        slot.recording = true; slot.queue = queue;
        for (uint32_t i = 0; i < m_CommandLists.size(); ++i)
            if (!m_CommandLists[i].commandBuffer) {
                slot.generation = m_CommandLists[i].generation;
                m_CommandLists[i] = slot;
                return {i + 1, slot.generation};
            }
        m_CommandLists.push_back(slot);
        return {static_cast<uint32_t>(m_CommandLists.size()), slot.generation};
    }

    void NRIDevice::EndCommandList(CommandListHandle handle)
    {
        if (!handle || handle.index > m_CommandLists.size())
            return;

        CommandSlot& slot = m_CommandLists[handle.index - 1];
        if (slot.generation != handle.generation || !slot.commandBuffer || !slot.recording)
            return;

        m_Core.EndCommandBuffer(*slot.commandBuffer);
        slot.recording = false;
    }

    void NRIDevice::Submit(CommandListHandle handle)
    {
        if (!handle || handle.index > m_CommandLists.size())
            return;

        CommandSlot& slot = m_CommandLists[handle.index - 1];
        if (slot.generation != handle.generation || !slot.commandBuffer || slot.recording || slot.submitted)
            return;

        nri::CommandBuffer* commandBuffers[] = {slot.commandBuffer};
        nri::QueueSubmitDesc submit{};
        submit.commandBuffers = commandBuffers;
        submit.commandBufferNum = 1;

        slot.submitted = m_Core.QueueSubmit(*slot.queue, submit) == nri::Result::SUCCESS;
    }

    void NRIDevice::BeginFrame()
    {
        if (!m_Window || !m_FrameCommandBuffer || m_FrameOpen || m_DeviceFailed)
            return;

        // One allocator: wait for its last submission before resetting it.
        const uint64_t completedFrame = m_FrameIndex;
        m_Core.Wait(*m_FrameFence, completedFrame);
        CollectGarbage();
        if (m_DescriptorPool)
            m_Core.ResetDescriptorPool(*m_DescriptorPool);

        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(m_Window->GetNativeWindow(), &width, &height);
        if (width <= 0 || height <= 0 || (SDL_GetWindowFlags(m_Window->GetNativeWindow()) & SDL_WINDOW_MINIMIZED)) return;
        if (!m_SwapChain || m_RebuildSwapChain || width != m_SwapChainWidth || height != m_SwapChainHeight)
        {
            WaitIdle(); DestroySwapChain();
            if (!CreateSwapChain(*m_Window)) return;
        }
        m_AcquireIndex = static_cast<uint32_t>(m_FrameIndex % m_AcquireSemaphores.size());
        const auto acquire = m_SwapChainInterface.AcquireNextTexture(*m_SwapChain, *m_AcquireSemaphores[m_AcquireIndex], m_BackBufferIndex);
        if (acquire != nri::Result::SUCCESS) {
            m_RebuildSwapChain = acquire == nri::Result::OUT_OF_DATE;
            m_DeviceFailed = !m_RebuildSwapChain;
            return;
        }
        m_Core.ResetCommandAllocator(*m_FrameAllocator);
        if (m_Core.BeginCommandBuffer(*m_FrameCommandBuffer, m_DescriptorPool) != nri::Result::SUCCESS)
        {
            m_DeviceFailed = true;
            return;
        }

        nri::TextureBarrierDesc toColor{};
        toColor.texture = m_SwapChainTextures[m_BackBufferIndex];
        toColor.before = {nri::AccessBits::NONE, m_Presented[m_BackBufferIndex] ? nri::Layout::PRESENT : nri::Layout::UNDEFINED, nri::StageBits::NONE};
        toColor.after = {nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT};
        toColor.mipNum = nri::REMAINING;
        toColor.layerNum = nri::REMAINING;
        toColor.planes = nri::PlaneBits::COLOR;
        nri::BarrierDesc barrier{};
        barrier.textures = &toColor;
        barrier.textureNum = 1;
        m_Core.CmdBarrier(*m_FrameCommandBuffer, barrier);

        nri::AttachmentDesc color{};
        color.descriptor = m_SwapChainViews[m_BackBufferIndex];
        color.loadOp = nri::LoadOp::CLEAR;
        color.storeOp = nri::StoreOp::STORE;
        color.clearValue.color.f = {0.035f, 0.045f, 0.065f, 1.0f};

        nri::RenderingDesc rendering{};
        rendering.colors = &color;
        rendering.colorNum = 1;
        m_Core.CmdBeginRendering(*m_FrameCommandBuffer, rendering);
        m_Core.CmdEndRendering(*m_FrameCommandBuffer);
        m_FrameOpen = true;
    }

    void NRIDevice::EndFrame()
    {
        if (!m_SwapChain || !m_FrameCommandBuffer || !m_FrameOpen)
            return;

        EndRendering();
        if(m_Imgui&&m_Streamer){
            ImDrawData* dd=ImGui::GetDrawData();
            if(dd&&dd->Valid&&dd->CmdListsCount>0){
                ImGuiPlatformIO& pio=ImGui::GetPlatformIO();
                nri::CopyImguiDataDesc cp{};cp.drawLists=dd->CmdLists.Data;cp.drawListNum=(uint32_t)dd->CmdLists.Size;cp.textures=pio.Textures.Data;cp.textureNum=(uint32_t)pio.Textures.Size;
                m_ImguiInterface.CmdCopyImguiData(*m_FrameCommandBuffer,*m_Streamer,*m_Imgui,cp);m_StreamerInterface.CmdCopyStreamedData(*m_FrameCommandBuffer,*m_Streamer);
                nri::AttachmentDesc a{};a.descriptor=m_SwapChainViews[m_BackBufferIndex];a.loadOp=nri::LoadOp::LOAD;a.storeOp=nri::StoreOp::STORE;
                nri::RenderingDesc rd{};rd.colors=&a;rd.colorNum=1;m_Core.CmdBeginRendering(*m_FrameCommandBuffer,rd);
                nri::DrawImguiDesc draw{};draw.drawLists=dd->CmdLists.Data;draw.drawListNum=(uint32_t)dd->CmdLists.Size;draw.displaySize={(nri::Dim_t)dd->DisplaySize.x,(nri::Dim_t)dd->DisplaySize.y};draw.hdrScale=1.0f;draw.attachmentFormat=m_SwapChainFormat;draw.linearColor=(m_SwapChainFormat==nri::Format::RGBA8_SRGB||m_SwapChainFormat==nri::Format::BGRA8_SRGB);
                m_ImguiInterface.CmdDrawImgui(*m_FrameCommandBuffer,*m_Imgui,draw);m_Core.CmdEndRendering(*m_FrameCommandBuffer);
            }
        }
        nri::TextureBarrierDesc toPresent{};
        toPresent.texture = m_SwapChainTextures[m_BackBufferIndex];
        toPresent.before = {nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT};
        toPresent.after = {nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE};
        toPresent.mipNum = nri::REMAINING;
        toPresent.layerNum = nri::REMAINING;
        toPresent.planes = nri::PlaneBits::COLOR;
        nri::BarrierDesc barrier{};
        barrier.textures = &toPresent;
        barrier.textureNum = 1;
        m_Core.CmdBarrier(*m_FrameCommandBuffer, barrier);
        m_Core.EndCommandBuffer(*m_FrameCommandBuffer);

        nri::FenceSubmitDesc waitFence{};
        waitFence.fence = m_AcquireSemaphores[m_AcquireIndex];
        waitFence.stages = nri::StageBits::COLOR_ATTACHMENT;

        nri::FenceSubmitDesc releaseFence{};
        releaseFence.fence = m_ReleaseSemaphores[m_BackBufferIndex];

        nri::FenceSubmitDesc frameFence{};
        frameFence.fence = m_FrameFence;
        frameFence.value = 1 + m_FrameIndex;

        nri::FenceSubmitDesc signals[] = {releaseFence, frameFence};
        nri::CommandBuffer* commands[] = {m_FrameCommandBuffer};

        nri::QueueSubmitDesc submit{};
        submit.waitFences = &waitFence;
        submit.waitFenceNum = 1;
        submit.commandBuffers = commands;
        submit.commandBufferNum = 1;
        submit.signalFences = signals;
        submit.signalFenceNum = 2;

        if (m_Core.QueueSubmit(*m_GraphicsQueue, submit) == nri::Result::SUCCESS)
        {
            m_Presented[m_BackBufferIndex] = true;
            const auto result = m_SwapChainInterface.QueuePresent(*m_SwapChain, *m_ReleaseSemaphores[m_BackBufferIndex]);
            m_RebuildSwapChain = result == nri::Result::OUT_OF_DATE;
            m_DeviceFailed = result != nri::Result::SUCCESS && !m_RebuildSwapChain;
            ++m_FrameIndex;
        }
        else { m_DeviceFailed = true; Logger::Error("RHI: frame submission failed."); }
        if(m_Streamer)m_StreamerInterface.EndStreamerFrame(*m_Streamer);
        m_FrameOpen = false;
    }

    void NRIDevice::WaitIdle()
    {
        if (m_Device && m_Core.DeviceWaitIdle)
            m_Core.DeviceWaitIdle(m_Device);
    }

    nri::Queue* NRIDevice::ResolveQueue(QueueType queue) const
    {
        switch (queue)
        {
            case QueueType::Compute:
                return m_ComputeQueue ? m_ComputeQueue : m_GraphicsQueue;
            case QueueType::Copy:
                return m_CopyQueue ? m_CopyQueue : m_GraphicsQueue;
            default:
                return m_GraphicsQueue;
        }
    }

    nri::Format NRIDevice::ToNRIFormat(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGBA8_UNorm: return nri::Format::RGBA8_UNORM;
            case TextureFormat::RGBA8_sRGB: return nri::Format::RGBA8_SRGB;
            case TextureFormat::RGBA16_Float: return nri::Format::RGBA16_SFLOAT;
            case TextureFormat::D24S8: return nri::Format::D24_UNORM_S8_UINT;
            case TextureFormat::D32_Float: return nri::Format::D32_SFLOAT;
            default: return nri::Format::UNKNOWN;
        }
    }

    nri::BufferUsageBits NRIDevice::ToNRIBufferUsage(BufferUsage usage)
    {
        switch (usage)
        {
            case BufferUsage::Vertex: return nri::BufferUsageBits::VERTEX_BUFFER;
            case BufferUsage::Index: return nri::BufferUsageBits::INDEX_BUFFER;
            case BufferUsage::Constant: return nri::BufferUsageBits::CONSTANT_BUFFER;
            case BufferUsage::Storage: return nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
            default: return nri::BufferUsageBits::NONE;
        }
    }

    nri::TextureUsageBits NRIDevice::ToNRITextureUsage(TextureUsage usage)
    {
        nri::TextureUsageBits result = nri::TextureUsageBits::NONE;
        const auto bits = static_cast<uint32_t>(usage);

        if (bits & static_cast<uint32_t>(TextureUsage::Sampled))
            result |= nri::TextureUsageBits::SHADER_RESOURCE;
        if (bits & static_cast<uint32_t>(TextureUsage::RenderTarget))
            result |= nri::TextureUsageBits::COLOR_ATTACHMENT;
        if (bits & static_cast<uint32_t>(TextureUsage::DepthStencil))
            result |= nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT;
        if (bits & static_cast<uint32_t>(TextureUsage::Storage))
            result |= nri::TextureUsageBits::SHADER_RESOURCE_STORAGE;

        return result;
    }
}
