#include "NRIDevice.h"

#include "../../../Core/Logger.h"
#include "../../../Platform/SDL/Window.h"

#include <SDL3/SDL.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#include <cstring>
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

        #if defined(_WIN32)
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window.GetNativeWindow());
        void* hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (!hwnd)
        {
            Logger::Error("RHI: SDL did not expose a Win32 window handle.");
            Shutdown();
            return false;
        }

        nri::SwapChainDesc swapDesc{};
        swapDesc.window.windows.hwnd = hwnd;
        swapDesc.queue = m_GraphicsQueue;
        swapDesc.width = static_cast<nri::Dim_t>(window.GetWidth());
        swapDesc.height = static_cast<nri::Dim_t>(window.GetHeight());
        swapDesc.textureNum = 3;
        swapDesc.format = nri::SwapChainFormat::BT709_G22_8BIT;
        swapDesc.flags = nri::SwapChainBits::NONE;
        swapDesc.queuedFrameNum = 2;

        if (m_SwapChainInterface.CreateSwapChain(*m_Device, swapDesc, m_SwapChain) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to create the Vulkan swapchain.");
            Shutdown();
            return false;
        }

        uint32_t textureNum = 0;
        nri::Texture* const* textures = m_SwapChainInterface.GetSwapChainTextures(*m_SwapChain, textureNum);
        m_SwapChainTextures.assign(textures, textures + textureNum);
        if (m_SwapChainTextures.empty())
        {
            Logger::Error("RHI: swapchain returned no backbuffers.");
            Shutdown();
            return false;
        }

        m_SwapChainFormat = m_Core.GetTextureDesc(*m_SwapChainTextures[0]).format;
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
                Shutdown();
                return false;
            }
        }

        if (m_Core.CreateFence(*m_Device, 0, m_FrameFence) != nri::Result::SUCCESS ||
            m_Core.CreateCommandAllocator(*m_GraphicsQueue, m_FrameAllocator) != nri::Result::SUCCESS ||
            m_Core.CreateCommandBuffer(*m_FrameAllocator, m_FrameCommandBuffer) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to create Vulkan frame synchronization resources.");
            Shutdown();
            return false;
        }
#else
        Logger::Error("RHI: Vulkan swapchain platform binding is not implemented on this platform yet.");
        Shutdown();
        return false;
