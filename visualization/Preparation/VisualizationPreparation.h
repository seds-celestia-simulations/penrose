#pragma once

#include "StoredTrajectory.h"
#include "../Presentation/VisualizationConfig.h"
#include "../Scene/Scene.h"

#include <span>
#include <vector>

namespace viz {

Scene prepare_scene(std::span<const StoredTrajectory> trajectories, const VisualizationConfig& config);

Scene prepare_scene(const std::vector<StoredTrajectory>& trajectories, const VisualizationConfig& config);

} // namespace viz
