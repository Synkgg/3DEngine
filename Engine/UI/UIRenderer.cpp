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
#include <UI.vert.h>
#include <UI.frag.h>

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
    desc.size = sizeof(float) * 16;
    desc.usage = Velcryn::RHI::BufferUsage::Vertex;
    desc.cpuVisible = true;
    desc.debugName = "UIQuadVertices";
    m_VertexBuffer = device->CreateBuffer(desc);
    if (!m_VertexBuffer) return false;

    const std::uint32_t indices[6] = {0, 1, 2, 2, 3, 0};
    Velcryn::RHI::BufferDesc indexDesc{};
    indexDesc.size = sizeof(indices); indexDesc.usage = Velcryn::RHI::BufferUsage::Index;
    indexDesc.debugName = "UIQuadIndices";
    m_IndexBuffer = device->CreateBuffer(indexDesc, indices);
    if (!m_IndexBuffer) { Shutdown(); return false; }

    const Velcryn::RHI::VertexAttribute attributes[] = {
        {0, 0, Velcryn::RHI::VertexFormat::Float2},
        {1, sizeof(float) * 2, Velcryn::RHI::VertexFormat::Float2}
    };
    Velcryn::RHI::GraphicsPipelineDesc pipeline{};
    pipeline.vertexShader = UI_vert; pipeline.fragmentShader = UI_frag;
    pipeline.attributes = attributes; pipeline.vertexStride = sizeof(float) * 4;
    pipeline.constantSize = sizeof(float) * 12;
    pipeline.colorFormat = Velcryn::RHI::TextureFormat::RGBA16_Float;
    pipeline.depthFormat = Velcryn::RHI::TextureFormat::D32_Float;
    pipeline.depthTest = false; pipeline.depthWrite = false; pipeline.cullBackFaces = false;
    pipeline.sampledTexture = true; pipeline.alphaBlend = true; pipeline.debugName = "RuntimeUI";
    m_Pipeline = device->CreateGraphicsPipeline(pipeline);
    if (!m_Pipeline) { Shutdown(); return false; }

    const std::uint8_t white[4] = {255,255,255,255};
    Velcryn::RHI::TextureDesc whiteDesc{};
    whiteDesc.width = whiteDesc.height = 1;
    whiteDesc.format = Velcryn::RHI::TextureFormat::RGBA8_UNorm;
    whiteDesc.usage = Velcryn::RHI::TextureUsage::Sampled | Velcryn::RHI::TextureUsage::TransferDestination;
    whiteDesc.debugName = "UIWhiteTexture";
    m_WhiteTexture = device->CreateTexture(whiteDesc, white, sizeof(white));
    if (!m_WhiteTexture) { Shutdown(); return false; }

    if (!InitializeFontAtlas()) Logger::Warning("Inter runtime font could not be loaded; UI renderer will continue without canvas text.");
    return true;
}

void UIRenderer::Shutdown()
{
    if (auto* device = Velcryn::RHI::GetDevice()) {
        if (m_FontTexture) device->DestroyTexture(m_FontTexture);
        if (m_WhiteTexture) device->DestroyTexture(m_WhiteTexture);
        if (m_Pipeline) device->DestroyPipeline(m_Pipeline);
        if (m_IndexBuffer) device->DestroyBuffer(m_IndexBuffer);
        if (m_VertexBuffer) device->DestroyBuffer(m_VertexBuffer);
    }
    m_FontTexture = {};
    m_WhiteTexture = {};
    m_Pipeline = {};
    m_IndexBuffer = {};
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
    for (const auto& [id, element] : m_Elements)
        if (element.visible) DrawElement(id, element);
    for (const auto& [id, element] : m_TextElements)
        if (element.visible) DrawTextElement(id, element);
}

void UIRenderer::DrawQuad(float x, float y, float width, float height,
    float u0, float v0, float u1, float v1, const Vec4& color,
    Velcryn::RHI::TextureHandle texture)
{
    if (!m_Pipeline || !m_VertexBuffer || !m_IndexBuffer || width <= 0.0f || height <= 0.0f)
        return;
    struct Vertex { float x, y, u, v; };
    const Vertex vertices[4] = {
        {x, y, u0, v0}, {x + width, y, u1, v0},
        {x + width, y + height, u1, v1}, {x, y + height, u0, v1}
    };
    struct Constants { float color[4]; float viewport[4]; float offset[4]; };
    Constants constants{{color.x,color.y,color.z,color.w},
        {static_cast<float>(m_Width),static_cast<float>(m_Height),m_UIScale,0.0f},
        {m_UIOffsetX,m_UIOffsetY,0.0f,0.0f}};
    if (auto* device = Velcryn::RHI::GetDevice())
    {
        if (!device->UpdateBuffer(m_VertexBuffer, vertices, sizeof(vertices))) return;
        device->DrawIndexed(m_Pipeline, m_VertexBuffer, m_IndexBuffer, 6,
            &constants, sizeof(constants), 1, texture ? texture : m_WhiteTexture);
    }
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















