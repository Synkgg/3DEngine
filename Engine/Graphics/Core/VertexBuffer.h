#pragma once
#include "../RHI/RHITypes.h"

class VertexBuffer
{
public:
    VertexBuffer() = default;
    ~VertexBuffer();
    bool Initialize(const void* data, unsigned int size);
    void Bind() {}
    void Unbind() {}
    void Shutdown();
    Velcryn::RHI::BufferHandle GetHandle() const { return m_Handle; }
private:
    Velcryn::RHI::BufferHandle m_Handle{};
};
