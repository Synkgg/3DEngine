# Rendering architecture

The renderer is being split by responsibility while `Renderer` remains the
engine-facing facade.

- **LightingSystem** — Cook-Torrance direct lighting, local-light data and
  indirect-light policy.
- **EnvironmentSystem** — procedural sky today; HDR cubemap/irradiance/
  prefiltered-specular IBL belongs here.
- **ShadowSystem** — directional shadow resources, stabilization and the future
  cascade/local-light shadow implementation.
- **ReflectionSystem** — conservative screen-space reflections today and
  reflection probes as the fallback path.
- **PostProcessSystem** — depth-aware AO, screen-space bounce, bloom,
  atmosphere, exposure, grading and future motion-vector TAA.

This boundary is intentionally introduced without changing the public Renderer
API. Systems can be extracted incrementally without forcing Scene, Runtime or
editor code to know about graphics implementation details.


## RHI boundary

Velcryn now owns a low-level RHI contract under `Engine/Graphics/RHI`.
The renderer and future RenderGraph should depend on this contract rather than
NRI, Vulkan, D3D12, or platform headers directly.

Initial migration order:

1. Device/capabilities and engine-owned handles/descriptors.
2. Buffers, textures, samplers, and command submission.
3. Pipeline/shader binding and render targets.
4. Move Framebuffer, VertexBuffer/IndexBuffer, Texture2D, and Shader behind RHI.
5. Introduce RenderGraph resource/pass scheduling.
6. Add the NRI-backed Vulkan device, then D3D12 without changing renderer-facing APIs.

The existing OpenGL path remains operational while this migration is underway.
Backend-specific capabilities are queried through `RHICapabilities`; higher
layers must not branch on Vulkan/D3D12 unless the behavior is truly API-specific.
