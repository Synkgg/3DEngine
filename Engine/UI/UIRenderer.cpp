#include "UIRenderer.h"

#include "UIText.h"
#include "UIImage.h"
#include "UIButton.h"
#include "UITextInput.h"
#include "UISlider.h"
#include "../Platform/SDL/Input.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture2D.h"
#include "../Core/Logger.h"
#include "../Audio/AudioEngine.h"

#include <glad/gl.h>

#include <algorithm>
#include <string>
#include <fstream>
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

        out vec4 FragColor;

        void main()
        {
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
    if (m_VAO != 0)
    {
        return true;
    }

    if (!m_Shader.Initialize(
        UI_VERTEX_SHADER,
        UI_FRAGMENT_SHADER))
    {
        return false;
    }

    glGenVertexArrays(
        1,
        &m_VAO
    );

    glGenBuffers(
        1,
        &m_VBO
    );

    glBindVertexArray(m_VAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VBO
    );

    const float vertices[24] =
    {
        // Position      UV
        0.0f, 0.0f,      0.0f, 0.0f,
        1.0f, 0.0f,      1.0f, 0.0f,
        1.0f, 1.0f,      1.0f, 1.0f,

        0.0f, 0.0f,      0.0f, 0.0f,
        1.0f, 1.0f,      1.0f, 1.0f,
        0.0f, 1.0f,      0.0f, 1.0f
    };

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 4,
        reinterpret_cast<void*>(0)
    );

    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 4,
        reinterpret_cast<void*>(
            sizeof(float) * 2
            )
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        0
    );

    glBindVertexArray(0);

    // The runtime font is optional at renderer startup. Visual UI must still
    // initialize even when the editor is launched from a build directory
    // where the source-tree font path is unavailable.
    if (!InitializeFontAtlas())
    {
        Logger::Warning(
            "Inter runtime font could not be loaded; UI renderer will continue without canvas text.");
    }

    return true;
}

void UIRenderer::Shutdown()
{
    if (m_FontTexture != 0)
    {
        glDeleteTextures(1, &m_FontTexture);
        m_FontTexture = 0;
    }

    if (m_VBO != 0)
    {
        glDeleteBuffers(
            1,
            &m_VBO
        );

        m_VBO = 0;
    }

    if (m_VAO != 0)
    {
        glDeleteVertexArrays(
            1,
            &m_VAO
        );

        m_VAO = 0;
    }

    m_Shader.Shutdown();

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
    if (m_VAO == 0)
    {
        return;
    }

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    const float scaleX =
        static_cast<float>(m_Width) /
        m_LogicalWidth;

    const float scaleY =
        static_cast<float>(m_Height) /
        m_LogicalHeight;

    m_UIScale =
        std::min(
            scaleX,
            scaleY
        );

    m_UIOffsetX =
        (
            static_cast<float>(m_Width) -
            m_LogicalWidth * m_UIScale
            ) * 0.5f;

    m_UIOffsetY =
        (
            static_cast<float>(m_Height) -
            m_LogicalHeight * m_UIScale
            ) * 0.5f;

    m_Shader.Bind();

    m_Shader.SetFloat(
        "u_ScreenWidth",
        static_cast<float>(m_Width)
    );

    m_Shader.SetFloat(
        "u_ScreenHeight",
        static_cast<float>(m_Height)
    );

    m_Shader.SetFloat(
        "u_UIScale",
        m_UIScale
    );

    m_Shader.SetFloat(
        "u_UIOffsetX",
        m_UIOffsetX
    );

    m_Shader.SetFloat(
        "u_UIOffsetY",
        m_UIOffsetY
    );
}

void UIRenderer::End()
{
    if (m_VAO == 0)
    {
        return;
    }

    for (const auto& element :
        m_Elements)
    {
        DrawElement(
            element.first,
            element.second
        );
    }

    for (const auto& text :
        m_TextElements)
    {
        DrawTextElement(
            text.first,
            text.second
        );
    }

    m_Shader.Unbind();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

bool UIRenderer::ViewportToCanvas(
    float mouseX,
    float mouseY,
    float viewportWidth,
    float viewportHeight,
    Vec2& result) const
{
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f ||
        m_LogicalWidth <= 0.0f || m_LogicalHeight <= 0.0f)
        return false;

    const float scale = std::min(
        viewportWidth / m_LogicalWidth,
        viewportHeight / m_LogicalHeight);

    if (scale <= 0.0f) return false;

    const float offsetX = (viewportWidth - m_LogicalWidth * scale) * 0.5f;
    const float offsetY = (viewportHeight - m_LogicalHeight * scale) * 0.5f;

    result.x = (mouseX - offsetX) / scale;
    result.y = (mouseY - offsetY) / scale;

    return mouseX >= offsetX &&
           mouseY >= offsetY &&
           mouseX <= offsetX + m_LogicalWidth * scale &&
           mouseY <= offsetY + m_LogicalHeight * scale;
}

