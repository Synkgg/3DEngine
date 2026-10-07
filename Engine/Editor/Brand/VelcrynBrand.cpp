#include "VelcrynBrand.h"

#include "../../Graphics/Texture2D.h"
#include "../../Graphics/RHI/RHI.h"

#include <cstdint>
#include <memory>

namespace
{
    std::unique_ptr<Texture2D> s_LogoTexture;
}

namespace Velcryn::Editor::Brand
{
    bool Initialize()
    {
        if (s_LogoTexture && s_LogoTexture->IsLoaded())
            return true;

        s_LogoTexture = std::make_unique<Texture2D>();

        if (!s_LogoTexture->Load("Engine/Branding/VelcrynLogo.png"))
        {
            s_LogoTexture.reset();
            return false;
        }

        return true;
    }

    void Shutdown()
    {
        s_LogoTexture.reset();
    }

    bool HasLogoTexture()
    {
        return s_LogoTexture && s_LogoTexture->IsLoaded();
    }

    void DrawLogoMark(ImDrawList* drawList, ImVec2 position, float size)
    {
        if (!HasLogoTexture())
            return;

        auto* device = Velcryn::RHI::GetDevice();
        if (!device)
            return;

        const std::uint64_t descriptor =
            device->GetImGuiTextureID(s_LogoTexture->GetHandle());
        if (descriptor == 0)
            return;

        const ImTextureID texture = static_cast<ImTextureID>(descriptor);
        drawList->AddImage(
            texture,
            position,
            ImVec2(position.x + size, position.y + size),
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f)
        );
    }
}
