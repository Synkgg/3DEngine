#include "Shader.h"

#include <glad/gl.h>
#include <iostream>
#include "../Core/Logger.h"

static unsigned int CompileShader(unsigned int type, const char* source)
{
    unsigned int shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char infoLog[512];

        glGetShaderInfoLog(
            shader,
            sizeof(infoLog),
            nullptr,
            infoLog
        );

        Logger::Error( std::string("Shader compilation failed: ") + infoLog);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

Shader::Shader()
	: m_Program(0)
{
}

Shader::~Shader()
{
	Shutdown();
}

bool Shader::Initialize(const char* vertexSource, const char* fragmentSource)
{
    unsigned int vertexShader = CompileShader(
        GL_VERTEX_SHADER,
        vertexSource
    );

    if (vertexShader == 0)
    {
        return false;
    }

    unsigned int fragmentShader = CompileShader(
        GL_FRAGMENT_SHADER,
        fragmentSource
    );

    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return false;
    }

    m_Program = glCreateProgram();

    glAttachShader(m_Program, vertexShader);
    glAttachShader(m_Program, fragmentShader);

    glLinkProgram(m_Program);

    int success = 0;

    glGetProgramiv(
        m_Program,
        GL_LINK_STATUS,
        &success
    );

    if (!success)
    {
        char infoLog[512];

        glGetProgramInfoLog(
            m_Program,
            sizeof(infoLog),
            nullptr,
            infoLog
        );

        Logger::Error( std::string("Shader linking failed: ") + infoLog);

        glDeleteProgram(m_Program);
        m_Program = 0;

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

void Shader::Bind()
{
    glUseProgram(m_Program);
}

void Shader::Unbind()
{
    glUseProgram(0);
}

void Shader::Shutdown()
{
	if (m_Program != 0)
	{
		glDeleteProgram(m_Program);
		m_Program = 0;
	}
}

void Shader::SetMat4(const char* name, const Mat4& matrix)
{
    int location = glGetUniformLocation(
        m_Program,
        name
    );

    if (location == -1)
    {
        return;
    }

    glUniformMatrix4fv(
        location,
        1,
        GL_FALSE,
        matrix.elements
    );
}

void Shader::SetVec4(
    const char* name,
    float x,
    float y,
    float z,
    float w)
{
    int location = glGetUniformLocation(
        m_Program,
        name
    );

    if (location == -1)
    {
        return;
    }

    glUniform4f(
        location,
        x,
        y,
        z,
        w
    );
}

void Shader::SetVec3(
    const char* name,
    float x,
    float y,
    float z)
{
    GLint location =
        glGetUniformLocation(
            m_Program,
            name
        );

    if (location == -1)
    {
        return;
    }

    glUniform3f(
        location,
        x,
        y,
        z
    );
}

void Shader::SetFloat(
    const char* name,
    float value)
{
    GLint location =
        glGetUniformLocation(
            m_Program,
            name
        );

    if (location == -1)
    {
        return;
    }

    glUniform1f(
        location,
        value
    );
}

void Shader::SetInt(
    const char* name,
    int value)
{
    GLint location =
        glGetUniformLocation(
            m_Program,
            name
        );

    if (location == -1)
    {
        return;
    }

    glUniform1i(
        location,
        value
    );
}