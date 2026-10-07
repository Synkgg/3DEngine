#pragma once
#include "RHI/RHITypes.h"
class Grid{public:Grid();~Grid();bool Initialize();void Shutdown();void Draw();private:Velcryn::RHI::BufferHandle m_VertexBuffer{};unsigned int m_VertexCount=0;};
