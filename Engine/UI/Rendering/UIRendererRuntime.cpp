#include "UIRenderer.h"
#include "UIText.h"
#include "UIImage.h"
#include "UIButton.h"
#include "UITextInput.h"
#include "UISlider.h"
#include "../Platform/SDL/Input.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture2D.h"
#include "../Audio/AudioEngine.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <string>

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
