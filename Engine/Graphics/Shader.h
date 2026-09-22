#pragma once

#include "../Math/Mat4.h"

class Shader
{
public:
    Shader();
    ~Shader();

    bool Initialize(const char* vertexSource, const char* fragmentSource);

    void Bind();
    void Unbind();
    void Shutdown();

    void SetMat4(const char* name, const Mat4& matrix);
    void SetVec4( const char* name, float x, float y, float z, float w);
    void SetVec3(const char* name, float x, float y, float z);
    void SetFloat(const char* name, float value);
    void SetInt(const char* name, int value);

private:
    unsigned int m_Program;
};