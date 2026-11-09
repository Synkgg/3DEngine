#pragma once

#include "IRHIDevice.h"

#include <memory>

class Window;

namespace Velcryn::RHI
{
    bool Initialize(Window& window, GraphicsAPI api = GraphicsAPI::Vulkan);
    void Shutdown();

    IRHIDevice* GetDevice();
    const IRHIDevice* GetDeviceConst();
    GraphicsAPI GetAPI();
    bool IsInitialized();
}