void UIRenderer::UpdateInput(
    UICanvas& canvas,
    const Input& input,
    float viewportX,
    float viewportY,
    float viewportWidth,
    float viewportHeight)
{
    UIWidget* root = canvas.GetRoot();
    if (!root) return;

    for (const auto& child : root->GetChildren())
        if (child) ResetButtonInput(*child);

    if (!m_MouseInteractionEnabled)
    {
        m_PressedCanvasButton = nullptr;
        if (m_FocusedTextInput)
            m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = nullptr;
        return;
    }

    // Loading/clearing a UI replaces the widget tree. Never keep a raw focus
    // pointer into an old canvas: validate it against the current tree first.
    if (m_FocusedTextInput)
    {
        UIWidget* current = root->Find(m_FocusedTextInput->GetName());
        if (current != m_FocusedTextInput)
            m_FocusedTextInput = nullptr;
    }

    Vec2 mouse;
    const float localX = input.GetMouseX() - viewportX;
    const float localY = input.GetMouseY() - viewportY;
    const bool inside = ViewportToCanvas(localX, localY, viewportWidth, viewportHeight, mouse);
    const UIRect canvasRect{0.0f, 0.0f, m_LogicalWidth, m_LogicalHeight};

    UIButton* hovered = nullptr;
    UITextInput* hoveredInput = nullptr;
    UISlider* hoveredSlider = nullptr;
    if (inside)
    {
        // Children are z-sorted ascending, so later hits replace earlier ones
        // and the visually topmost button owns the interaction.
        for (const auto& child : root->GetChildren())
            if (child)
            {
                if (UIButton* hit = FindTopButton(*child, canvasRect, mouse))
                    hovered = hit;
                if (UITextInput* hit = FindTopTextInput(*child, canvasRect, mouse))
                    hoveredInput = hit;
                if (UISlider* hit = FindTopSlider(*child, canvasRect, mouse)) hoveredSlider = hit;
            }
    }

    if (hovered) hovered->SetHovered(true);

    if (inside && input.IsMouseButtonPressed(SDL_BUTTON_LEFT))
    {
        m_PressedCanvasButton = hovered;
        m_DraggedSlider = hoveredSlider;
        if (m_FocusedTextInput && m_FocusedTextInput != hoveredInput)
            m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = hoveredInput;
        if (m_FocusedTextInput)
            m_FocusedTextInput->SetFocused(true);
    }

    if (m_FocusedTextInput)
    {
        // Match normal text-editor behavior: delete once immediately, then
        // repeat after a short hold delay at a steady rate.
        constexpr float repeatDelay = 1.f;
        constexpr float repeatInterval = 1.f;
        const float frameSeconds = 1.0f / 60.0f;

        auto repeatKey = [&](SDL_Scancode key, float& heldTime, float& repeatTime, auto action)
        {
            if (!input.IsKeyDown(key))
            {
                heldTime = 0.0f;
                repeatTime = 0.0f;
                return;
            }

            if (input.IsKeyPressed(key))
            {
                action();
                heldTime = 0.0f;
                repeatTime = 0.0f;
                return;
            }

            heldTime += frameSeconds;
            if (heldTime < repeatDelay)
                return;

            repeatTime += frameSeconds;
            while (repeatTime >= repeatInterval)
            {
                action();
                repeatTime -= repeatInterval;
            }
        };

        const bool ctrl = input.IsKeyDown(SDL_SCANCODE_LCTRL) || input.IsKeyDown(SDL_SCANCODE_RCTRL);
        const bool shift = input.IsKeyDown(SDL_SCANCODE_LSHIFT) || input.IsKeyDown(SDL_SCANCODE_RSHIFT);

        if (ctrl && input.IsKeyPressed(SDL_SCANCODE_A)) m_FocusedTextInput->SelectAll();
        else if (ctrl && input.IsKeyPressed(SDL_SCANCODE_C) && m_FocusedTextInput->HasSelection())
            SDL_SetClipboardText(m_FocusedTextInput->GetText().c_str());
        else if (ctrl && input.IsKeyPressed(SDL_SCANCODE_X) && m_FocusedTextInput->HasSelection())
        {
            SDL_SetClipboardText(m_FocusedTextInput->GetText().c_str());
            m_FocusedTextInput->SetText("");
        }
        else if (ctrl && input.IsKeyPressed(SDL_SCANCODE_V))
        {
            char* clipboard = SDL_GetClipboardText();
            if (clipboard) { m_FocusedTextInput->Insert(clipboard); SDL_free(clipboard); }
        }
        else
        {
            repeatKey(SDL_SCANCODE_BACKSPACE, m_BackspaceHeldTime, m_BackspaceRepeatTime,
                [&]() { m_FocusedTextInput->Backspace(); });
            repeatKey(SDL_SCANCODE_DELETE, m_DeleteHeldTime, m_DeleteRepeatTime,
                [&]() { m_FocusedTextInput->DeleteForward(); });
            if (input.IsKeyPressed(SDL_SCANCODE_LEFT)) m_FocusedTextInput->MoveCursorLeft();
            if (input.IsKeyPressed(SDL_SCANCODE_RIGHT)) m_FocusedTextInput->MoveCursorRight();
            if (input.IsKeyPressed(SDL_SCANCODE_HOME)) m_FocusedTextInput->SetCursor(0);
            if (input.IsKeyPressed(SDL_SCANCODE_END)) m_FocusedTextInput->SetCursor(m_FocusedTextInput->GetText().size());

            const bool caps = (SDL_GetModState() & SDL_KMOD_CAPS) != 0;
            for (int i = 0; i < 26; ++i)
                if (input.IsKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_A + i)))
                {
                    char ch = static_cast<char>('a' + i);
                    if (shift != caps) ch = static_cast<char>('A' + i);
                    m_FocusedTextInput->Insert(std::string(1, ch));
                }
            // SDL digit scancodes are not laid out numerically: 1..9 are
            // contiguous and 0 comes after 9.
            for (int i = 1; i <= 9; ++i)
            {
                const SDL_Scancode key = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (i - 1));
                if (input.IsKeyPressed(key))
                    m_FocusedTextInput->Insert(std::string(1, static_cast<char>('0' + i)));
            }
            if (input.IsKeyPressed(SDL_SCANCODE_0))
                m_FocusedTextInput->Insert("0");
            if (input.IsKeyPressed(SDL_SCANCODE_SPACE)) m_FocusedTextInput->Insert(" ");
            if (input.IsKeyPressed(SDL_SCANCODE_PERIOD)) m_FocusedTextInput->Insert(".");
            if (input.IsKeyPressed(SDL_SCANCODE_MINUS)) m_FocusedTextInput->Insert(shift ? "_" : "-");
            if (input.IsKeyPressed(SDL_SCANCODE_SLASH)) m_FocusedTextInput->Insert("/");
        }

        if (input.IsKeyPressed(SDL_SCANCODE_RETURN) || input.IsKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            m_BackspaceHeldTime = m_DeleteHeldTime = 0.0f;
            m_BackspaceRepeatTime = m_DeleteRepeatTime = 0.0f;
            m_FocusedTextInput->SetFocused(false);
            m_FocusedTextInput = nullptr;
        }
    }

    if (m_PressedCanvasButton)
        m_PressedCanvasButton->SetPressed(true);

    if (m_DraggedSlider && inside && input.IsMouseButtonDown(SDL_BUTTON_LEFT))
    {
        const UIRect sliderRect=UILayout::Calculate(*m_DraggedSlider, m_DraggedSlider->GetParent() && m_DraggedSlider->GetParent()!=root ? UILayout::Calculate(*m_DraggedSlider->GetParent(),canvasRect) : canvasRect);
        if(sliderRect.width>0.0f) m_DraggedSlider->SetValue((mouse.x-sliderRect.x)/sliderRect.width);
    }

    if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT))
    {
        if (m_PressedCanvasButton && m_PressedCanvasButton == hovered)
        {
            m_PressedCanvasButton->SetClicked(true);
            if (m_Audio) m_Audio->PlayUISound();
        }
        if (m_PressedCanvasButton)
            m_PressedCanvasButton->SetPressed(false);
        m_PressedCanvasButton = nullptr;
        m_DraggedSlider = nullptr;
    }
}

