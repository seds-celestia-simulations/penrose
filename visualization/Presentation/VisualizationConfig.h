#pragma once

#include "../Camera/Camera.h"
#include "../Preparation/VisualizationPreparationSettings.h"
#include "../Presentation/PresentationProfile.h"
#include "../Renderer/CPURasterizer.h"
#include "../Scene/Scene.h"
#include "../Trajectory/Trajectory.h"

#include <span>
#include <string>
#include <vector>

struct State;

namespace viz {

// All drawing / camera / presentation knobs. Set these in examples/*/main.cpp —
// the visualization module must not inject cinematic presets behind the scenes.
struct CameraConfig {
    bool auto_frame = true;
    float distance = 12.0f;
    float yaw = 0.0f;
    float pitch = 0.45f;
    float fov_y = 0.95f;
    float near_plane = 0.05f;
    float far_plane = 500.0f;

    // Used only when auto_frame == true
    float auto_frame_distance_scale = 1.2f;
    float auto_frame_min_distance = 7.0f;
    float auto_frame_max_distance = 120.0f;
    float auto_frame_pitch = 0.52f;
};

struct VisualizationConfig {
    int width = 1280;
    int height = 720;
    const char* title = "Penrose Viewer";
    float playback_speed = 1.0f;

    // Stage 2 — viz resolution (independent of physics dt).
    VisualizationPreparationSettings preparation{};

    SceneSettings scene{};
    CameraConfig camera{};
    PresentationProfile presentation{};
    TrajectoryStyle trajectory_style{}; // default when a particle has use_style == false
    RenderOptions render{};

    // Export only
    int frame_count = 1;
    std::string still_basename = "frame.ppm";
};

Camera make_camera(const Scene& scene, const CameraConfig& config);
void apply_camera_config(Camera& camera, const Scene& scene, const CameraConfig& config);

// Legacy single-trajectory helper — forwards to prepare_scene (pass-through).
Scene build_scene(std::span<const State> history, const std::string& name,
                  const VisualizationConfig& config);
Scene build_scene(const std::vector<State>& history, const std::string& name,
                  const VisualizationConfig& config);

} // namespace viz
