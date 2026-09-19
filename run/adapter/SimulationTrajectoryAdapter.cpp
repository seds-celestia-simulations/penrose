#include "SimulationTrajectoryAdapter.h"

#include "Preparation/VisualizationPreparation.h"

namespace viz {

std::vector<StoredTrajectory> store_trajectories(std::span<const Simulation::SimulationResult> results,
                                                 std::span<const TrajectoryStyle> styles) {
    std::vector<StoredTrajectory> stored;
    stored.reserve(results.size());
    for (std::size_t i = 0; i < results.size(); ++i) {
        StoredTrajectory trajectory;
        trajectory.history = results[i].history;
        trajectory.name = results[i].name;
        trajectory.characteristic_radius = results[i].characteristic_radius;
        trajectory.horizon_radius = results[i].metadata.horizon_radius;
        trajectory.coordinate_chart = results[i].metadata.coordinate_chart;
        if (i < styles.size()) {
            trajectory.use_style = true;
            trajectory.style = styles[i];
        }
        stored.push_back(std::move(trajectory));
    }
    return stored;
}

std::vector<StoredTrajectory> store_trajectories(
    const std::vector<Simulation::SimulationResult>& results, std::span<const TrajectoryStyle> styles) {
    return store_trajectories(std::span<const Simulation::SimulationResult>(results), styles);
}

Scene prepare_scene_from_results(std::span<const Simulation::SimulationResult> results,
                                 const VisualizationConfig& config,
                                 std::span<const TrajectoryStyle> styles) {
    return prepare_scene(store_trajectories(results, styles), config);
}

Scene prepare_scene_from_results(const std::vector<Simulation::SimulationResult>& results,
                                 const VisualizationConfig& config,
                                 std::span<const TrajectoryStyle> styles) {
    return prepare_scene_from_results(std::span<const Simulation::SimulationResult>(results), config,
                                      styles);
}

} // namespace viz