void UIRenderer::ResetButtonInput(UIWidget& widget)
{
    if (UIButton* button = dynamic_cast<UIButton*>(&widget))
    {
        button->SetHovered(false);
        if (button != m_PressedCanvasButton)
            button->SetPressed(false);
    }
    for (const auto& child : widget.GetChildren())
        if (child) ResetButtonInput(*child);
}

UIButton* UIRenderer::FindTopButton(
    UIWidget& widget,
    const UIRect& parentRect,
    const Vec2& mouse)
{
    if (!widget.IsVisible() || !widget.IsEnabled())
        return nullptr;

    const UIRect rect = UILayout::Calculate(widget, parentRect);
    UIButton* result = nullptr;

    for (const auto& child : widget.GetChildren())
        if (child)
            if (UIButton* hit = FindTopButton(*child, rect, mouse))
                result = hit;

    const bool hit = widget.IsHitTestVisible() &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
        mouse.y >= rect.y && mouse.y <= rect.y + rect.height;

    if (hit)
        if (UIButton* button = dynamic_cast<UIButton*>(&widget))
            result = button;

    return result;
}

UISlider* UIRenderer::FindTopSlider(UIWidget& widget,const UIRect& parentRect,const Vec2& mouse)
{
    if(!widget.IsVisible()||!widget.IsEnabled())return nullptr;
    const UIRect rect=UILayout::Calculate(widget,parentRect); UISlider* result=nullptr;
    for(const auto& child:widget.GetChildren())if(child)if(UISlider* hit=FindTopSlider(*child,rect,mouse))result=hit;
    const bool hit=widget.IsHitTestVisible()&&mouse.x>=rect.x&&mouse.x<=rect.x+rect.width&&mouse.y>=rect.y&&mouse.y<=rect.y+rect.height;
    if(hit)if(UISlider* slider=dynamic_cast<UISlider*>(&widget))result=slider; return result;
}

