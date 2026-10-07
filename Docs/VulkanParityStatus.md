# Vulkan parity implementation status

The full 30-phase migration is **not complete**. This change establishes a working
basic scene pass; the advanced renderer remains unfinished. Do not use a successful
build or the smoke test as evidence of full OpenGL visual parity.

## Implemented in this change

- SDR swapchain format logging; ImGui vertex de-gamma follows the attachment format.
- Separate UNORM editor image views for sRGB textures; 64-bit editor image IDs.
- HLSL to SPIR-V through DXC, configure-time compiler probe, embedded shader bytecode.
- Engine-owned graphics pipeline description, vertex layout, depth/cull state,
  root constants, generational pipeline handles and deferred pipeline destruction.
- Color/depth attachment descriptors, tracked transitions, clear/render/end/sample flow.
- Indexed primitive and static mesh submission with model/view/projection constants.
  Normal transforms handle nonuniform scale. This is basic directional shading,
  not the engine's PBR material or scene-light implementation.
- The viewport samples rendered scene color instead of an unwritten post-process target.
- Buffer replacement, stale-handle rejection, free-slot reuse, checked texture upload
  sizes, layer/mip upload descriptions, and deferred buffer/texture/descriptor release.
- Single-frame allocator synchronization, correct command-list queue submission,
  first-use swapchain layouts, pixel-size resize detection, minimized-window skipping,
  swapchain recreation and OUT_OF_DATE recovery.
- Diagnostic texture readback and a GPU smoke test covering real pixels, all four
  primitives, ImGui fonts/images, scene/swapchain resizing, buffer replacement,
  stale handles, and shutdown. CTest fails on captured NRI/Vulkan warning/error messages.
- SDL OpenGL/OpenGL ES build options disabled. Engine source audit found no active
  `gl*` graphics calls, `GLuint`, `GLenum`, `SDL_GL_*`, or GLAD usage.

## Still required

The RHI needs independently exposed sampler/view/shader/layout/descriptor handles,
general command recording, compute, copies/resolves/mips, and complete state validation.
Shader reflection, pipeline caching, shader reload and material descriptor binding
are not implemented. Shaders currently output display-encoded basic lighting into
the scene target for the SDR compositor; HDR linear lighting and tone mapping must
replace this basic output before enabling HDR or PBR.

Material textures and maps, scene lights/PBR, skeletal GPU skinning, shadows/cascades,
sky/IBL, grid/debug lines, model previews, post-processing/bloom/TAA, runtime UI GPU
drawing, and RenderGraph are still pending. Existing compatibility/no-op code for
these features is retained until those consumers can be replaced correctly.

Synchronization is deliberately serialized around one frame allocator. Multiple
frames in flight, asynchronous queues, granular retirement fences, broader resource
lifetime validation, performance work and device-loss recovery remain pending.
Fullscreen, DPI, minimize/restore, docking, Play/Stop, scene switching, imported-model
visual checks and comparison against the previous renderer still require testing.
Bundled third-party source trees still contain their upstream OpenGL examples.

## Build and test

Requires CMake 3.30+, MSVC C++20, and a DXC build with SPIR-V support. The Windows SDK
DXC may list `-spirv` while having its code generator disabled; the configure probe
detects this. Set `VELCRYN_DXC` to a standalone compiler from
[Microsoft DXC releases](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2505.1)
or the Vulkan SDK. The compiler is a build dependency, not a shipped runtime dependency.

The local compiler used here is DXC v1.8.2505.1, archive `dxc_2025_07_14.zip`, SHA256
`9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4`.
It is installed under ignored `out/tools/dxc`.

From a Visual Studio developer shell with CMake 3.30+:

```powershell
cmake --preset x64-Debug -DVELCRYN_DXC=<path-to-dxc.exe>
cmake --build --preset x64-Debug --parallel 4
ctest --test-dir out/build/x64-Debug -R RHI.Smoke --output-on-failure
```

The GPU test opens a temporary Vulkan window and exits automatically. It writes
captured diagnostics in the build directory and a cube readback to
`out/build/rhi-cube.ppm`. It requires a GPU/driver capable of running NRI's Vulkan
backend. Debug requests NRI and Vulkan validation; a clean run only covers the
exercised paths and does not certify the unfinished migration.

## Recorded verification (2026-10-07)

- Debug and Release configure/build: passed with MSVC 14.38 and CMake 3.31.
- `RHI.Smoke`: passed in both configurations on NVIDIA GeForce RTX 4070 Ti SUPER.
- Debug run: zero captured NRI/Vulkan warning/error messages. Release also ran
  successfully, but does not request validation layers.
- Debug framebuffer readback found 55,768 cube pixels, 27,348 sphere pixels,
  9,875 plane pixels, and 40,692 cylinder pixels differing from the clear color.
- Scene targets resized from 640x480 to 320x240 and back; the native swapchain
  resized from 640x480 to 800x600 and back during the same test.
- The plane test caught and fixed triangle winding opposite to its +Y normals.
- `git diff --check`: passed.
