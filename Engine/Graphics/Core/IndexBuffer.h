#pragma once

#include <cstdint>

class IndexBuffer
{
public:
    IndexBuffer();
    ~IndexBuffer();

    bool Initialize(
        const std::uint32_t* indices,
        std::uint32_t count
    );

    void Bind();
    void Unbind();
    void Shutdown();

    std::uint32_t GetCount() const;

private:
    unsigned int m_RendererID;
    std::uint32_t m_Count;
};