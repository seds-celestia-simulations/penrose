#include "VisualizationConfig.h"

#include "../Preparation/VisualizationPreparation.h"
#include <state/GeodesicState.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace viz {

Scene build_scene(std::span<const State> history, const std::string& name,
                  const VisualizationConfig& config) {
    StoredTrajectory trajectory;
    trajectory.history.assign(history.begin(), history.end());
    trajectory.name = name;
    trajectory.characteristic_radius = config.scene.horizon_radius;
    return prepare_scene(std::span<const StoredTrajectory>(&trajectory, 1), config);
}

Scene build_scene(const std::vector<State>& history, const std::string& name,
                  const VisualizationConfig& config) {
    return build_scene(std::span<const State>(history), name, config);
}

void apply_camera_config(Camera& camera, const Scene& scene, const CameraConfig& config) {
    camera.set_target(Vec3(0.0f, 0.0f, 0.0f));
    camera.set_fov_y(config.fov_y);
    camera.set_clipping(config.near_plane, config.far_plane);

    if (!config.auto_frame || scene.trajectories().empty()) {
        camera.set_distance(config.distance);
        camera.set_angles(config.yaw, config.pitch);
        return;
    }

    float max_radius = 1.0f;
    Vec3 dominant_sample{0.0f, 0.0f, 0.0f};
    for (const Trajectory& trajectory : scene.trajectories()) {
        for (const TrajectorySample& sample : trajectory.samples()) {
            const float radius = sample.position.length();
            if (radius >= max_radius) {
                max_radius = radius;
                dominant_sample = sample.position;
            }
        }
    }

    camera.set_distance(std::clamp(max_radius * config.auto_frame_distance_scale,
                                   config.auto_frame_min_distance, config.auto_frame_max_distance));

    // Keep the configured yaw. Re-aiming from the farthest sample often places the
    // camera in the equatorial (xy) orbital plane, collapsing orbits to a line.
    // Pitch provides the tilted 3/4 view; a small yaw offset avoids a dead-on look.
    float yaw = config.yaw;
    if (std::abs(yaw) < 1e-4f && dominant_sample.length_squared() > 1e-8f) {
        yaw = 0.65f;
    }
    camera.set_angles(yaw, config.auto_frame_pitch);
}

Camera make_camera(const Scene& scene, const CameraConfig& config) {
    Camera camera;
    apply_camera_config(camera, scene, config);
    return camera;
}

} // namespace viz