#endif

        nri::StreamerDesc streamerDesc{};
        // NRI validates this optional field even when the static constant ring is unused.
        // Zero-initialization maps to DEVICE here, which is invalid for Streamer constants.
        streamerDesc.constantBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
        streamerDesc.constantBufferSize = 0;
        streamerDesc.dynamicBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
        streamerDesc.dynamicBufferDesc.usage = nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;
        streamerDesc.queuedFrameNum = 2;
        if (m_StreamerInterface.CreateStreamer(*m_Device, streamerDesc, m_Streamer) != nri::Result::SUCCESS) { Logger::Error("RHI: failed to create NRI streamer."); Shutdown(); return false; }
        nri::ImguiDesc imguiDesc{}; imguiDesc.descriptorPoolSize = 2048;
        if (m_ImguiInterface.CreateImgui(*m_Device, imguiDesc, m_Imgui) != nri::Result::SUCCESS) { Logger::Error("RHI: failed to create NRI ImGui renderer."); Shutdown(); return false; }
        Logger::Info(std::string("RHI: NRI Vulkan device + swapchain + ImGui initialized on ") + deviceDesc.adapterDesc.name);
        return true;
    }

    void NRIDevice::Shutdown()
    {
        if (!m_Device)
            return;

        WaitIdle();

        if (m_Imgui) m_ImguiInterface.DestroyImgui(m_Imgui);
        if (m_Streamer) m_StreamerInterface.DestroyStreamer(m_Streamer);
        m_Imgui=nullptr; m_Streamer=nullptr;
        if (m_FrameCommandBuffer) m_Core.DestroyCommandBuffer(m_FrameCommandBuffer);
        if (m_FrameAllocator) m_Core.DestroyCommandAllocator(m_FrameAllocator);
        if (m_FrameFence) m_Core.DestroyFence(m_FrameFence);
        for (nri::Fence* fence : m_AcquireSemaphores) if (fence) m_Core.DestroyFence(fence);
        for (nri::Fence* fence : m_ReleaseSemaphores) if (fence) m_Core.DestroyFence(fence);
        for (nri::Descriptor* view : m_SwapChainViews) if (view) m_Core.DestroyDescriptor(view);
        if (m_SwapChain) m_SwapChainInterface.DestroySwapChain(m_SwapChain);
        m_FrameCommandBuffer = nullptr; m_FrameAllocator = nullptr; m_FrameFence = nullptr; m_SwapChain = nullptr;
        m_AcquireSemaphores.clear(); m_ReleaseSemaphores.clear(); m_SwapChainViews.clear(); m_SwapChainTextures.clear();

        for (CommandSlot& slot : m_CommandLists)
        {
            if (slot.commandBuffer)
                m_Core.DestroyCommandBuffer(slot.commandBuffer);
            if (slot.allocator)
                m_Core.DestroyCommandAllocator(slot.allocator);
        }

        for (TextureSlot& slot : m_Textures)
        {
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
        if (m_Core.CreateBuffer(*m_Device, nriDesc, slot.resource) != nri::Result::SUCCESS)
            return {};

        nri::Buffer* resources[] = {slot.resource};
        nri::ResourceGroupDesc group{};
        group.memoryLocation = desc.cpuVisible ? nri::MemoryLocation::HOST_UPLOAD : nri::MemoryLocation::DEVICE;
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
                if (mapped)
                {
                    std::memcpy(mapped, initialData, static_cast<size_t>(desc.size));
                    m_Core.UnmapBuffer(*slot.resource);
                }
            }
            else
            {
                nri::BufferUploadDesc upload{};
                upload.data = initialData;
                upload.buffer = slot.resource;
                upload.after = {};
                m_Helper.UploadData(*m_GraphicsQueue, nullptr, 0, &upload, 1);
            }
        }

        m_Buffers.push_back(std::move(slot));
        return {static_cast<uint32_t>(m_Buffers.size()), m_Buffers.back().generation};
    }

    void NRIDevice::DestroyBuffer(BufferHandle handle)
    {
        if (!handle || handle.index > m_Buffers.size())
            return;

        BufferSlot& slot = m_Buffers[handle.index - 1];
        if (slot.generation != handle.generation || !slot.resource)
            return;

        m_Core.DestroyBuffer(slot.resource);
        for (nri::Memory* memory : slot.allocations)
            m_Core.FreeMemory(memory);

        slot.resource = nullptr;
        slot.allocations.clear();
        ++slot.generation;
    }

    TextureHandle NRIDevice::CreateTexture(const TextureDesc& desc, const void* initialData, std::size_t initialDataSize)
    {
        if (!m_Device || desc.width == 0 || desc.height == 0)
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
            if(m_Core.CreateTextureView(view,slot.shaderResource)!=nri::Result::SUCCESS){for(auto* m:slot.allocations)m_Core.FreeMemory(m);m_Core.DestroyTexture(slot.resource);return{};}
        }
        if(initialData&&initialDataSize){
            const nri::FormatProps& props=*nri::nriGetFormatProps(nriDesc.format);
            nri::TextureSubresourceUploadDesc sub{};sub.slices=initialData;sub.sliceNum=1;sub.rowPitch=desc.width*props.stride;sub.slicePitch=sub.rowPitch*desc.height;
            nri::TextureUploadDesc upload{};upload.subresources=&sub;upload.texture=slot.resource;upload.after={nri::AccessBits::SHADER_RESOURCE,nri::Layout::SHADER_RESOURCE,nri::StageBits::ALL};upload.planes=props.isDepth?nri::PlaneBits::DEPTH:nri::PlaneBits::COLOR;
            if(m_Helper.UploadData(*m_GraphicsQueue,&upload,1,nullptr,0)!=nri::Result::SUCCESS){if(slot.shaderResource)m_Core.DestroyDescriptor(slot.shaderResource);for(auto* m:slot.allocations)m_Core.FreeMemory(m);m_Core.DestroyTexture(slot.resource);return{};}
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

        if (slot.shaderResource) m_Core.DestroyDescriptor(slot.shaderResource);
        m_Core.DestroyTexture(slot.resource);
        for (nri::Memory* memory : slot.allocations)
            m_Core.FreeMemory(memory);

        slot.shaderResource = nullptr;
        slot.resource = nullptr;
        slot.allocations.clear();
        ++slot.generation;
    }

    std::uint64_t NRIDevice::GetImGuiTextureID(TextureHandle handle) const {
        if(!handle||handle.index>m_Textures.size())return 0;const TextureSlot& s=m_Textures[handle.index-1];
        return (s.generation==handle.generation&&s.shaderResource)?reinterpret_cast<std::uint64_t>(s.shaderResource):0;
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

        slot.recording = true;
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
        if (slot.generation != handle.generation || !slot.commandBuffer || slot.recording)
            return;

        nri::CommandBuffer* commandBuffers[] = {slot.commandBuffer};
        nri::QueueSubmitDesc submit{};
        submit.commandBuffers = commandBuffers;
        submit.commandBufferNum = 1;

        m_Core.QueueSubmit(*m_GraphicsQueue, submit);
    }

    void NRIDevice::BeginFrame()
    {
        if (!m_SwapChain || !m_FrameCommandBuffer || m_FrameOpen)
            return;

        const uint64_t completedFrame = m_FrameIndex >= 2 ? 1 + m_FrameIndex - 2 : 0;
        m_Core.Wait(*m_FrameFence, completedFrame);

        m_AcquireIndex = static_cast<uint32_t>(m_FrameIndex % m_AcquireSemaphores.size());
        if (m_SwapChainInterface.AcquireNextTexture(*m_SwapChain, *m_AcquireSemaphores[m_AcquireIndex], m_BackBufferIndex) != nri::Result::SUCCESS)
            return;

        m_Core.ResetCommandAllocator(*m_FrameAllocator);
        if (m_Core.BeginCommandBuffer(*m_FrameCommandBuffer, nullptr) != nri::Result::SUCCESS)
            return;

        nri::TextureBarrierDesc toColor{};
        toColor.texture = m_SwapChainTextures[m_BackBufferIndex];
        toColor.before = {nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE};
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

        if(m_Imgui&&m_Streamer){
            ImDrawData* dd=ImGui::GetDrawData();
            if(dd&&dd->Valid&&dd->CmdListsCount>0){
                ImGuiPlatformIO& pio=ImGui::GetPlatformIO();
                nri::CopyImguiDataDesc cp{};cp.drawLists=dd->CmdLists.Data;cp.drawListNum=(uint32_t)dd->CmdLists.Size;cp.textures=pio.Textures.Data;cp.textureNum=(uint32_t)pio.Textures.Size;
                m_ImguiInterface.CmdCopyImguiData(*m_FrameCommandBuffer,*m_Streamer,*m_Imgui,cp);m_StreamerInterface.CmdCopyStreamedData(*m_FrameCommandBuffer,*m_Streamer);
                nri::AttachmentDesc a{};a.descriptor=m_SwapChainViews[m_BackBufferIndex];a.loadOp=nri::LoadOp::LOAD;a.storeOp=nri::StoreOp::STORE;
                nri::RenderingDesc rd{};rd.colors=&a;rd.colorNum=1;m_Core.CmdBeginRendering(*m_FrameCommandBuffer,rd);
                nri::DrawImguiDesc draw{};draw.drawLists=dd->CmdLists.Data;draw.drawListNum=(uint32_t)dd->CmdLists.Size;draw.displaySize={(nri::Dim_t)dd->DisplaySize.x,(nri::Dim_t)dd->DisplaySize.y};draw.hdrScale=1.0f;draw.attachmentFormat=m_SwapChainFormat;draw.linearColor=true;
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
            m_SwapChainInterface.QueuePresent(*m_SwapChain, *m_ReleaseSemaphores[m_BackBufferIndex]);

        if(m_Streamer)m_StreamerInterface.EndStreamerFrame(*m_Streamer);
        ++m_FrameIndex;
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
