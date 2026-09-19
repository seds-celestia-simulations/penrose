#pragma once

#include "../Camera/Camera.h"
#include "../Scene/Scene.h"

namespace viz {

// Convenience helpers for tests / ad-hoc scripts.
// Production entry points must set VisualizationConfig in examples/*/main.cpp instead.

SceneSettings cinematic_scene_settings(float horizon_radius = 1.0f);
void apply_cinematic_camera(Camera& camera);
void frame_camera_on_trajectories(Camera& camera, const Scene& scene);

} // namespace viz
