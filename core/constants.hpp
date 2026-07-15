#pragma once

// Tunable engine/render constants gathered in one place (was scattered literals).
namespace smallgine {
namespace k {

    // Lighting
    constexpr int   MaxLights          = 8;    // must match MAXL in lit.frag

    // Shadow map (directional)
    constexpr int   ShadowMapSize      = 1024; // must match texel size in lit.frag
    constexpr float ShadowOrthoHalf    = 4.0f; // ortho half-extent covering the scene
    constexpr float ShadowLightDist    = 8.0f; // light position distance from origin
    constexpr float ShadowNear         = 0.1f;
    constexpr float ShadowFar          = 20.0f;

    // Skybox
    constexpr int   SkyboxFaceSize     = 64;

    // Camera projection
    constexpr float PerspectiveNear    = 0.1f;
    constexpr float PerspectiveFar     = 100.0f;

    // Post-processing
    constexpr int   BloomBlurPasses    = 6;

    // Instanced ring demo
    constexpr int   InstanceRingCount  = 48;
    constexpr float InstanceRingRadius = 3.6f;
    constexpr float InstanceRingY      = -1.35f;
    constexpr float InstanceScale      = 0.22f;

    // Editor
    constexpr float GizmoStep          = 0.15f;

} // namespace k
} // namespace smallgine
