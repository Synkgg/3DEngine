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