UITextInput* UIRenderer::FindTopTextInput(
    UIWidget& widget, const UIRect& parentRect, const Vec2& mouse)
{
    if (!widget.IsVisible() || !widget.IsEnabled()) return nullptr;
    const UIRect rect = UILayout::Calculate(widget, parentRect);
    UITextInput* result = nullptr;
    for (const auto& child : widget.GetChildren())
        if (child)
            if (UITextInput* hit = FindTopTextInput(*child, rect, mouse))
                result = hit;
    const bool hit = widget.IsHitTestVisible() &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
        mouse.y >= rect.y && mouse.y <= rect.y + rect.height;
    if (hit)
        if (UITextInput* input = dynamic_cast<UITextInput*>(&widget))
            result = input;
    return result;
}

// =============================================================
// Canvas UI
// =============================================================

void UIRenderer::RenderCanvas(
    UICanvas& canvas,
    Renderer* renderer)
{
    const UIWidget* root =
        canvas.GetRoot();

    if (root == nullptr ||
        !root->IsVisible())
    {
        return;
    }

    /*
     * The canvas uses logical coordinates.
     *
     * The shader converts these coordinates
     * to the actual framebuffer.
     */
    const UIRect canvasRect{
        0.0f,
        0.0f,
        m_LogicalWidth,
        m_LogicalHeight
    };

    for (const auto& child :
        root->GetChildren())
    {
        if (child)
        {
            RenderCanvasWidget(
                *child,
                canvasRect,
                renderer
            );
        }
    }
}

void UIRenderer::RenderCanvasWidget(
    const UIWidget& widget,
    const UIRect& parentRect,
    Renderer* renderer)
{
    if (!widget.IsVisible())
    {
        return;
    }

    const UIRect rect =
        UILayout::Calculate(
            widget,
            parentRect
        );

    if (widget.GetType() == UIWidgetType::Slider)
    {
        if(const UISlider* slider=dynamic_cast<const UISlider*>(&widget)) DrawSlider(*slider,rect);
    }
    else if (widget.GetType() == UIWidgetType::TextInput)
    {
        if (const UITextInput* input = dynamic_cast<const UITextInput*>(&widget))
            DrawTextInput(*input, rect);
    }
    else if (widget.GetType() ==
        UIWidgetType::Text)
    {
        const UIText* text =
            dynamic_cast<const UIText*>(
                &widget
                );

        if (text)
        {
            DrawCanvasText(
                *text,
                rect
            );
        }
    }
    else
    {
        DrawCanvasWidget(
            widget,
            rect,
            renderer
        );
    }

    for (const auto& child :
        widget.GetChildren())
    {
        if (child)
        {
            RenderCanvasWidget(
                *child,
                rect,
                renderer
            );
        }
    }
}

