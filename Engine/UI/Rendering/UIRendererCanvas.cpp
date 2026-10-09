#include "../UIRenderer.h"
#include "../UIText.h"
#include "../UIImage.h"
#include "../UIButton.h"
#include "../UITextInput.h"
#include "../UISlider.h"
#include "../UIProgressBar.h"
#include "../UIScrollBox.h"
#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../Audio/AudioEngine.h"
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

    // A full-canvas opaque background also covers letterbox margins. Keep
    // controls in logical coordinates, but don't expose the 3D sky around menus.
    if (!root->GetChildren().empty() && m_UIScale > 0) {
        const auto& background = root->GetChildren().front();
        if (background && background->GetType() == UIWidgetType::Panel && background->IsVisible() &&
            background->GetPosition().x == 0 && background->GetPosition().y == 0 &&
            background->GetSize().x == m_LogicalWidth && background->GetSize().y == m_LogicalHeight &&
            background->GetColor().w >= 1 && background->GetRenderOpacity() >= 1 && !background->HasGradient()) {
            const auto clip = m_ClipRect;
            m_ClipRect = {-m_UIOffsetX/m_UIScale,-m_UIOffsetY/m_UIScale,float(m_Width)/m_UIScale,float(m_Height)/m_UIScale};
            DrawQuad(m_ClipRect.x,m_ClipRect.y,m_ClipRect.width,m_ClipRect.height,0,0,1,1,background->GetColor(),m_WhiteTexture);
            m_ClipRect = clip;
        }
    }

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

