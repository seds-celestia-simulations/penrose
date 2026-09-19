#pragma once

#include "Trajectory.h"

#include <spacetime/MetricKind.h>

#include <span>
#include <vector>

struct State;

namespace viz {

struct TrajectoryAdapterOptions {
    std::string name = "trajectory";
    TrajectoryStyle style{};
    double parameter_scale = 1.0;
    double parameter_offset = 0.0;
    bool use_state_time = true;
    Spacetime::CoordinateChartKind coordinate_chart =
        Spacetime::CoordinateChartKind::SchwarzschildSpherical;
};

// Convert chart-sampled states into a Cartesian trajectory for presentation.
Trajectory adapt_states(std::span<const State> states, const TrajectoryAdapterOptions& options = {});

Trajectory adapt_states(const std::vector<State>& states, const TrajectoryAdapterOptions& options = {});

} // namespace viz
