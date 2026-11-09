#include "RHI.h"
#include "RHIDeviceFactory.h"

#include "../../Core/Logger.h"

namespace
{
    std::unique_ptr<Velcryn::RHI::IRHIDevice> s_Device;
}

namespace Velcryn::RHI
{
    bool Initialize(Window& window, GraphicsAPI api)
    {
        if (s_Device)
            return true;

        s_Device = CreateDevice(api);
        if (!s_Device)
        {
            Logger::Error("RHI: no backend is available for the requested graphics API.");
            return false;
        }

        if (!s_Device->Initialize(window))
        {
            s_Device.reset();
            return false;
        }

        return true;
    }

    void Shutdown()
    {
        if (!s_Device)
            return;

        s_Device->Shutdown();
        s_Device.reset();
    }

    IRHIDevice* GetDevice()
    {
        return s_Device.get();
    }

    const IRHIDevice* GetDeviceConst()
    {
        return s_Device.get();
    }

    GraphicsAPI GetAPI()
    {
        return s_Device ? s_Device->GetAPI() : GraphicsAPI::Unknown;
    }

    bool IsInitialized()
    {
        return s_Device != nullptr;
    }
}