namespace
{
    float InheritedOpacity(const UIWidget& widget)
    {
        float opacity = 1.0f;
        for (const UIWidget* current = &widget; current; current = current->GetParent())
            opacity *= current->GetRenderOpacity();
        return std::clamp(opacity, 0.0f, 1.0f);
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

    if (widget.GetType() == UIWidgetType::ProgressBar)
    {
        if(const UIProgressBar* progress=dynamic_cast<const UIProgressBar*>(&widget)) DrawProgressBar(*progress,rect);
    }
    else if (widget.GetType() == UIWidgetType::Slider)
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

    UIRect childRect = rect;
    const UIRect previousClip = m_ClipRect;
    if (const auto* scroll = dynamic_cast<const UIScrollBox*>(&widget))
    {
        const float left = std::max(previousClip.x, rect.x);
        const float top = std::max(previousClip.y, rect.y);
        const float right = std::min(previousClip.x + previousClip.width, rect.x + rect.width);
        const float bottom = std::min(previousClip.y + previousClip.height, rect.y + rect.height);
        m_ClipRect = {left, top, std::max(0.0f, right-left), std::max(0.0f, bottom-top)};
        childRect.y -= scroll->GetScrollOffset();
    }
    if (m_ClipRect.width > 0.0f && m_ClipRect.height > 0.0f)
    {
        for (const auto& child : widget.GetChildren())
            if (child) RenderCanvasWidget(*child, childRect, renderer);
    }
    m_ClipRect = previousClip;
}

void UIRenderer::DrawCanvasWidget(const UIWidget& widget, const UIRect& rect, Renderer* renderer)
{
    // A user widget is a transparent container; its linked children draw below.
    if (widget.GetType() == UIWidgetType::UserWidget) return;
    Vec4 color = widget.GetColor();
    std::string texturePath;
    if (const auto* button = dynamic_cast<const UIButton*>(&widget))
    {
        color = button->GetCurrentColor();
        texturePath = button->GetCurrentImage();
    }
    else if (const auto* image = dynamic_cast<const UIImage*>(&widget))
        texturePath = image->GetTexturePath();

    color.w *= InheritedOpacity(widget);
    Velcryn::RHI::TextureHandle texture = m_WhiteTexture;
    if (renderer && !texturePath.empty())
        if (Texture2D* loaded = renderer->LoadTexture(texturePath); loaded && loaded->IsLoaded())
            texture = loaded->GetHandle();

    Vec4 gradient = widget.GetGradientColor(); gradient.w *= InheritedOpacity(widget);
    DrawQuad(rect.x, rect.y, rect.width, rect.height, 0.0f, 1.0f, 1.0f, 0.0f,
        color, texture, gradient, widget.HasGradient(),
        widget.GetGradientDirection() == UIGradientDirection::Horizontal, widget.GetCornerRadius());
}

void UIRenderer::DrawSlider(const UISlider& slider,const UIRect& rect)
{
    DrawCanvasWidget(slider,rect,nullptr);
    const float opacity=InheritedOpacity(slider);
    UIWidget fill(UIWidgetType::Panel); fill.SetColor(slider.GetFillColor()); fill.SetRenderOpacity(opacity);
    DrawCanvasWidget(fill,UIRect{rect.x,rect.y,rect.width*slider.GetValue(),rect.height},nullptr);
    UIWidget handle(UIWidgetType::Panel); handle.SetColor(slider.GetHandleColor()); handle.SetRenderOpacity(opacity);
    const float w=10.0f; DrawCanvasWidget(handle,UIRect{rect.x+rect.width*slider.GetValue()-w*0.5f,rect.y-3.0f,w,rect.height+6.0f},nullptr);
}

void UIRenderer::DrawProgressBar(const UIProgressBar& progress,const UIRect& rect)
{
    DrawCanvasWidget(progress,rect,nullptr);
    const float value=std::clamp(progress.GetPercent(),0.0f,1.0f);
    UIRect fillRect=rect;
    switch(progress.GetFillDirection())
    {
    case UIProgressBarFillDirection::LeftToRight: fillRect.width*=value; break;
    case UIProgressBarFillDirection::RightToLeft: fillRect.x+=fillRect.width*(1.0f-value); fillRect.width*=value; break;
    case UIProgressBarFillDirection::TopToBottom: fillRect.height*=value; break;
    case UIProgressBarFillDirection::BottomToTop: fillRect.y+=fillRect.height*(1.0f-value); fillRect.height*=value; break;
    }
    UIWidget fill(UIWidgetType::Panel); fill.SetColor(progress.GetFillColor()); fill.SetCornerRadius(progress.GetCornerRadius()); fill.SetRenderOpacity(InheritedOpacity(progress));
    DrawCanvasWidget(fill,fillRect,nullptr);
}

void UIRenderer::DrawTextInput(const UITextInput& input, const UIRect& rect)
{
    DrawCanvasWidget(input, rect, nullptr);
    const UIRect previousClip = m_ClipRect;
    const float left = std::max(previousClip.x, rect.x+12);
    const float top = std::max(previousClip.y, rect.y+4);
    m_ClipRect = {left, top,
        std::max(0.0f, std::min(previousClip.x+previousClip.width, rect.x+rect.width-12)-left),
        std::max(0.0f, std::min(previousClip.y+previousClip.height, rect.y+rect.height-4)-top)};
    const float opacity = InheritedOpacity(input);
    const float scale = input.GetFontSize()/FontBakeSize;
    const std::string display = input.GetDisplayText();
    auto measure = [&](std::size_t end) {
        float width = 0;
        for (std::size_t i=0; i<std::min(end, display.size()); ++i) {
            unsigned char ch = display[i]; if (ch < 32 || ch > 126) ch = '?';
            width += m_FontGlyphs[ch-32].xadvance*scale;
        }
        return width;
    };
    const float caret = input.GetText().empty() ? 0 : measure(input.GetCursor());
    const float scroll = input.IsFocused() ? std::max(0.0f, caret-m_ClipRect.width+2) : 0;
    const float x = rect.x+12-scroll;
    if (input.IsFocused() && input.HasSelection())
        DrawQuad(x, rect.y+8, measure(display.size()), input.GetFontSize()*1.2f, 0,0,1,1,
            Vec4(0.18f,0.38f,0.65f,0.7f*opacity), m_WhiteTexture);
    UIText text;
    text.SetFontSize(input.GetFontSize());
    text.SetColor(input.GetText().empty() ? Vec4(0.45f,0.48f,0.52f,1) : Vec4(0.92f,0.94f,0.97f,1));
    text.SetText(display); text.SetRenderOpacity(opacity);
    DrawCanvasText(text, {x, rect.y+8, std::max(0.0f, rect.width-24), rect.height-16});
    if (input.IsFocused())
        DrawQuad(x+caret, rect.y+8, 1.5f, input.GetFontSize()*1.2f, 0,0,1,1,
            Vec4(0.95f,0.97f,1,opacity), m_WhiteTexture);
    m_ClipRect = previousClip;

}

void UIRenderer::DrawCanvasText(
    const UIText& text,
    const UIRect& rect)
{
    if (!m_FontTexture || text.GetText().empty()) return;

    const float requestedSize = std::max(1.0f, text.GetFontSize());
    const float scale = requestedSize / FontBakeSize;
    const float lineHeight = requestedSize * 1.2f;
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
    color.w *= InheritedOpacity(text);

    std::vector<std::string> lines;
    std::string current;
    for (char ch : text.GetText())
    {
        if (ch == '\n') { lines.push_back(current); current.clear(); }
        else current += ch;
    }
    lines.push_back(current);

    const float blockHeight = requestedSize + (lines.size() > 1 ? (lines.size() - 1) * lineHeight : 0.0f);
    float top = rect.y;
    if (text.GetVerticalAlignment() == UITextVerticalAlignment::Center) top += (rect.height - blockHeight) * 0.5f;
    else if (text.GetVerticalAlignment() == UITextVerticalAlignment::Bottom) top += rect.height - blockHeight;

    for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        float lineWidth = 0.0f;
        for (unsigned char character : lines[lineIndex])
        {
            if (character < 32 || character > 126) character = '?';
            lineWidth += m_FontGlyphs[character - 32].xadvance * scale;
        }

        float penX = rect.x;
        if (text.GetHorizontalAlignment() == UITextHorizontalAlignment::Center) penX += (rect.width - lineWidth) * 0.5f;
        else if (text.GetHorizontalAlignment() == UITextHorizontalAlignment::Right) penX += rect.width - lineWidth;
        float penY = top + requestedSize + lineIndex * lineHeight;

        for (unsigned char character : lines[lineIndex])
        {
            if (character < 32 || character > 126) character = '?';
            const FontGlyph& glyph = m_FontGlyphs[character - 32];
            const float width = (glyph.x1 - glyph.x0) * scale;
            const float height = (glyph.y1 - glyph.y0) * scale;
            if (width > 0.0f && height > 0.0f)
                DrawFontGlyph(penX + glyph.xoff * scale, penY + glyph.yoff * scale,
                    width, height, glyph.x0 / FontAtlasWidth, glyph.y0 / FontAtlasHeight,
                    glyph.x1 / FontAtlasWidth, glyph.y1 / FontAtlasHeight, color);
            penX += glyph.xadvance * scale;
        }
    }
}

void UIRenderer::DrawFontGlyph(float x,float y,float width,float height,float u0,float v0,float u1,float v1,const Vec4& color)
{
    DrawQuad(x, y, width, height, u0, v0, u1, v1, color, m_FontTexture);
}