void UIRenderer::DrawCanvasWidget(
    const UIWidget& widget,
    const UIRect& rect,
    Renderer* renderer)
{
    const float vertices[24] =
    {
        rect.x,
        rect.y,
        0.0f,
        1.0f,

        rect.x + rect.width,
        rect.y,
        1.0f,
        1.0f,

        rect.x + rect.width,
        rect.y + rect.height,
        1.0f,
        0.0f,

        rect.x,
        rect.y,
        0.0f,
        1.0f,

        rect.x + rect.width,
        rect.y + rect.height,
        1.0f,
        0.0f,

        rect.x,
        rect.y + rect.height,
        0.0f,
        0.0f
    };

    glBindVertexArray(m_VAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VBO
    );

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );

    Vec4 color = widget.GetColor();

    if (const UIButton* button = dynamic_cast<const UIButton*>(&widget))
        color = button->GetCurrentColor();

    m_Shader.SetVec4(
        "u_Color",
        color.x,
        color.y,
        color.z,
        color.w
    );

    const Vec4 gradientColor = widget.GetGradientColor();
    m_Shader.SetVec4("u_GradientColor", gradientColor.x, gradientColor.y, gradientColor.z, gradientColor.w);
    m_Shader.SetInt("u_UseGradient", widget.HasGradient() ? 1 : 0);
    m_Shader.SetInt("u_GradientDirection", widget.GetGradientDirection() == UIGradientDirection::Horizontal ? 1 : 0);

    Texture2D* texture = nullptr;

    if (renderer != nullptr)
    {
        if (const UIImage* image = dynamic_cast<const UIImage*>(&widget))
        {
            if (!image->GetTexturePath().empty())
                texture = renderer->LoadTexture(image->GetTexturePath());
        }
    }

    if (texture != nullptr)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture->GetID());
        m_Shader.SetInt("u_Texture", 0);
        m_Shader.SetInt("u_UseTexture", 1);
    }
    else
    {
        m_Shader.SetInt("u_UseTexture", 0);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (texture != nullptr)
        glBindTexture(GL_TEXTURE_2D, 0);

    glBindVertexArray(0);
}

