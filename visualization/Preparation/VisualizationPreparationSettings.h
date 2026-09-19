#pragma once

namespace viz {

// Visualization-only resolution / prep knobs.
// Independent of physics dt / integrator / max_steps.
// Algorithms (splines, adaptive resampling, LOD) are reserved — pass-through today.
enum class InterpolationMethod {
    PassThrough,
    // Future: Linear, CubicSpline, ...
};

struct VisualizationPreparationSettings {
    InterpolationMethod interpolation_method = InterpolationMethod::PassThrough;

    // Reserved for future resampling / curve reconstruction.
    int render_samples_per_segment = 1;
    float trajectory_resolution = 1.0f;
};

} // namespace viz
