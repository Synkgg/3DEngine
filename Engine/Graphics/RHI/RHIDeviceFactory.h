#pragma once

#include "IRHIDevice.h"
#include <memory>

namespace Velcryn::RHI
{
    // Backend creation stays centralized here. NRI/Vulkan will be the first
    // explicit backend; higher-level renderer code must not include NRI headers.
    std::unique_ptr<IRHIDevice> CreateDevice(GraphicsAPI api);
}
