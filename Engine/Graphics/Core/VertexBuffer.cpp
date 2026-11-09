#include "VertexBuffer.h"
#include "../RHI/RHI.h"

VertexBuffer::~VertexBuffer() { Shutdown(); }

bool VertexBuffer::Initialize(const void* data, unsigned int size)
{
    Shutdown();
    auto* device = Velcryn::RHI::GetDevice();
    if (!device || !data || !size) return false;
    Velcryn::RHI::BufferDesc desc{};
    desc.size = size;
    desc.usage = Velcryn::RHI::BufferUsage::Vertex;
    desc.debugName = "VertexBuffer";
    m_Handle = device->CreateBuffer(desc, data);
    return static_cast<bool>(m_Handle);
}

void VertexBuffer::Shutdown()
{
    if (m_Handle && Velcryn::RHI::GetDevice())
        Velcryn::RHI::GetDevice()->DestroyBuffer(m_Handle);
    m_Handle = {};
}
