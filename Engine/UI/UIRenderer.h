#pragma once

#include "../Graphics/Shader.h"
#include "UICanvas.h"
#include "UILayout.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <array>

class Texture2D;
class UIText;
class Renderer;
class Input;

class UIRenderer
{
public:
    UIRenderer();
    ~UIRenderer();

    bool Initialize();
    void Shutdown();

    void Resize(
        unsigned int width,
        unsigned int height
    );

    void SetLogicalSize(
        float width,
        float height
    );

    void Begin();
    void End();

    void RenderCanvas(
        UICanvas& canvas,
        Renderer* renderer = nullptr
    );

    void UpdateInput(
        UICanvas& canvas,
        const Input& input,
        float viewportX,
        float viewportY,
        float viewportWidth,
        float viewportHeight
    );

    bool ViewportToCanvas(
        float mouseX,
        float mouseY,
        float viewportWidth,
        float viewportHeight,
        Vec2& result
    ) const;

    // ---------------------------------------------------------
    // Runtime / scripted UI
    // ---------------------------------------------------------

    void DrawRect(
        float x,
        float y,
        float width,
        float height,
        float red,
        float green,
        float blue,
        float alpha
    );

    void SetRect(
        const std::string& id,
        float x,
        float y,
        float width,
        float height,
        float red,
        float green,
        float blue,
        float alpha
    );

    void SetText(
        const std::string& id,
        const std::string& text,
        float x,
        float y,
        float scale,
        float red,
        float green,
        float blue,
        float alpha
    );

    void SetImage(
        const std::string& id,
        Texture2D* texture,
        float x,
        float y,
        float width,
        float height,
        float red,
        float green,
        float blue,
        float alpha
    );

    void SetVisible(
        const std::string& id,
        bool visible
    );

    void SetParent(
        const std::string& id,
        const std::string& parentId
    );

    void Remove(
        const std::string& id
    );

    void Clear();

    void SetMouseInteractionEnabled(
        bool enabled
    );

    bool IsMouseInteractionEnabled() const;

private:
    // ---------------------------------------------------------
    // Canvas UI
    // ---------------------------------------------------------

    void RenderCanvasWidget(
        const UIWidget& widget,
        const UIRect& parentRect,
        Renderer* renderer
    );

    void DrawCanvasWidget(
        const UIWidget& widget,
        const UIRect& rect,
        Renderer* renderer
    );

    void DrawCanvasText(
        const UIText& text,
        const UIRect& rect
    );

    bool InitializeFontAtlas();
    void DrawFontGlyph(
        float x, float y, float width, float height,
        float u0, float v0, float u1, float v1,
        const Vec4& color
    );

    struct FontGlyph
    {
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float xoff = 0, yoff = 0, xadvance = 0;
    };

    // ---------------------------------------------------------
    // Runtime UI
    // ---------------------------------------------------------

    struct Element
    {
        float x = 0.0f;
        float y = 0.0f;

        float width = 0.0f;
        float height = 0.0f;

        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        float alpha = 1.0f;

        Texture2D* texture = nullptr;

        std::string parent;

        bool visible = true;
    };

    struct TextElement
    {
        std::string text;

        float x = 0.0f;
        float y = 0.0f;

        float scale = 1.0f;

        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        float alpha = 1.0f;

        std::string parent;

        bool visible = true;
    };

    void DrawElement(
        const std::string& id,
        const Element& element
    );

    void DrawTextElement(
        const std::string& id,
        const TextElement& element
    );

    void ResetButtonInput(UIWidget& widget);
    UIButton* FindTopButton(UIWidget& widget, const UIRect& parentRect, const Vec2& mouse);

    bool GetAbsolutePosition(
        const std::string& id,
        float& x,
        float& y,
        std::unordered_set<std::string>& resolving
    ) const;

    void DrawTextPixel(
        float x,
        float y,
        float size,
        float red,
        float green,
        float blue,
        float alpha
    );

    unsigned char GetGlyphRow(
        char character,
        int row
    ) const;

private:
    Shader m_Shader;

    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_FontTexture = 0;
    static constexpr int FontAtlasWidth = 1024;
    static constexpr int FontAtlasHeight = 1024;
    static constexpr float FontBakeSize = 48.0f;
    std::array<FontGlyph, 95> m_FontGlyphs{};

    // Actual framebuffer size.
    unsigned int m_Width = 1;
    unsigned int m_Height = 1;

    // Logical UI coordinate space.
    float m_LogicalWidth = 1280.0f;
    float m_LogicalHeight = 720.0f;

    // Design resolution.
    float m_DesignWidth = 1280.0f;
    float m_DesignHeight = 720.0f;

    // Calculated in Begin().
    float m_UIScale = 1.0f;

    float m_UIOffsetX = 0.0f;
    float m_UIOffsetY = 0.0f;

    std::unordered_map<
        std::string,
        Element
    > m_Elements;

    std::unordered_map<
        std::string,
        TextElement
    > m_TextElements;

    bool m_MouseInteractionEnabled = false;
    UIButton* m_PressedCanvasButton = nullptr;
};