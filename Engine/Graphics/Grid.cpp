#include "Grid.h"
#include "RHI/RHI.h"
#include <Grid.vert.h>
#include <Grid.frag.h>

bool Grid::Initialize(unsigned int samples)
{
    using namespace Velcryn::RHI;
    auto* device = GetDevice();
    if (!device) return false;
    const float vertices[] = {-1,-1, 1,-1, 1,1, -1,1};
    const uint32_t indices[] = {0,1,2, 0,2,3};
    BufferDesc buffer{};
    buffer.size = sizeof(vertices); buffer.usage = BufferUsage::Vertex;
    m_Vertices = device->CreateBuffer(buffer, vertices);
    buffer.size = sizeof(indices); buffer.usage = BufferUsage::Index;
    m_Indices = device->CreateBuffer(buffer, indices);
    const VertexAttribute attribute{0, 0, VertexFormat::Float2};
    GraphicsPipelineDesc pipeline{}; pipeline.sampleCount=samples;
    pipeline.vertexShader = Grid_vert; pipeline.fragmentShader = Grid_frag;
    pipeline.attributes = std::span(&attribute, 1); pipeline.vertexStride = sizeof(float)*2;
    pipeline.constantSize = sizeof(Mat4)+sizeof(float)*4;
    pipeline.depthWrite = false; pipeline.cullBackFaces = false; pipeline.alphaBlend = true;
    pipeline.debugName = "EditorGrid";
    m_Pipeline = device->CreateGraphicsPipeline(pipeline);
    if (!m_Vertices || !m_Indices || !m_Pipeline) { Shutdown(); return false; }
    return true;
}

void Grid::Shutdown()
{
    if (auto* device = Velcryn::RHI::GetDevice()) {
        device->DestroyBuffer(m_Vertices); device->DestroyBuffer(m_Indices);
        device->DestroyPipeline(m_Pipeline);
    }
    m_Vertices = {}; m_Indices = {}; m_Pipeline = {};
}

void Grid::Draw(const Mat4& viewProjection, const Vec3& camera)
{
    struct Constants { Mat4 viewProjection; float camera[4]; };
    const Constants constants{viewProjection, {camera.x, camera.y, camera.z, 0}};
    if (auto* device = Velcryn::RHI::GetDevice())
        device->DrawIndexed(m_Pipeline, m_Vertices, m_Indices, 6, &constants, sizeof(constants));
}
