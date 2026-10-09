#pragma once
#include "RHI/RHITypes.h"
#include "../Math/Mat4.h"
#include "../Math/Vec3.h"
class Grid {
public:
    ~Grid() { Shutdown(); }
    bool Initialize(unsigned int samples=1);
    void Shutdown();
    void Draw(const Mat4& viewProjection, const Vec3& camera);
private:
    Velcryn::RHI::BufferHandle m_Vertices{}, m_Indices{};
    Velcryn::RHI::PipelineHandle m_Pipeline{};
};
