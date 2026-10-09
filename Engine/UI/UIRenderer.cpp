#include "UIRenderer.h"

#include "UIText.h"
#include "UIImage.h"
#include "UIButton.h"
#include "UITextInput.h"
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
    struct UIVertex { float x, y, u, v; };
    const UIVertex unitQuad[4] = {
        {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}
    };
    Velcryn::RHI::BufferDesc desc{};
    desc.size = sizeof(unitQuad);
    desc.usage = Velcryn::RHI::BufferUsage::Vertex;
    desc.debugName = "UIUnitQuadVertices";
    m_VertexBuffer = device->CreateBuffer(desc, unitQuad);
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
    pipeline.constantSize = sizeof(float) * 32;
    pipeline.colorFormat = Velcryn::RHI::TextureFormat::RGBA16_Float;
    pipeline.depthFormat = Velcryn::RHI::TextureFormat::D32_Float;
    pipeline.depthTest = false; pipeline.depthWrite = false; pipeline.cullBackFaces = false;
    pipeline.sampledTexture = true; pipeline.displayEncodedTexture = true; pipeline.clampSampler = true; pipeline.alphaBlend = true; pipeline.debugName = "RuntimeUI";
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
    m_ElementOrder.clear();
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
    m_ClipRect = {0, 0, m_LogicalWidth, m_LogicalHeight};
    const float scaleX = static_cast<float>(m_Width) / m_LogicalWidth;
    const float scaleY = static_cast<float>(m_Height) / m_LogicalHeight;
    m_UIScale = std::min(scaleX, scaleY);
    m_UIOffsetX = (static_cast<float>(m_Width) - m_LogicalWidth * m_UIScale) * 0.5f;
    m_UIOffsetY = (static_cast<float>(m_Height) - m_LogicalHeight * m_UIScale) * 0.5f;
}

void UIRenderer::End()
{
    for (const auto& id : m_ElementOrder)
    {
        std::string ancestor = id;
        std::unordered_set<std::string> visited;
        bool visible = true;
        while (!ancestor.empty()) {
            if (!visited.insert(ancestor).second) { visible = false; break; }
            if (auto it = m_Elements.find(ancestor); it != m_Elements.end()) {
                if (!it->second.visible) { visible = false; break; }
                ancestor = it->second.parent;
            } else if (auto it = m_TextElements.find(ancestor); it != m_TextElements.end()) {
                if (!it->second.visible) { visible = false; break; }
                ancestor = it->second.parent;
            } else break;
        }
        if (!visible) continue;
        if (auto it = m_Elements.find(id); it != m_Elements.end()) DrawElement(id, it->second);
        else if (auto it = m_TextElements.find(id); it != m_TextElements.end()) DrawTextElement(id, it->second);
    }
}

void UIRenderer::DrawQuad(float x, float y, float width, float height,
    float u0, float v0, float u1, float v1, const Vec4& color,
    Velcryn::RHI::TextureHandle texture, const Vec4& gradientColor,
    bool useGradient, bool horizontalGradient, float cornerRadius)
{
    if (!m_Pipeline || !m_VertexBuffer || !m_IndexBuffer || width <= 0.0f || height <= 0.0f)
        return;
    struct Constants {
        float color[4]; float viewport[4]; float offset[4];
        float gradient[4]; float style[4]; float rect[4]; float uvRect[4]; float clip[4];
    };
    Constants constants{{color.x,color.y,color.z,color.w},
        {static_cast<float>(m_Width),static_cast<float>(m_Height),m_UIScale,0.0f},
        {m_UIOffsetX,m_UIOffsetY,0.0f,0.0f},
        {gradientColor.x,gradientColor.y,gradientColor.z,gradientColor.w},
        {useGradient ? 1.0f : 0.0f, horizontalGradient ? 1.0f : 0.0f,
         width * m_UIScale, cornerRadius * m_UIScale},
        {x, y, width, height},
        {u0, v0, u1, v1},
        {m_ClipRect.x*m_UIScale+m_UIOffsetX, m_ClipRect.y*m_UIScale+m_UIOffsetY,
         (m_ClipRect.x+m_ClipRect.width)*m_UIScale+m_UIOffsetX,
         (m_ClipRect.y+m_ClipRect.height)*m_UIScale+m_UIOffsetY}};
    if (auto* device = Velcryn::RHI::GetDevice())
    {
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















