#include "NRIDevice.h"
#include "../../../Core/Logger.h"
#include <cstring>

namespace Velcryn::RHI
{
    bool NRIDevice::ReadTexture(TextureHandle handle, std::vector<std::uint8_t>& bytes)
    {
        bytes.clear();
        if (m_FrameOpen || !handle || handle.index > m_Textures.size()) return false;
        auto& texture = m_Textures[handle.index - 1];
        if (!texture.resource || texture.generation != handle.generation || texture.desc.depth != 1 || texture.desc.sampleCount != 1 ||
            texture.desc.format == TextureFormat::D32_Float || texture.desc.format == TextureFormat::D24S8) return false;
        WaitIdle();
        const auto& alignment = m_Core.GetDeviceDesc(*m_Device).memoryAlignment;
        const uint32_t rowBytes = texture.desc.width * nri::nriGetFormatProps(ToNRIFormat(texture.desc.format))->stride;
        const auto align = [](uint32_t value, uint32_t granularity) { return (value + granularity - 1) / granularity * granularity; };
        nri::TextureDataLayoutDesc data{};
        data.rowPitch = align(rowBytes, alignment.uploadBufferTextureRow);
        data.slicePitch = align(data.rowPitch * texture.desc.height, alignment.uploadBufferTextureSlice);
        BufferDesc bufferDesc{}; bufferDesc.size = data.slicePitch; bufferDesc.usage = BufferUsage::Readback;
        const auto buffer = CreateBuffer(bufferDesc, nullptr);
        if (!buffer) return false;
        auto* resource = m_Buffers[buffer.index - 1].resource;
        auto command = BeginCommandList(QueueType::Graphics);
        if (!command) { DestroyBuffer(buffer); return false; }
        auto& slot = m_CommandLists[command.index - 1];
        nri::TextureBarrierDesc transition{};
        transition.texture = texture.resource; transition.before = texture.state;
        transition.after = {nri::AccessBits::COPY_SOURCE, nri::Layout::COPY_SOURCE, nri::StageBits::COPY};
        transition.mipNum = nri::REMAINING; transition.layerNum = nri::REMAINING; transition.planes = nri::PlaneBits::COLOR;
        nri::BarrierDesc barrier{}; barrier.textures = &transition; barrier.textureNum = 1;
        m_Core.CmdBarrier(*slot.commandBuffer, barrier);
        nri::TextureRegionDesc region{};
        region.width = static_cast<nri::Dim_t>(texture.desc.width); region.height = static_cast<nri::Dim_t>(texture.desc.height);
        region.depth = 1; region.planes = nri::PlaneBits::COLOR;
        m_Core.CmdReadbackTextureToBuffer(*slot.commandBuffer, *resource, data, *texture.resource, region);
        std::swap(transition.before, transition.after);
        m_Core.CmdBarrier(*slot.commandBuffer, barrier);
        EndCommandList(command); Submit(command); WaitIdle();
        const auto* mapped = static_cast<const uint8_t*>(m_Core.MapBuffer(*resource, 0, data.slicePitch));
        if (mapped)
        {
            bytes.resize(static_cast<size_t>(rowBytes) * texture.desc.height);
            for (uint32_t y = 0; y < texture.desc.height; ++y)
                std::memcpy(bytes.data() + static_cast<size_t>(y) * rowBytes, mapped + static_cast<size_t>(y) * data.rowPitch, rowBytes);
            m_Core.UnmapBuffer(*resource);
        }
        m_Core.DestroyCommandBuffer(slot.commandBuffer); m_Core.DestroyCommandAllocator(slot.allocator);
        slot.commandBuffer = nullptr; slot.allocator = nullptr; ++slot.generation;
        DestroyBuffer(buffer); CollectGarbage();
        return !bytes.empty();
    }

