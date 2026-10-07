#include "IndexBuffer.h"
#include "../RHI/RHI.h"

IndexBuffer::~IndexBuffer() { Shutdown(); }

bool IndexBuffer::Initialize(const std::uint32_t* indices, std::uint32_t count)
{
    Shutdown();
    auto* device = Velcryn::RHI::GetDevice();
    if (!device || !indices || !count) return false;
    Velcryn::RHI::BufferDesc desc{};
    desc.size = static_cast<std::uint64_t>(count) * sizeof(std::uint32_t);
    desc.usage = Velcryn::RHI::BufferUsage::Index;
    desc.debugName = "IndexBuffer";
    m_Handle = device->CreateBuffer(desc, indices);
    if (!m_Handle) return false;
    m_Count = count;
    return true;
}

void IndexBuffer::Shutdown()
{
    if (m_Handle && Velcryn::RHI::GetDevice())
        Velcryn::RHI::GetDevice()->DestroyBuffer(m_Handle);
    m_Handle = {};
    m_Count = 0;
}
