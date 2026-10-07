#include "RHIDeviceFactory.h"
#include "NRI/NRIDevice.h"

namespace Velcryn::RHI
{
    std::unique_ptr<IRHIDevice> CreateDevice(GraphicsAPI api)
    {
        switch (api)
        {
            case GraphicsAPI::Vulkan:
                return std::make_unique<NRIDevice>();

            default:
                return nullptr;
        }
    }
}