    PipelineHandle NRIDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
    {
        if (!m_Device || desc.vertexShader.empty() || desc.fragmentShader.empty() ||
            desc.attributes.size() > 255 || desc.vertexStride > 65535 || desc.constantSize % 4)
            return {};
        PipelineSlot slot{};
        slot.stride = desc.vertexStride;
        slot.constantSize = desc.constantSize;
        slot.sampledTexture = desc.sampledTexture;
        nri::RootConstantDesc constants{0, desc.constantSize, nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER};
        nri::DescriptorRangeDesc textureRange{};
        textureRange.baseRegisterIndex = 0;
        textureRange.descriptorNum = 1;
        textureRange.descriptorType = nri::DescriptorType::TEXTURE;
        textureRange.shaderStages = nri::StageBits::FRAGMENT_SHADER;
        nri::DescriptorSetDesc textureSet{};
        textureSet.registerSpace = 0;
        textureSet.ranges = &textureRange;
        textureSet.rangeNum = 1;
        nri::RootSamplerDesc sampler{};
        sampler.registerIndex = 0;
        sampler.shaderStages = nri::StageBits::FRAGMENT_SHADER;
        sampler.desc.filters = {nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR, nri::FilterOp::AVERAGE};
        sampler.desc.addressModes = {nri::AddressMode::REPEAT, nri::AddressMode::REPEAT, nri::AddressMode::REPEAT};
        sampler.desc.mipMax = 16.0f;
        nri::PipelineLayoutDesc layout{};
        layout.shaderStages = constants.shaderStages;
        layout.rootConstants = desc.constantSize ? &constants : nullptr;
        layout.rootConstantNum = desc.constantSize ? 1 : 0;
        layout.rootSamplers = desc.sampledTexture ? &sampler : nullptr;
        layout.rootSamplerNum = desc.sampledTexture ? 1 : 0;
        layout.descriptorSets = desc.sampledTexture ? &textureSet : nullptr;
        layout.descriptorSetNum = desc.sampledTexture ? 1 : 0;
        if (m_Core.CreatePipelineLayout(*m_Device, layout, slot.layout) != nri::Result::SUCCESS)
            return {};
        std::vector<nri::VertexAttributeDesc> attributes;
        for (const auto& source : desc.attributes)
        {
            nri::VertexAttributeDesc a{};
            a.vk.location = source.location;
            a.offset = source.offset;
            a.format = source.format == VertexFormat::Float2 ? nri::Format::RG32_SFLOAT :
                source.format == VertexFormat::Float3 ? nri::Format::RGB32_SFLOAT : nri::Format::RGBA32_SFLOAT;
            attributes.push_back(a);
        }
        nri::VertexStreamDesc stream{};
        stream.stride = static_cast<uint16_t>(desc.vertexStride);
        nri::VertexInputDesc input{};
        input.attributes = attributes.data(); input.attributeNum = static_cast<uint8_t>(attributes.size());
        input.streams = &stream; input.streamNum = 1;
        nri::ShaderDesc shaders[2]{};
        shaders[0] = {nri::StageBits::VERTEX_SHADER, desc.vertexShader.data(), desc.vertexShader.size_bytes(), "main"};
        shaders[1] = {nri::StageBits::FRAGMENT_SHADER, desc.fragmentShader.data(), desc.fragmentShader.size_bytes(), "main"};
        nri::ColorAttachmentDesc color{};
        color.format = ToNRIFormat(desc.colorFormat);
        color.colorWriteMask = nri::ColorWriteBits::RGBA;
        color.blendEnabled = desc.alphaBlend;
        if (desc.alphaBlend)
        {
            color.colorBlend = {nri::BlendFactor::SRC_ALPHA, nri::BlendFactor::ONE_MINUS_SRC_ALPHA, nri::BlendOp::ADD};
            color.alphaBlend = {nri::BlendFactor::ONE, nri::BlendFactor::ONE_MINUS_SRC_ALPHA, nri::BlendOp::ADD};
        }
        nri::GraphicsPipelineDesc pipeline{};
        pipeline.pipelineLayout = slot.layout;
        pipeline.vertexInput = &input;
        pipeline.inputAssembly.topology = nri::Topology::TRIANGLE_LIST;
        pipeline.rasterization.cullMode = desc.cullBackFaces ? nri::CullMode::BACK : nri::CullMode::NONE;
        pipeline.rasterization.frontCounterClockwise = true;
        pipeline.outputMerger.colors = &color; pipeline.outputMerger.colorNum = 1;
        pipeline.outputMerger.depthStencilFormat = ToNRIFormat(desc.depthFormat);
        pipeline.outputMerger.depth.compareOp = desc.depthTest ? nri::CompareOp::LESS : nri::CompareOp::NONE;
        pipeline.outputMerger.depth.write = desc.depthWrite;
        pipeline.shaders = shaders; pipeline.shaderNum = 2;
        if (m_Core.CreateGraphicsPipeline(*m_Device, pipeline, slot.resource) != nri::Result::SUCCESS)
        {
            m_Core.DestroyPipelineLayout(slot.layout);
            Logger::Error("RHI: graphics pipeline creation failed.");
            return {};
        }
        if (desc.debugName) m_Core.SetDebugName(slot.resource, desc.debugName);
        for (uint32_t i = 0; i < m_Pipelines.size(); ++i)
            if (!m_Pipelines[i].resource)
            {
                slot.generation = m_Pipelines[i].generation;
                m_Pipelines[i] = slot;
                return {i + 1, slot.generation};
            }
        m_Pipelines.push_back(slot);
        return {static_cast<uint32_t>(m_Pipelines.size()), slot.generation};
    }

