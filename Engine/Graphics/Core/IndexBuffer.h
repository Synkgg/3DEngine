#pragma once
#include "../RHI/RHITypes.h"
#include <cstdint>

class IndexBuffer
{
public:
    IndexBuffer() = default;
    ~IndexBuffer();
    bool Initialize(const std::uint32_t* indices, std::uint32_t count);
    void Bind() {}
    void Unbind() {}
    void Shutdown();
    std::uint32_t GetCount() const { return m_Count; }
    Velcryn::RHI::BufferHandle GetHandle() const { return m_Handle; }
private:
    Velcryn::RHI::BufferHandle m_Handle{};
    std::uint32_t m_Count = 0;
};
