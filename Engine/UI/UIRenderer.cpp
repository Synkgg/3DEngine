#include "UIRenderer.h"

#include "UIText.h"
#include "UIImage.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture2D.h"

#include <glad/gl.h>

#include <algorithm>
#include <string>

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
        uniform sampler2D u_Texture;
        uniform int u_UseTexture;

        out vec4 FragColor;

        void main()
        {
            if (u_UseTexture != 0)
            {
                FragColor =
                    texture(u_Texture, v_UV) *
                    u_Color;
            }
            else
            {
                FragColor = u_Color;
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

    return true;
}

void UIRenderer::Shutdown()
{
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

// =============================================================
// Canvas UI
// =============================================================

void UIRenderer::RenderCanvas(
    const UICanvas& canvas,
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

    if (widget.GetType() ==
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
        0.0f,

        rect.x + rect.width,
        rect.y,
        1.0f,
        0.0f,

        rect.x + rect.width,
        rect.y + rect.height,
        1.0f,
        1.0f,

        rect.x,
        rect.y,
        0.0f,
        0.0f,

        rect.x + rect.width,
        rect.y + rect.height,
        1.0f,
        1.0f,

        rect.x,
        rect.y + rect.height,
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

    const Vec4& color =
        widget.GetColor();

    m_Shader.SetVec4(
        "u_Color",
        color.x,
        color.y,
        color.z,
        color.w
    );

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

void UIRenderer::DrawCanvasText(
    const UIText& text,
    const UIRect& rect)
{
    const std::string& value =
        text.GetText();

    if (value.empty())
    {
        return;
    }

    const float size =
        text.GetFontSize();

    const Vec4& color =
        text.GetColor();

    float x = rect.x;
    float y = rect.y;

    for (char character : value)
    {
        if (character == '\n')
        {
            y += size * 8.0f;
            x = rect.x;
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

                DrawCanvasTextPixel(
                    x + column * size,
                    y + row * size,
                    size,
                    color.x,
                    color.y,
                    color.z,
                    color.w
                );
            }
        }

        x += size * 6.0f;
    }
}

void UIRenderer::DrawCanvasTextPixel(
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