    void NRIDevice::DestroyPipeline(PipelineHandle handle)
    {
        if (!handle || handle.index > m_Pipelines.size()) return;
        auto& slot = m_Pipelines[handle.index - 1];
        if (slot.generation != handle.generation || !slot.resource) return;
        m_Garbage.push_back([this, resource = slot.resource, layout = slot.layout] {
            m_Core.DestroyPipeline(resource); m_Core.DestroyPipelineLayout(layout);
        });
        slot.resource = nullptr; slot.layout = nullptr; ++slot.generation;
    }

    void NRIDevice::CollectGarbage()
    {
        for (auto& destroy : m_Garbage) destroy();
        m_Garbage.clear();
    }

    void NRIDevice::TransitionTexture(TextureSlot& slot, nri::AccessLayoutStage after)
    {
        nri::TextureBarrierDesc texture{};
        texture.texture = slot.resource; texture.before = slot.state; texture.after = after;
        texture.mipNum = nri::REMAINING; texture.layerNum = nri::REMAINING;
        texture.planes = (slot.desc.format == TextureFormat::D32_Float || slot.desc.format == TextureFormat::D24S8)
            ? nri::PlaneBits::DEPTH : nri::PlaneBits::COLOR;
        nri::BarrierDesc barrier{};
        barrier.textures = &texture; barrier.textureNum = 1;
        m_Core.CmdBarrier(*m_FrameCommandBuffer, barrier);
        slot.state = after;
    }

    bool NRIDevice::BeginRendering(TextureHandle color, TextureHandle depth, const float clearColor[4], bool clear)
    {
        if (!m_FrameOpen || m_Rendering || !color || !depth || !clearColor ||
            color.index > m_Textures.size() || depth.index > m_Textures.size()) return false;
        auto& c = m_Textures[color.index - 1]; auto& d = m_Textures[depth.index - 1];
        if (c.generation != color.generation || d.generation != depth.generation || !c.attachment || !d.attachment ||
            c.desc.width != d.desc.width || c.desc.height != d.desc.height) return false;
        TransitionTexture(c, {nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT});
        TransitionTexture(d, {nri::AccessBits::DEPTH_STENCIL_ATTACHMENT, nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::StageBits::DEPTH_STENCIL_ATTACHMENT});
        nri::AttachmentDesc ca{}, da{};
        ca.descriptor = c.attachment; ca.loadOp = clear ? nri::LoadOp::CLEAR : nri::LoadOp::LOAD; ca.storeOp = nri::StoreOp::STORE;
        std::memcpy(&ca.clearValue.color.f, clearColor, sizeof(float) * 4);
        da.descriptor = d.attachment; da.loadOp = clear ? nri::LoadOp::CLEAR : nri::LoadOp::LOAD; da.storeOp = nri::StoreOp::STORE;
        da.clearValue.depthStencil.depth = 1.0f;
        nri::RenderingDesc rendering{};
        rendering.colors = &ca; rendering.colorNum = 1; rendering.depth = da;
        m_Core.CmdBeginRendering(*m_FrameCommandBuffer, rendering);
        nri::Viewport viewport{0, 0, static_cast<float>(c.desc.width), static_cast<float>(c.desc.height), 0, 1};
        nri::Rect scissor{0, 0, static_cast<nri::Dim_t>(c.desc.width), static_cast<nri::Dim_t>(c.desc.height)};
        m_Core.CmdSetViewports(*m_FrameCommandBuffer, &viewport, 1);
        m_Core.CmdSetScissors(*m_FrameCommandBuffer, &scissor, 1);
        m_RenderColor = color; m_RenderDepth = depth; m_Rendering = true;
        return true;
    }