bool UIRenderer::InitializeFontAtlas()
{
    std::ifstream file(
        "Assets/Fonts/InterVariable.ttf",
        std::ios::binary | std::ios::ate
    );
    if (!file) return false;

    const std::streamsize length = file.tellg();
    if (length <= 0) return false;
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> fontData(static_cast<size_t>(length));
    if (!file.read(reinterpret_cast<char*>(fontData.data()), length))
        return false;

    std::vector<unsigned char> bitmap(
        FontAtlasWidth * FontAtlasHeight, 0);

    stbtt_bakedchar baked[95]{};
    const int result = stbtt_BakeFontBitmap(
        fontData.data(), 0, FontBakeSize,
        bitmap.data(), FontAtlasWidth, FontAtlasHeight,
        32, 95, baked);
    if (result <= 0) return false;

    std::vector<unsigned char> rgba(
        FontAtlasWidth * FontAtlasHeight * 4, 255);
    for (int i = 0; i < FontAtlasWidth * FontAtlasHeight; ++i)
        rgba[i * 4 + 3] = bitmap[i];

    glGenTextures(1, &m_FontTexture);
    glBindTexture(GL_TEXTURE_2D, m_FontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        FontAtlasWidth, FontAtlasHeight, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);

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

void UIRenderer::DrawSlider(const UISlider& slider,const UIRect& rect)
{
    DrawCanvasWidget(slider,rect,nullptr);
    UIWidget fill(UIWidgetType::Panel); fill.SetColor(slider.GetFillColor());
    DrawCanvasWidget(fill,UIRect{rect.x,rect.y,rect.width*slider.GetValue(),rect.height},nullptr);
    UIWidget handle(UIWidgetType::Panel); handle.SetColor(slider.GetHandleColor());
    const float w=10.0f; DrawCanvasWidget(handle,UIRect{rect.x+rect.width*slider.GetValue()-w*0.5f,rect.y-3.0f,w,rect.height+6.0f},nullptr);
}

void UIRenderer::DrawTextInput(const UITextInput& input, const UIRect& rect)
{
    // Background is drawn by the normal widget path before this text overlay.
    DrawCanvasWidget(input, rect, nullptr);
    UIText text;
    text.SetPosition(Vec2(rect.x + 12.0f, rect.y + 8.0f));
    text.SetSize(Vec2(std::max(0.0f, rect.width - 24.0f), rect.height - 16.0f));
    text.SetFontSize(input.GetFontSize());
    text.SetColor(input.GetText().empty() ? Vec4(0.45f,0.48f,0.52f,1.0f) : Vec4(0.92f,0.94f,0.97f,1.0f));
    std::string display = input.GetDisplayText();
    if (input.IsFocused()) display += "|";
    text.SetText(display);
    DrawCanvasText(text, UIRect{rect.x + 12.0f, rect.y + 8.0f, std::max(0.0f, rect.width - 24.0f), rect.height - 16.0f});
}

void UIRenderer::DrawCanvasText(
    const UIText& text,
    const UIRect& rect)
{
    if (m_FontTexture == 0 || text.GetText().empty()) return;

    const float requestedSize = std::max(1.0f, text.GetFontSize());
    const float scale = requestedSize / FontBakeSize;
    Vec4 color = text.GetColor();
    for (const UIWidget* parent = text.GetParent(); parent; parent = parent->GetParent())
    {
        const UIButton* button = dynamic_cast<const UIButton*>(parent);
        if (button && button->GetAffectChildText())
        {
            color = button->GetCurrentTextColor();
            break;
        }
    }

    float penX = rect.x;
    float penY = rect.y + requestedSize;

    for (unsigned char character : text.GetText())
    {
        if (character == '\n')
        {
            penX = rect.x;
            penY += requestedSize * 1.2f;
            continue;
        }

        if (character < 32 || character > 126)
            character = '?';

        const FontGlyph& glyph = m_FontGlyphs[character - 32];
        const float width = (glyph.x1 - glyph.x0) * scale;
        const float height = (glyph.y1 - glyph.y0) * scale;

        if (width > 0.0f && height > 0.0f)
        {
            DrawFontGlyph(
                penX + glyph.xoff * scale,
                penY + glyph.yoff * scale,
                width, height,
                glyph.x0 / FontAtlasWidth,
                glyph.y0 / FontAtlasHeight,
                glyph.x1 / FontAtlasWidth,
                glyph.y1 / FontAtlasHeight,
                color);
        }

        penX += glyph.xadvance * scale;
    }
}

void UIRenderer::DrawFontGlyph(
    float x, float y, float width, float height,
    float u0, float v0, float u1, float v1,
    const Vec4& color)
{
    const float vertices[24] = {
        x, y, u0, v0,
        x + width, y, u1, v0,
        x + width, y + height, u1, v1,
        x, y, u0, v0,
        x + width, y + height, u1, v1,
        x, y + height, u0, v1
    };

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    m_Shader.SetVec4(
        "u_Color", color.x, color.y, color.z, color.w);
    m_Shader.SetInt("u_UseGradient", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_FontTexture);
    m_Shader.SetInt("u_Texture", 0);
    m_Shader.SetInt("u_UseTexture", 1);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

// =============================================================
// Runtime / scripted UI
// =============================================================

void UIRenderer::DrawRect(
    float x,
    float y,
    float width,
    float height,
    float red,
    float green,
    float blue,
    float alpha)
{
    const std::string id =
        "__rect_" +
        std::to_string(
            m_Elements.size()
        );

    Element element;

    element.x = x;
    element.y = y;

    element.width = width;
    element.height = height;

    element.red = red;
    element.green = green;
    element.blue = blue;
    element.alpha = alpha;

    element.texture = nullptr;
    element.visible = true;

    m_Elements[id] = element;
}

void UIRenderer::SetRect(
    const std::string& id,
    float x,
    float y,
    float width,
    float height,
    float red,
    float green,
    float blue,
    float alpha)
{
    Element& element =
        m_Elements[id];

    element.x = x;
    element.y = y;

    element.width = width;
    element.height = height;

    element.red = red;
    element.green = green;
    element.blue = blue;
    element.alpha = alpha;

    element.texture = nullptr;
    element.visible = true;
}

void UIRenderer::SetText(
    const std::string& id,
    const std::string& text,
    float x,
    float y,
    float scale,
    float red,
    float green,
    float blue,
    float alpha)
{
    TextElement& element =
        m_TextElements[id];

    element.text = text;

    element.x = x;
    element.y = y;

    element.scale = scale;

    element.red = red;
    element.green = green;
    element.blue = blue;
    element.alpha = alpha;

    element.visible = true;
}

void UIRenderer::SetImage(
    const std::string& id,
    Texture2D* texture,
    float x,
    float y,
    float width,
    float height,
    float red,
    float green,
    float blue,
    float alpha)
{
    Element& element =
        m_Elements[id];

    element.x = x;
    element.y = y;

    element.width = width;
    element.height = height;

    element.red = red;
    element.green = green;
    element.blue = blue;
    element.alpha = alpha;

    element.texture = texture;
    element.visible = true;
}

void UIRenderer::SetVisible(
    const std::string& id,
    bool visible)
{
    auto element =
        m_Elements.find(id);

    if (element != m_Elements.end())
    {
        element->second.visible =
            visible;

        return;
    }

    auto text =
        m_TextElements.find(id);

    if (text != m_TextElements.end())
    {
        text->second.visible =
            visible;
    }
}

void UIRenderer::SetParent(
    const std::string& id,
    const std::string& parentId)
{
    auto element =
        m_Elements.find(id);

    if (element != m_Elements.end())
    {
        element->second.parent =
            parentId;

        return;
    }

    auto text =
        m_TextElements.find(id);

    if (text != m_TextElements.end())
    {
        text->second.parent =
            parentId;
    }
}

void UIRenderer::Remove(
    const std::string& id)
{
    m_Elements.erase(id);
    m_TextElements.erase(id);
}

void UIRenderer::Clear()
{
    m_Elements.clear();
    m_TextElements.clear();
    m_PressedCanvasButton = nullptr;
    m_BackspaceHeldTime = m_DeleteHeldTime = 0.0f;
    m_BackspaceRepeatTime = m_DeleteRepeatTime = 0.0f;
    if (m_FocusedTextInput)
        m_FocusedTextInput->SetFocused(false);
    m_FocusedTextInput = nullptr;
}

void UIRenderer::SetMouseInteractionEnabled(
    bool enabled)
{
    m_MouseInteractionEnabled =
        enabled;
}

bool UIRenderer::IsMouseInteractionEnabled() const
{
    return m_MouseInteractionEnabled;
}

bool UIRenderer::GetAbsolutePosition(
    const std::string& id,
    float& x,
    float& y,
    std::unordered_set<std::string>& resolving
) const
{
    if (resolving.contains(id))
    {
        return false;
    }

    resolving.insert(id);

    auto element =
        m_Elements.find(id);

    if (element != m_Elements.end())
    {
        x = element->second.x;
        y = element->second.y;

        if (!element->second.parent.empty())
        {
            float parentX = 0.0f;
            float parentY = 0.0f;

            if (GetAbsolutePosition(
                element->second.parent,
                parentX,
                parentY,
                resolving))
            {
                x += parentX;
                y += parentY;
            }
        }

        resolving.erase(id);

        return true;
    }

    auto text =
        m_TextElements.find(id);

    if (text != m_TextElements.end())
    {
        x = text->second.x;
        y = text->second.y;

        if (!text->second.parent.empty())
        {
            float parentX = 0.0f;
            float parentY = 0.0f;

            if (GetAbsolutePosition(
                text->second.parent,
                parentX,
                parentY,
                resolving))
            {
                x += parentX;
                y += parentY;
            }
        }

        resolving.erase(id);

        return true;
    }

    resolving.erase(id);

    return false;
}

void UIRenderer::DrawElement(
    const std::string& id,
    const Element& element)
{
    if (!element.visible)
    {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;

    std::unordered_set<std::string>
        resolving;

    if (!GetAbsolutePosition(
        id,
        x,
        y,
        resolving))
    {
        return;
    }

    const float vertices[24] =
    {
        x,
        y,
        0.0f,
        0.0f,

        x + element.width,
        y,
        1.0f,
        0.0f,

        x + element.width,
        y + element.height,
        1.0f,
        1.0f,

        x,
        y,
        0.0f,
        0.0f,

        x + element.width,
        y + element.height,
        1.0f,
        1.0f,

        x,
        y + element.height,
        0.0f,
        1.0f
    };

    glBindVertexArray(m_VAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VBO
    );

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );

    m_Shader.SetVec4(
        "u_Color",
        element.red,
        element.green,
        element.blue,
        element.alpha
    );

    if (element.texture != nullptr)
    {
        glActiveTexture(GL_TEXTURE0);

        glBindTexture(
            GL_TEXTURE_2D,
            element.texture->GetID()
        );

        m_Shader.SetInt(
            "u_Texture",
            0
        );

        m_Shader.SetInt(
            "u_UseTexture",
            1
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            6
        );

        glBindTexture(
            GL_TEXTURE_2D,
            0
        );
    }
    else
    {
        m_Shader.SetInt(
            "u_UseTexture",
            0
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            6
        );
    }

    glBindVertexArray(0);
}

void UIRenderer::DrawTextElement(
    const std::string& id,
    const TextElement& element)
{
    if (!element.visible ||
        element.text.empty())
    {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;

    std::unordered_set<std::string>
        resolving;

    if (!GetAbsolutePosition(
        id,
        x,
        y,
        resolving))
    {
        return;
    }

    float cursorX = x;
    float cursorY = y;

    for (char character :
    element.text)
    {
        if (character == '\n')
        {
            cursorX = x;
            cursorY +=
                element.scale * 8.0f;

            continue;
        }

        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                const unsigned char pixel =
                    GetGlyphRow(
                        character,
                        row
                    );

                if ((pixel &
                    (1 << (4 - column))) == 0)
                {
                    continue;
                }

                DrawTextPixel(
                    cursorX +
                    column * element.scale,
                    cursorY +
                    row * element.scale,
                    element.scale,
                    element.red,
                    element.green,
                    element.blue,
                    element.alpha
                );
            }
        }

        cursorX +=
            element.scale * 6.0f;
    }
}

void UIRenderer::DrawTextPixel(
    float x,
    float y,
    float size,
    float red,
    float green,
    float blue,
    float alpha)
{
    const float vertices[24] =
    {
        x,
        y,
        0.0f,
        0.0f,

        x + size,
        y,
        1.0f,
        0.0f,

        x + size,
        y + size,
        1.0f,
        1.0f,

        x,
        y,
        0.0f,
        0.0f,

        x + size,
        y + size,
        1.0f,
        1.0f,

        x,
        y + size,
        0.0f,
        1.0f
    };

    glBindVertexArray(m_VAO);

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_VBO
    );

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        sizeof(vertices),
        vertices
    );

    m_Shader.SetVec4(
        "u_Color",
        red,
        green,
        blue,
        alpha
    );

    m_Shader.SetInt(
        "u_UseTexture",
        0
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );
}

