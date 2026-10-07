#pragma once
#include "../Math/Vec3.h"
#include "../Math/Mat4.h"
#include "RHI/RHITypes.h"
class DebugRenderer{public:DebugRenderer();~DebugRenderer();bool Initialize();void Shutdown();void DrawDirectionalLight(const Mat4&,const Mat4&,const Vec3&,const Vec3&);void DrawLine(const Mat4&,const Mat4&,const Vec3&,const Vec3&,const Vec3&);void DrawBox(const Mat4&,const Mat4&,const Vec3&,const Vec3&,const Vec3&);private:Velcryn::RHI::BufferHandle m_VertexBuffer{};};
