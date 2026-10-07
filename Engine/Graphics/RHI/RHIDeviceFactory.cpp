#include "RHIDeviceFactory.h"

namespace Velcryn::RHI
{
    std::unique_ptr<IRHIDevice> CreateDevice(GraphicsAPI)
    {
        // Backend wiring follows after the interface is established. Returning
        // null keeps the existing OpenGL renderer operational during migration.
        return nullptr;
    }
}
