# Vulkan parity implementation status

The major OpenGL rendering passes have now been restored on Vulkan/NRI. See
[the source comparison and verification matrix](RendererParityAudit.md) for the
baseline commit, causes of the dull appearance, implemented features, and
remaining acceptance work. The full 30-phase RHI architecture checklist remains
unfinished; passing the GPU tests does not establish pixel-identical visuals or
performance parity.

## Current implementation

- HDR scene color and normal/roughness MRT, with a separate post-process chain.
- PBR material maps and scalar overrides, environment radiance/irradiance,
  directional/point/spot lights, three cascaded shadow maps, procedural sky and fog.
- Bloom, AO/GI/SSR/atmosphere shader ports, ACES grading and temporal resolve.
- Real 2x/4x/8x MSAA attachments and color/normal/depth resolves.
- 128-bone GPU skinning, static/animated previews, debug lines and editor grid.
- Legacy-authored texture working values and full texture mip chains.
- Runtime UI drawn after post-processing, with hover, capture, focus and text
  input fixes covered by GPU and input regression tests.
- Generational handles, deferred destruction and single-frame synchronization;
  64 MiB per-frame uniform arena and 4096 descriptor sets.

## Still required

Synchronized old/new renderer image comparisons, high-resolution performance
profiling, prolonged interactive editor/runtime testing, and additional GPU
coverage remain. General RenderGraph, compute/async APIs, reflection, pipeline
caching, multiple frames in flight and device-loss recovery are separate pending
migration work. Arbitrary nested UI clipping and a full Unicode atlas are not
implemented. See the audit for the exact limitations.

## Build and test

### Viewport navigation and CPU overhead (2026-10-08)

- Restored a depth-tested, alpha-blended editor grid with X/Z colors and distance
  fade. It renders after scene geometry without writing scene depth.
- Added a camera-axis indicator, position/look direction, frame time/FPS, and
  Viewport > Reset editor view to move above the default ground plane.
- Corrected picking rays to match the renderer's horizontal-FOV projection.
- Model dependency scans now run at most once per source every two seconds;
  explicit invalidation forces a fresh lookup. Empty shadow passes skip scene traversal.
- Debug GPU readback verified 54,025 grid pixels; picking checks cover square,
  landscape and portrait aspect ratios. This does not establish performance in
  the user's scene or diagnose the screenshot's boundary conclusively.

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

## Material and UI regression coverage (2026-10-08)

Runtime interaction coverage now includes real Duel FPS menu hover pixels,
letterboxed/offset viewport hit testing, press/release/cancel and once-only click
events, cross-control overlap, dynamic z order, disabled/hidden controls, rounded
hit regions, nested slider capture, focused widget deletion/canvas replacement,
selection persistence, SDL text entry and missing button image fallback. Duel FPS
menu/pause/match buttons now have distinct saved state colors and label feedback.
Text fields render their actual caret/selection, scroll horizontally, and clip
their text to the field; UI quads clip to the logical canvas. Arbitrary nested
widget clipping and a full Unicode font atlas remain unimplemented.

### Run the already-built smoke test

In PowerShell, from the repository root:

```powershell
Set-Location F:\GitHub\3DEngine
& .\out\build\x64-Debug\VelcrynEditor.exe --rhi-smoke-test
$LASTEXITCODE
```

Zero means success. The test opens a temporary Vulkan window, exercises renderer
and UI paths, prints its results, then exits. Use the repository root as the
working directory so it can locate the test assets. Prefer the Debug build for
validation-layer diagnostics. For automated runs that also fail on captured
validation warnings, use CTest (requires an existing configured/built tree):

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir out/build/x64-Debug -R '^RHI.Smoke$' --output-on-failure
```

CTest saves `rhi-smoke.log` and `rhi-smoke-errors.log` under that build directory.
Replace `x64-Debug` with `x64-Release` to test the Release build.

The smoke test now renders known texture colors and inspects half-float scene
readback. It verifies directional-light color/intensity, point/spot contributions,
colored mesh texels, UI alpha compositing, image orientation, preservation of a
128/255 gray image value, progress fill extent, non-square rounded buttons, and
both canvas and scripted text. It writes a UI capture to `out/build/rhi-ui.ppm`.
These checks cover GPU output; they do not certify full renderer parity or every
project's UI interactions.

Debug and Release builds and `RHI.Smoke` passed on the RTX 4070 Ti SUPER on
2026-10-08. The Debug run captured zero NRI/Vulkan warning/error messages. The UI
readback was also inspected visually for panels, image colors, rounded buttons,
progress bars, and text. `git diff --check` passed.
