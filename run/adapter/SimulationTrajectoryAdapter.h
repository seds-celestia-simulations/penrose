#pragma once

#include "simulation/SimulationConfig.h"

#include "Preparation/StoredTrajectory.h"
#include "Presentation/VisualizationConfig.h"
#include "Scene/Scene.h"

#include <span>
#include <vector>

namespace viz {

std::vector<StoredTrajectory> store_trajectories(
    std::span<const Simulation::SimulationResult> results,
    std::span<const TrajectoryStyle> styles = {});

std::vector<StoredTrajectory> store_trajectories(
    const std::vector<Simulation::SimulationResult>& results,
    std::span<const TrajectoryStyle> styles = {});

Scene prepare_scene_from_results(std::span<const Simulation::SimulationResult> results,
                                 const VisualizationConfig& config,
                                 std::span<const TrajectoryStyle> styles = {});

Scene prepare_scene_from_results(const std::vector<Simulation::SimulationResult>& results,
                                 const VisualizationConfig& config,
                                 std::span<const TrajectoryStyle> styles = {});

} // namespace viz
