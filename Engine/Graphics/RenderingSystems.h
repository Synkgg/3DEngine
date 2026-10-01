#pragma once

// Rendering subsystem ownership map.
//
// Renderer remains the public facade while the implementation is migrated into
// focused systems. Keeping these boundaries explicit prevents lighting,
// shadows, post effects and environment rendering from growing back into one
// monolithic implementation.
//
// LightingSystem: direct PBR lights and indirect-light policy.
// EnvironmentSystem: sky/environment sampling and future HDR cubemap IBL.
// ShadowSystem: directional cascades/local-light shadow resources.
// ReflectionSystem: screen-space reflections and future reflection probes.
// PostProcessSystem: AO, bloom, atmosphere, grading and temporal AA.
//
// The first extraction keeps shader/resource lifetime in Renderer so existing
// scenes and editor code remain binary/source compatible.
namespace Rendering
{
    struct LightingSystem final {};
    struct EnvironmentSystem final {};
    struct ShadowSystem final {};
    struct ReflectionSystem final {};
    struct PostProcessSystem final {};
}
