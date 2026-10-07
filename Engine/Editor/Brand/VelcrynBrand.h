#pragma once

#include <imgui.h>

namespace Velcryn::Editor::Brand
{
    bool Initialize();
    void Shutdown();
    bool HasLogoTexture();
    void DrawLogoMark(ImDrawList* drawList, ImVec2 position, float size);
}