    void NRIDevice::EndRendering()
    {
        if (!m_Rendering) return;
        m_Core.CmdEndRendering(*m_FrameCommandBuffer);
        auto& color = m_Textures[m_RenderColor.index - 1];
        TransitionTexture(color, {nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::FRAGMENT_SHADER});
        m_RenderColor = {}; m_RenderDepth = {}; m_Rendering = false;
    }

    bool NRIDevice::DrawIndexed(PipelineHandle pipeline, BufferHandle vertices, BufferHandle indices,
        uint32_t count, const void* constants, uint32_t constantSize, uint32_t instances, TextureHandle texture)
    {
        if (!m_Rendering || !pipeline || !vertices || !indices || !count || !instances ||
            pipeline.index > m_Pipelines.size() || vertices.index > m_Buffers.size() || indices.index > m_Buffers.size()) return false;
        const auto& p = m_Pipelines[pipeline.index - 1];
        const auto& v = m_Buffers[vertices.index - 1]; const auto& i = m_Buffers[indices.index - 1];
        if (p.generation != pipeline.generation || v.generation != vertices.generation || i.generation != indices.generation ||
            !p.resource || !v.resource || !i.resource || constantSize != p.constantSize || (constantSize && !constants) ||
            i.desc.usage != BufferUsage::Index || v.desc.usage != BufferUsage::Vertex ||
            static_cast<uint64_t>(count) * sizeof(uint32_t) > i.desc.size) return false;
        m_Core.CmdSetPipelineLayout(*m_FrameCommandBuffer, nri::BindPoint::GRAPHICS, *p.layout);
        m_Core.CmdSetPipeline(*m_FrameCommandBuffer, *p.resource);
        nri::VertexBufferDesc vertex{v.resource, 0, p.stride};
        m_Core.CmdSetVertexBuffers(*m_FrameCommandBuffer, 0, &vertex, 1);
        m_Core.CmdSetIndexBuffer(*m_FrameCommandBuffer, *i.resource, 0, nri::IndexType::UINT32);
        if (constantSize)
        {
            nri::SetRootConstantsDesc root{}; root.data = constants; root.size = constantSize;
            m_Core.CmdSetRootConstants(*m_FrameCommandBuffer, root);
        }
        if (p.sampledTexture)
        {
            if (!m_DescriptorPool || !texture || texture.index > m_Textures.size()) return false;
            const auto& t = m_Textures[texture.index - 1];
            if (t.generation != texture.generation || !t.shaderResource) return false;
            nri::DescriptorSet* set = nullptr;
            if (m_Core.AllocateDescriptorSets(*m_DescriptorPool, *p.layout, 0, &set, 1, 0) != nri::Result::SUCCESS || !set)
                return false;
            nri::Descriptor* descriptor = t.shaderResource;
            nri::UpdateDescriptorRangeDesc update{};
            update.descriptorSet = set; update.rangeIndex = 0; update.baseDescriptor = 0;
            update.descriptors = &descriptor; update.descriptorNum = 1;
            m_Core.UpdateDescriptorRanges(&update, 1);
            nri::SetDescriptorSetDesc bind{}; bind.setIndex = 0; bind.descriptorSet = set; bind.bindPoint = nri::BindPoint::GRAPHICS;
            m_Core.CmdSetDescriptorSet(*m_FrameCommandBuffer, bind);
        }
        nri::DrawIndexedDesc draw{}; draw.indexNum = count; draw.instanceNum = instances;
        m_Core.CmdDrawIndexed(*m_FrameCommandBuffer, draw);
        return true;
    }
}
