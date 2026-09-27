#include "../UIRenderer.h"
#include "../UIText.h"
#include "../UIImage.h"
#include "../UIButton.h"
#include "../UITextInput.h"
#include "../UISlider.h"
#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../Graphics/Texture2D.h"
#include "../../Audio/AudioEngine.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <string>

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
