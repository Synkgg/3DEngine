#pragma once

#include "../Graphics/RHI/RHITypes.h"
#include "UICanvas.h"
#include "UILayout.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <array>

class Texture2D;
class UIText;
class UIButton;
class UITextInput;
class UISlider;
class UIProgressBar;
class Renderer;
class Input;
class AudioEngine;

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
        float viewportHeight,
        float deltaTime = 1.0f / 60.0f
    );
    bool HasTextInputFocus() const { return m_FocusedTextInput != nullptr; }

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
    void SetAudioEngine(AudioEngine* audio) { m_Audio = audio; }

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
    void DrawTextInput(const UITextInput& input, const UIRect& rect);
    void DrawSlider(const UISlider& slider, const UIRect& rect);
    void DrawProgressBar(const UIProgressBar& progress, const UIRect& rect);

    bool InitializeFontAtlas();
    void DrawFontGlyph(
        float x, float y, float width, float height,
        float u0, float v0, float u1, float v1,
        const Vec4& color
    );
    void DrawQuad(float x, float y, float width, float height,
        float u0, float v0, float u1, float v1,
        const Vec4& color, Velcryn::RHI::TextureHandle texture,
        const Vec4& gradientColor = Vec4(), bool useGradient = false,
        bool horizontalGradient = false, float cornerRadius = 0.0f);

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
    UIWidget* FindTopControl(UIWidget& widget, const UIRect& parentRect, const Vec2& mouse);

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
    Velcryn::RHI::BufferHandle m_VertexBuffer{};
    Velcryn::RHI::BufferHandle m_IndexBuffer{};
    Velcryn::RHI::PipelineHandle m_Pipeline{};
    Velcryn::RHI::TextureHandle m_FontTexture{};
    Velcryn::RHI::TextureHandle m_WhiteTexture{};
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
    UIRect m_ClipRect{};

    std::unordered_map<
        std::string,
        Element
    > m_Elements;
    // Preserve script creation order so backgrounds cannot randomly cover images.
    std::vector<std::string> m_ElementOrder;

    std::unordered_map<
        std::string,
        TextElement
    > m_TextElements;

    bool m_MouseInteractionEnabled = false;
    const UICanvas* m_InputCanvas = nullptr;
    std::uint64_t m_InputCanvasRevision = 0;
    UIButton* m_PressedCanvasButton = nullptr;
    UITextInput* m_FocusedTextInput = nullptr;
    UISlider* m_DraggedSlider = nullptr;
    std::uint64_t m_PressedButtonId = 0, m_FocusedTextId = 0, m_DraggedSliderId = 0;
    float m_BackspaceHeldTime = 0.0f;
    float m_DeleteHeldTime = 0.0f;
    float m_BackspaceRepeatTime = 0.0f;
    float m_DeleteRepeatTime = 0.0f;
    AudioEngine* m_Audio = nullptr;
};
