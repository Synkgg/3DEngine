#include "UIRenderer.h"

#include "UIText.h"
#include "UIImage.h"
#include "UIButton.h"
#include "UITextInput.h"
#include "UISlider.h"
#include "UISlider.h"
#include "../Platform/SDL/Input.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/RHI/RHI.h"
#include "../Core/Logger.h"
#include "../Audio/AudioEngine.h"
#include "../Editor/Fonts/InterFont.h"


#include <algorithm>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

namespace
{
    const char* UI_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec2 a_Position;
        layout(location = 1) in vec2 a_UV;

        uniform float u_ScreenWidth;
        uniform float u_ScreenHeight;

        uniform float u_UIScale;
        uniform float u_UIOffsetX;
        uniform float u_UIOffsetY;

        out vec2 v_UV;

        void main()
        {
            float screenX =
                a_Position.x * u_UIScale +
                u_UIOffsetX;

            float screenY =
                a_Position.y * u_UIScale +
                u_UIOffsetY;

            float ndcX =
                (screenX / u_ScreenWidth) * 2.0 - 1.0;

            float ndcY =
                1.0 -
                (screenY / u_ScreenHeight) * 2.0;

            gl_Position =
                vec4(ndcX, ndcY, 0.0, 1.0);

            v_UV = a_UV;
        }
    )";

    const char* UI_FRAGMENT_SHADER = R"(
        #version 450 core

        in vec2 v_UV;

        uniform vec4 u_Color;
        uniform vec4 u_GradientColor;
        uniform int u_UseGradient;
        uniform int u_GradientDirection;
        uniform sampler2D u_Texture;
        uniform int u_UseTexture;
        uniform vec2 u_RectSize;
        uniform float u_CornerRadius;

        out vec4 FragColor;

        void main()
        {
            // UI quads use a flipped V for texture sampling, but rounding only
            // needs a stable 0..1 local coordinate inside the rectangle.
            if (u_CornerRadius > 0.001)
            {
                vec2 localUV = vec2(v_UV.x, 1.0 - v_UV.y);
                vec2 p = localUV * u_RectSize;
                vec2 halfSize = u_RectSize * 0.5;
                float radius = min(u_CornerRadius, min(halfSize.x, halfSize.y));
                vec2 q = abs(p - halfSize) - (halfSize - vec2(radius));
                float sd = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
                if (sd > 0.0) discard;
            }
            float gradientT = u_GradientDirection == 1 ? v_UV.x : v_UV.y;
            vec4 baseColor = u_UseGradient != 0 ? mix(u_Color, u_GradientColor, gradientT) : u_Color;
            if (u_UseTexture != 0)
            {
                FragColor =
                    texture(u_Texture, v_UV) *
                    baseColor;
            }
            else
            {
                FragColor = baseColor;
            }
        }
    )";
}

UIRenderer::UIRenderer()
{
}

UIRenderer::~UIRenderer()
{
    Shutdown();
}

bool UIRenderer::Initialize()
{
    if (m_VertexBuffer) return true;
    auto* device = Velcryn::RHI::GetDevice();
    if (!device) return false;
    Velcryn::RHI::BufferDesc desc{};
    desc.size = sizeof(float) * 24;
    desc.usage = Velcryn::RHI::BufferUsage::Vertex;
    desc.cpuVisible = true;
    desc.debugName = "UIQuadVertices";
    m_VertexBuffer = device->CreateBuffer(desc);
    if (!m_VertexBuffer) return false;
    if (!InitializeFontAtlas()) Logger::Warning("Inter runtime font could not be loaded; UI renderer will continue without canvas text.");
    return true;
}

void UIRenderer::Shutdown()
{
    if (auto* device = Velcryn::RHI::GetDevice()) {
        if (m_FontTexture) device->DestroyTexture(m_FontTexture);
        if (m_VertexBuffer) device->DestroyBuffer(m_VertexBuffer);
    }
    m_FontTexture = {};
    m_VertexBuffer = {};
    m_Elements.clear();
    m_TextElements.clear();
}

void UIRenderer::Resize(
    unsigned int width,
    unsigned int height)
{
    m_Width =
        std::max(
            1u,
            width
        );

    m_Height =
        std::max(
            1u,
            height
        );
}

void UIRenderer::SetLogicalSize(
    float width,
    float height)
{
    if (width <= 0.0f ||
        height <= 0.0f)
    {
        return;
    }

    m_LogicalWidth = width;
    m_LogicalHeight = height;
}

void UIRenderer::Begin()
{
    const float scaleX = static_cast<float>(m_Width) / m_LogicalWidth;
    const float scaleY = static_cast<float>(m_Height) / m_LogicalHeight;
    m_UIScale = std::min(scaleX, scaleY);
    m_UIOffsetX = (static_cast<float>(m_Width) - m_LogicalWidth * m_UIScale) * 0.5f;
    m_UIOffsetY = (static_cast<float>(m_Height) - m_LogicalHeight * m_UIScale) * 0.5f;
}

void UIRenderer::End()
{
    // Runtime/canvas UI draw submission is owned by the Vulkan scene/UI pass.
}

bool UIRenderer::InitializeFontAtlas()
{
    if (g_InterFontDataSize == 0)
        return false;

    std::vector<unsigned char> bitmap(
        FontAtlasWidth * FontAtlasHeight, 0);

    stbtt_bakedchar baked[95]{};
    const int result = stbtt_BakeFontBitmap(
        g_InterFontData, 0, FontBakeSize,
        bitmap.data(), FontAtlasWidth, FontAtlasHeight,
        32, 95, baked);
    if (result <= 0) return false;

    std::vector<unsigned char> rgba(
        FontAtlasWidth * FontAtlasHeight * 4, 255);
    for (int i = 0; i < FontAtlasWidth * FontAtlasHeight; ++i)
        rgba[i * 4 + 3] = bitmap[i];

    auto* device = Velcryn::RHI::GetDevice();
    if (!device) return false;
    Velcryn::RHI::TextureDesc textureDesc{};
    textureDesc.width = FontAtlasWidth;
    textureDesc.height = FontAtlasHeight;
    textureDesc.format = Velcryn::RHI::TextureFormat::RGBA8_UNorm;
    textureDesc.usage = Velcryn::RHI::TextureUsage::Sampled | Velcryn::RHI::TextureUsage::TransferDestination;
    textureDesc.debugName = "UIFontAtlas";
    m_FontTexture = device->CreateTexture(textureDesc, rgba.data(), rgba.size());
    if (!m_FontTexture) return false;

    for (int i = 0; i < 95; ++i)
    {
        FontGlyph& glyph = m_FontGlyphs[i];
        glyph.x0 = static_cast<float>(baked[i].x0);
        glyph.y0 = static_cast<float>(baked[i].y0);
        glyph.x1 = static_cast<float>(baked[i].x1);
        glyph.y1 = static_cast<float>(baked[i].y1);
        glyph.xoff = baked[i].xoff;
        glyph.yoff = baked[i].yoff;
        glyph.xadvance = baked[i].xadvance;
    }
    return true;
}




// =============================================================
// Runtime / scripted UI
// =============================================================















