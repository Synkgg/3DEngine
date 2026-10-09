# OpenGL to Vulkan renderer parity audit

Comparison date: 2026-10-08. Repository: Synkgg/3DEngine.

The GitHub `velcryn-branding` branch was checked at
`d84b63aa47002135fb484656ba953228b824ab65`, matching the local HEAD. The working
changes are not committed or published. The reference is the last complete
OpenGL renderer inspected at `0d45bf0b9958aed56c4d5b86a840e96443f3c841`
("Unclump renderer by moving GLSL sources to OpenGL module"). The comparison
covered the renderer, shaders, camera, framebuffers, environment, mesh/model
submission, previews, debug drawing, textures and UI consumers in Engine/Graphics.

## What caused the dull appearance

The Vulkan cutover retained API entry points but left shadows, environment
lighting, skinning, previews and post-processing inactive. Metals therefore
lost their reflected environment. Texture samples also received an sRGB decode
that the OpenGL shader deliberately avoided for the existing authored palette.
Imported material roughness superseded scene settings, and animated models
ignored scene material overrides entirely. PracticeRange's directional-light
record was missing required fields; its light could not be read reliably.

These causes are addressed in the current working changes. Assets keep their
existing authored working space; this is compatibility with the old renderer,
not a conversion of the asset library to a new linear/sRGB authoring workflow.
The PracticeRange light now has a complete record and required light fields
are validated before optional fields are parsed.

## Feature comparison

| Area | OpenGL reference | Current Vulkan implementation | Verification |
|---|---|---|---|
| Scene geometry | Indexed primitives, imported sections, two-sided scene geometry | Same; correct normal transform and Vulkan depth conversion | Four primitive readbacks; actual PracticeRange |
| Materials | GGX, scalar PBR, base/normal/metal/roughness/AO/emissive maps, glTF combined MR | Connected through per-draw descriptors; authored scalar/map overrides take precedence | Direct/local-light and individual map pixel comparisons; weapon roughness override |
| Color and textures | Authored UNORM working values, generated mip chain | Raw views preserve working values; CPU box-filtered full mip chain including odd sizes | Known-color UI/mesh readbacks; real assets |
| Environment | Procedural radiance cube plus hemisphere-convolved irradiance | Uploaded RGBA16F cubes, radiance mip chain, roughness-dependent reflections | Metal surface changes with environment disabled |
| Lighting | Directional, eight point, four spot, local bounce | Restored original attenuation and environment contributions | Directional color/intensity, point and spot pixel checks |
| Shadows | Three cascades, front-face culling, PCF and cascade blending | Depth-only passes, three sampled depth targets, PCF/bias/blending | Occluder changes receiver pixels; real scene |
| Sky and fog | Procedural sun/sky, distance and height fog | HLSL ports with top-left screen coordinates | Actual sky rendering; distant fog pixel comparison |
| HDR | Floating-point scene color and normal/roughness MRT | RGBA16F MRT, independent HDR post chain | Normal and roughness debug readbacks |
| Bloom and grading | Extract, six blur passes, ACES, saturation, contrast, vignette, gamma | Restored HLSL passes | Emissive/bloom pixel comparison; scene capture |
| Screen-space effects | AO, GI, SSR, aerial scattering | Ported baseline shaders; corrected projection convention and actual camera near/far values | Executed in real scene; individual numerical parity not established |
| AA | MSAA color/normal/depth resolve plus temporal history | Actual multisampled attachments and dynamic-rendering resolves; camera reprojection/history clamp | 2x, 4x, 8x exercised; repeated frames and resize |
| Skinning | Up to 128 bone matrices, four weights per vertex | Extended GPU vertex stream; same CPU asset layout; vertex-stage bone palette | RiggedSimple animation changes rendered geometry |
| Previews | Static and animated model thumbnails | Cached color/depth targets queued into the next frame; 64-bit descriptor IDs | Static/animated preview commands and valid descriptors |
| Debug/editor | Grid, axes, light arrows, collider boxes, timed lines | Real line pipeline and grid; orientation HUD retained | PracticeRange grid/collider/line rendering |
| Runtime UI | Panels, images, glyphs and interactive controls | Scene-preserving overlay after tone mapping; hover/click/focus/capture fixes retained | Real DuelFPS menu hover readback and input regressions |

## Evidence and limits

`RHI.Smoke` builds and runs through CTest. It performs GPU readback rather than
only checking that draw calls succeed. The Debug run on RTX 4070 Ti SUPER captures
no NRI/Vulkan warnings or errors. Its source is
`Engine/Graphics/RHI/RHISmokeTest.cpp`; UI interaction coverage is in
`Engine/UI/UIInteractionTests.cpp`.

Readbacks in ignored `out/build` include `rhi-cube.ppm`, `rhi-ui.ppm`,
`rhi-parity-lit.ppm`, `rhi-parity-weapon.ppm`, and `rhi-practice-range.ppm`.
The PracticeRange image uses the scene's real hierarchy, materials, gun assets
and repaired light. It was inspected visually. It is not a screenshot of an
interactive multiplayer session.

The major previously inactive OpenGL rendering features now have Vulkan
implementations. This does **not** establish pixel-identical output, equal frame
times, or reliability on every GPU. Remaining acceptance work is synchronized
OpenGL/Vulkan image comparison, long interactive Play/Stop and scene-switching
runs, high-resolution performance profiling, and additional hardware coverage.
Screen-space reflections and temporal reprojection retain the baseline's
limitations, including no object-motion vectors.

## Separate migration work still open

The original 30-phase checklist also describes architecture beyond restoring
the old renderer: a general RenderGraph, independent shader/layout/sampler
handles, reflection, pipeline caching/hot reload, compute/async scheduling,
multiple frames in flight, comprehensive resource validation and device-loss
recovery. Those are not completed by this parity restoration. Submission still
uses a single fenced frame allocator. Draw resources are bounded by a 4096-set
pool and a 64 MiB uniform arena.

Arbitrary nested UI clipping and a full Unicode font atlas remain outside the
implemented UI coverage. MSAA depth MIN resolve and sample counts have been
verified on the local NVIDIA device; portable capability fallback needs wider
hardware validation. Unused legacy Shader/VertexArray compatibility wrappers
still exist, but the active renderer does not use them.

## Run it

From the repository root in PowerShell:

```powershell
& .\out\build\x64-Debug\VelcrynEditor.exe --rhi-smoke-test
$LASTEXITCODE
```

Zero means success. Prefer CTest to also fail on captured validation warnings:

```powershell
ctest --test-dir out/build/x64-Debug -R '^RHI.Smoke$' --output-on-failure
```

Use CMake 3.30+ from the Visual Studio developer environment. Build with
`cmake --build --preset x64-Debug --parallel 4`; use `x64-Release` for the optimized
build. The smoke test opens a temporary Vulkan window and exits on completion.
