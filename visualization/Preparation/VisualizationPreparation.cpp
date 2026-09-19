#include "VisualizationPreparation.h"

#include "../Trajectory/TrajectoryAdapter.h"

#include <algorithm>
#include <cmath>

namespace viz {
namespace {

TrajectoryStyle resolve_style(const StoredTrajectory& trajectory, const VisualizationConfig& config) {
    return trajectory.use_style ? trajectory.style : config.trajectory_style;
}

Trajectory prepare_renderable(const StoredTrajectory& stored, const VisualizationConfig& config) {
    (void)config.preparation.interpolation_method;
    (void)config.preparation.render_samples_per_segment;
    (void)config.preparation.trajectory_resolution;

    TrajectoryAdapterOptions options;
    options.name = stored.name;
    options.style = resolve_style(stored, config);
    options.coordinate_chart = stored.coordinate_chart;
    return adapt_states(stored.history, options);
}

float scene_horizon_radius(std::span<const StoredTrajectory> trajectories,
                           const VisualizationConfig& config) {
    float radius = config.scene.horizon_radius;
    for (const StoredTrajectory& trajectory : trajectories) {
        radius = std::max(radius, static_cast<float>(trajectory.horizon_radius));
        radius = std::max(radius, static_cast<float>(trajectory.characteristic_radius));
    }
    return radius;
}

} // namespace

Scene prepare_scene(std::span<const StoredTrajectory> trajectories, const VisualizationConfig& config) {
    SceneSettings settings = config.scene;
    settings.horizon_radius = scene_horizon_radius(trajectories, config);

    Scene scene(settings);
    for (const StoredTrajectory& stored : trajectories) {
        scene.add_trajectory(prepare_renderable(stored, config));
    }
    return scene;
}

Scene prepare_scene(const std::vector<StoredTrajectory>& trajectories, const VisualizationConfig& config) {
    return prepare_scene(std::span<const StoredTrajectory>(trajectories), config);
}

} // namespace viz
