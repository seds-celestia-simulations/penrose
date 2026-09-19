#pragma once

#include "../Trajectory/Trajectory.h"

#include <spacetime/MetricKind.h>
#include <state/GeodesicState.h>

#include <string>
#include <vector>

namespace viz {

// Stage 2 input — physics-agnostic trajectory storage with explicit chart metadata.
struct StoredTrajectory {
    std::vector<State> history;
    std::string name;
    double characteristic_radius = 1.0;
    double horizon_radius = 1.0;
    Spacetime::CoordinateChartKind coordinate_chart = Spacetime::CoordinateChartKind::SchwarzschildSpherical;

    bool use_style = false;
    TrajectoryStyle style{};
};

} // namespace viz
