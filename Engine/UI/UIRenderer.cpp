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
#include "../Core/Logger.h"
#include "../Audio/AudioEngine.h"
#include "../Editor/Fonts/InterFont.h"

#include <glad/gl.h>

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







// =============================================================
// Canvas UI
// =============================================================




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




// =============================================================
// Runtime / scripted UI
// =============================================================