unsigned char UIRenderer::GetGlyphRow(
    char character,
    int row) const
{
    static const unsigned char digits[10][7] =
    {
        {14, 17, 19, 21, 25, 17, 14},
        {4, 12, 4, 4, 4, 4, 14},
        {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},
        {2, 6, 10, 18, 31, 2, 2},
        {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14},
        {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 1, 14}
    };

    static const unsigned char letters[26][7] =
    {
        {14, 17, 17, 31, 17, 17, 17},
        {30, 17, 17, 30, 17, 17, 30},
        {14, 17, 16, 16, 16, 17, 14},
        {30, 17, 17, 17, 17, 17, 30},
        {31, 16, 16, 30, 16, 16, 31},
        {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 14},
        {17, 17, 17, 31, 17, 17, 17},
        {14, 4, 4, 4, 4, 4, 14},
        {7, 2, 2, 2, 18, 18, 12},
        {17, 18, 20, 24, 20, 18, 17},
        {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17},
        {17, 25, 25, 21, 19, 19, 17},
        {14, 17, 17, 17, 17, 17, 14},
        {30, 17, 17, 30, 16, 16, 16},
        {14, 17, 17, 17, 21, 18, 13},
        {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30},
        {31, 4, 4, 4, 4, 4, 4},
        {17, 17, 17, 17, 17, 17, 14},
        {17, 17, 17, 17, 17, 10, 4},
        {17, 17, 17, 21, 21, 27, 17},
        {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},
        {31, 1, 2, 4, 8, 16, 31}
    };

    if (row < 0 || row >= 7)
    {
        return 0;
    }

    if (character >= '0' &&
        character <= '9')
    {
        return digits[
            character - '0'
        ][row];
    }

    if (character >= 'A' &&
        character <= 'Z')
    {
        return letters[
            character - 'A'
        ][row];
    }

    if (character >= 'a' &&
        character <= 'z')
    {
        return letters[
            character - 'a'
        ][row];
    }

    if (character == ' ')
    {
        return 0;
    }

    return 0;
}