#include "NRIDevice.h"

#include "../../../Core/Logger.h"

namespace Velcryn::RHI
{
    NRIDevice::~NRIDevice()
    {
        Shutdown();
    }

    bool NRIDevice::Initialize(Window&)
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

        if (nri::nriGetInterface(*m_Device, NRI_INTERFACE(nri::CoreInterface), &m_Core) != nri::Result::SUCCESS)
        {
            Logger::Error("RHI: failed to acquire NRI CoreInterface.");
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
        m_Capabilities.api = GraphicsAPI::Vulkan;
        m_Capabilities.bindlessResources = true;
        m_Capabilities.timelineSemaphores = true;
        m_Capabilities.maxTextureDimension2D = deviceDesc.textureMaxDim;
        m_Capabilities.maxColorAttachments = deviceDesc.colorAttachmentMaxNum;

        Logger::Info("RHI: NRI Vulkan device initialized.");
        return true;
    }

    void NRIDevice::Shutdown()
    {
        if (!m_Device)
            return;

        WaitIdle();
        nri::nriDestroyDevice(m_Device);
        m_Device = nullptr;
        m_GraphicsQueue = nullptr;
        m_Core = {};
        m_Capabilities = {};
    }

    BufferHandle NRIDevice::CreateBuffer(const BufferDesc&, const void*)
    {
        // Resource allocation lands next; device/queue ownership is now live.
        return {};
    }

    void NRIDevice::DestroyBuffer(BufferHandle)
    {
    }

    TextureHandle NRIDevice::CreateTexture(const TextureDesc&, const void*, std::size_t)
    {
        return {};
    }

    void NRIDevice::DestroyTexture(TextureHandle)
    {
    }

    CommandListHandle NRIDevice::BeginCommandList(QueueType)
    {
        return {};
    }

    void NRIDevice::EndCommandList(CommandListHandle)
    {
    }

    void NRIDevice::Submit(CommandListHandle)
    {
    }

    void NRIDevice::BeginFrame()
    {
    }

    void NRIDevice::EndFrame()
    {
    }

    void NRIDevice::WaitIdle()
    {
        if (m_Device && m_Core.DeviceWaitIdle)
            m_Core.DeviceWaitIdle(m_Device);
    }
}
