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
        if (!m_Device || desc.vertexShader.empty() || (desc.fragmentShader.empty() && desc.colorFormat != TextureFormat::Unknown) ||
            desc.attributes.size() > 255 || desc.vertexStride > 65535 || desc.constantSize % 4)
            return {};
        PipelineSlot slot{};
        slot.stride = desc.vertexStride;
        slot.constantSize = desc.constantSize;
        slot.sampledTexture = desc.sampledTexture;
        slot.uniformSize = desc.uniformSize;
        slot.textureCount = desc.textureCount; slot.rawTextureMask = desc.rawTextureMask;
        slot.displayEncodedTexture = desc.displayEncodedTexture;
        nri::RootConstantDesc constants{0, desc.constantSize, nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER};
        nri::DescriptorRangeDesc ranges[2]{};
        auto& textureRange = ranges[0];
        ranges[1] = {32, 1, nri::DescriptorType::CONSTANT_BUFFER, constants.shaderStages};
        textureRange.baseRegisterIndex = 0;
        textureRange.descriptorNum = desc.textureCount;
        textureRange.descriptorType = nri::DescriptorType::TEXTURE;
        textureRange.shaderStages = nri::StageBits::FRAGMENT_SHADER;
        nri::DescriptorSetDesc textureSet{};
        textureSet.registerSpace = 0;
        textureSet.ranges = &textureRange;
        textureSet.rangeNum = desc.uniformSize ? 2 : 1;
        nri::RootSamplerDesc sampler{};
        sampler.registerIndex = 0;
        sampler.shaderStages = nri::StageBits::FRAGMENT_SHADER;
        sampler.desc.filters = {nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR, nri::FilterOp::AVERAGE};
        sampler.desc.addressModes = {nri::AddressMode::REPEAT, nri::AddressMode::REPEAT, nri::AddressMode::REPEAT};
        sampler.desc.mipMax = 16.0f;
        if (desc.clampSampler) sampler.desc.addressModes = {nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE};
        nri::PipelineLayoutDesc layout{};
        // NRI requires the root-parameter register space to be unique from every
        // descriptor-set register space. Keep push constants in space 0 and put
        // sampled resources in space 1.
        layout.rootRegisterSpace = 0;
        textureSet.registerSpace = 1;
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
        nri::MultisampleDesc multisample{}; multisample.sampleNum=static_cast<nri::Sample_t>(desc.sampleCount); multisample.sampleMask=nri::ALL;
        nri::GraphicsPipelineDesc pipeline{};
        pipeline.multisample=&multisample;
        pipeline.pipelineLayout = slot.layout;
        pipeline.vertexInput = &input;
        pipeline.inputAssembly.topology = desc.lineList ? nri::Topology::LINE_LIST : nri::Topology::TRIANGLE_LIST;
        pipeline.rasterization.cullMode = desc.cullFrontFaces ? nri::CullMode::FRONT : desc.cullBackFaces ? nri::CullMode::BACK : nri::CullMode::NONE;
        pipeline.rasterization.frontCounterClockwise = true;
        nri::ColorAttachmentDesc colors[2] = {color, color};
        pipeline.outputMerger.colors = colors; pipeline.outputMerger.colorNum = desc.colorFormat == TextureFormat::Unknown ? 0 : (desc.secondColor ? 2 : 1);
        pipeline.outputMerger.depthStencilFormat = ToNRIFormat(desc.depthFormat);
        pipeline.outputMerger.depth.compareOp = desc.depthTest ? (desc.depthLessEqual ? nri::CompareOp::LESS_EQUAL : nri::CompareOp::LESS) : nri::CompareOp::NONE;
        pipeline.outputMerger.depth.write = desc.depthWrite;
        pipeline.shaders = shaders; pipeline.shaderNum = desc.fragmentShader.empty() ? 1 : 2;
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

    bool NRIDevice::BeginRendering(TextureHandle color, TextureHandle depth, const float clearColor[4], bool clear, TextureHandle secondColor, TextureHandle resolveColor, TextureHandle resolveDepth, TextureHandle resolveNormal)
    {
        if (!m_FrameOpen || m_Rendering || (!color && !depth) || !clearColor) return false;
        auto valid = [&](TextureHandle h) { return !h || (h.index <= m_Textures.size() && m_Textures[h.index-1].generation == h.generation && m_Textures[h.index-1].attachment); };
        if (!valid(resolveColor) || !valid(resolveDepth) || !valid(resolveNormal) || !valid(color) || !valid(depth) || !valid(secondColor) || (secondColor && !color)) return false;
        auto& target = m_Textures[(color ? color : depth).index-1];
        nri::AttachmentDesc colors[2]{}, da{};
        TextureHandle handles[2] = {color, secondColor};
        for (int i=0;i<2;++i) if (handles[i]) {
            auto& c = m_Textures[handles[i].index-1];
            if (c.desc.width != target.desc.width || c.desc.height != target.desc.height) return false;
            TransitionTexture(c, {nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT});
            colors[i].descriptor=c.attachment; colors[i].loadOp=clear?nri::LoadOp::CLEAR:nri::LoadOp::LOAD; colors[i].storeOp=nri::StoreOp::STORE;
            std::memcpy(&colors[i].clearValue.color.f, clearColor, sizeof(float)*4);
        }
        if (depth) {
            auto& d=m_Textures[depth.index-1];
            if (d.desc.width != target.desc.width || d.desc.height != target.desc.height) return false;
            TransitionTexture(d, {nri::AccessBits::DEPTH_STENCIL_ATTACHMENT, nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::StageBits::DEPTH_STENCIL_ATTACHMENT});
            da.descriptor=d.attachment; da.loadOp=clear?nri::LoadOp::CLEAR:nri::LoadOp::LOAD; da.storeOp=nri::StoreOp::STORE; da.clearValue.depthStencil.depth=1;
        }
        TextureHandle resolves[]={resolveColor,resolveNormal,resolveDepth};
        TextureHandle sources[]={color,secondColor,depth};
        nri::AttachmentDesc* attachments[]={&colors[0],&colors[1],&da};
        for(int i=0;i<3;++i) if(resolves[i]) {
            if(!sources[i]) return false;
            auto& dst=m_Textures[resolves[i].index-1];
            if(dst.desc.width!=target.desc.width || dst.desc.height!=target.desc.height || dst.desc.sampleCount!=1 || target.desc.sampleCount<=1) return false;
            TransitionTexture(dst,i==2 ? nri::AccessLayoutStage{nri::AccessBits::DEPTH_STENCIL_ATTACHMENT,nri::Layout::DEPTH_STENCIL_ATTACHMENT,nri::StageBits::DEPTH_STENCIL_ATTACHMENT} : nri::AccessLayoutStage{nri::AccessBits::COLOR_ATTACHMENT,nri::Layout::COLOR_ATTACHMENT,nri::StageBits::COLOR_ATTACHMENT});
            attachments[i]->resolveDst=dst.attachment;
            attachments[i]->resolveOp=i==2?nri::ResolveOp::MIN:nri::ResolveOp::AVERAGE;
        }
        m_RenderResolves={resolveColor,resolveNormal,resolveDepth};
        nri::RenderingDesc rendering{};
        rendering.colors=colors; rendering.colorNum=color?(secondColor?2:1):0; rendering.depth=da;
        m_Core.CmdBeginRendering(*m_FrameCommandBuffer, rendering);
        nri::Viewport viewport{0,0,float(target.desc.width),float(target.desc.height),0,1};
        nri::Rect scissor{0,0,static_cast<nri::Dim_t>(target.desc.width),static_cast<nri::Dim_t>(target.desc.height)};
        m_Core.CmdSetViewports(*m_FrameCommandBuffer,&viewport,1); m_Core.CmdSetScissors(*m_FrameCommandBuffer,&scissor,1);
        m_RenderColor=color; m_RenderSecondColor=secondColor; m_RenderDepth=depth; m_Rendering=true;
        return true;
    }

    void NRIDevice::EndRendering()
    {
        if (!m_Rendering) return;
        m_Core.CmdEndRendering(*m_FrameCommandBuffer);
        for (auto handle : {m_RenderColor, m_RenderSecondColor, m_RenderDepth, m_RenderResolves[0], m_RenderResolves[1], m_RenderResolves[2]}) if (handle)
            TransitionTexture(m_Textures[handle.index-1], {nri::AccessBits::SHADER_RESOURCE,nri::Layout::SHADER_RESOURCE,nri::StageBits::FRAGMENT_SHADER});
        m_RenderResolves={}; m_RenderColor={}; m_RenderSecondColor={}; m_RenderDepth={}; m_Rendering=false;
    }

    bool NRIDevice::DrawIndexed(PipelineHandle pipeline, BufferHandle vertices, BufferHandle indices,
        uint32_t count, const void* constants, uint32_t constantSize, uint32_t instances, TextureHandle texture, std::span<const std::byte> uniforms, std::span<const TextureHandle> additionalTextures)
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
        if (uniforms.size() != p.uniformSize) return false;
        if (p.sampledTexture)
        {
            if (!m_DescriptorPool || !texture || texture.index > m_Textures.size()) return false;
            const auto& t = m_Textures[texture.index - 1];
            if (t.generation != texture.generation || !t.shaderResource) return false;
            nri::DescriptorSet* set = nullptr;
            if (m_Core.AllocateDescriptorSets(*m_DescriptorPool, *p.layout, 0, &set, 1, 0) != nri::Result::SUCCESS || !set)
                return false;
            if (additionalTextures.size()+1 != p.textureCount) return false;
            std::vector<nri::Descriptor*> descriptors;
            for (uint32_t index=0; index<p.textureCount; ++index) {
                const auto handle = index == 0 ? texture : additionalTextures[index-1];
                if (!handle || handle.index > m_Textures.size()) return false;
                const auto& source=m_Textures[handle.index-1];
                if (source.generation != handle.generation || !source.shaderResource) return false;
                const bool raw=p.displayEncodedTexture || ((p.rawTextureMask>>index)&1);
                descriptors.push_back(raw && source.imguiResource ? source.imguiResource : source.shaderResource);
            }
            nri::UpdateDescriptorRangeDesc update{};
            update.descriptorSet=set; update.rangeIndex=0; update.baseDescriptor=0;
            update.descriptors=descriptors.data(); update.descriptorNum=p.textureCount;
            m_Core.UpdateDescriptorRanges(&update,1);
            if (p.uniformSize)
            {
                const auto alignment = m_Core.GetDeviceDesc(*m_Device).memoryAlignment.constantBufferOffset;
                const uint64_t size = (p.uniformSize + alignment - 1) / alignment * alignment;
                auto& arena = m_Buffers[m_DrawUniformBuffer.index - 1];
                if (m_DrawUniformOffset + size > arena.desc.size) {
                    Logger::Error("RHI: per-frame draw uniform arena exhausted."); return false;
                }
                auto* resource = arena.resource;
                void* mapped = m_Core.MapBuffer(*resource, m_DrawUniformOffset, size);
                if (!mapped) return false;
                std::memcpy(mapped, uniforms.data(), uniforms.size());
                m_Core.UnmapBuffer(*resource);
                nri::BufferViewDesc view{};
                view.buffer = resource; view.type = nri::BufferView::CONSTANT_BUFFER;
                view.offset = m_DrawUniformOffset; view.size = size;
                m_DrawUniformOffset += size;
                nri::Descriptor* constantView = nullptr;
                if (m_Core.CreateBufferView(view, constantView) != nri::Result::SUCCESS) return false;
                update.rangeIndex = 1; update.descriptors = &constantView; update.descriptorNum = 1;
                m_Core.UpdateDescriptorRanges(&update, 1);
                m_Garbage.push_back([this, constantView] { m_Core.DestroyDescriptor(constantView); });
            }
            nri::SetDescriptorSetDesc bind{}; bind.setIndex = 0; bind.descriptorSet = set; bind.bindPoint = nri::BindPoint::GRAPHICS;
            m_Core.CmdSetDescriptorSet(*m_FrameCommandBuffer, bind);
        }
        nri::DrawIndexedDesc draw{}; draw.indexNum = count; draw.instanceNum = instances;
        m_Core.CmdDrawIndexed(*m_FrameCommandBuffer, draw);
        return true;
    }
}
