#include "VelcrynBrand.h"

#include "../../Graphics/Texture2D.h"

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

        const ImTextureID texture = static_cast<ImTextureID>(
            static_cast<std::uintptr_t>(s_LogoTexture->GetID())
        );

        // Texture2D flips source images for OpenGL material UVs. Flip the
        // ImGui UVs back so branding assets retain their authored orientation.
        drawList->AddImage(
            texture,
            position,
            ImVec2(position.x + size, position.y + size),
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f)
        );
    }
}
