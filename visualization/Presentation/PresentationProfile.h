#pragma once

namespace viz {

// Non-physical presentation tuning applied after the deterministic CPU rasterizer.
// Production path: set every field from VisualizationConfig in main.cpp.
// Default-constructed profiles are inert (enabled == false).
struct PresentationProfile {
    bool enabled = false;

    float bloom_threshold = 0.55f;
    float bloom_intensity = 0.85f;
    int bloom_radius = 3;

    // Screen-space warp scaled by projected horizon radius.
    float lensing_strength = 0.07f;
    float lensing_radius_scale = 1.85f;

    float contrast = 1.12f;
    float saturation = 1.15f;
    float vignette = 0.22f;

    float halo_radius_scale = 1.12f;
    float halo_strength = 0.0f;

    // Thin warm ring just outside the central shadow.
    float lens_ring_strength = 0.62f;
    float lens_ring_radius_scale = 1.045f;
    float lens_ring_width_scale = 0.032f;

    // Named presets for tests / ad-hoc scripts only — not used by viewer/export mains.
    static PresentationProfile cinematic_default();
    static PresentationProfile interactive_default();
};

} // namespace viz
