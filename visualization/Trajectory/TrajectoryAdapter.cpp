#include "TrajectoryAdapter.h"

#include "../Geometry/Coordinates.h"
#include <state/GeodesicState.h>

#include <stdexcept>

namespace viz {

Trajectory adapt_states(std::span<const State> states, const TrajectoryAdapterOptions& options) {
    if (options.coordinate_chart != Spacetime::CoordinateChartKind::SchwarzschildSpherical &&
        options.coordinate_chart != Spacetime::CoordinateChartKind::KerrBoyerLindquist) {
        throw std::runtime_error("TrajectoryAdapter: unsupported coordinate chart");
    }

    std::vector<TrajectorySample> samples;
    samples.reserve(states.size());

    for (std::size_t i = 0; i < states.size(); ++i) {
        const State& state = states[i];
        // Schwarzschild spherical / Kerr BL: X = (t, r, theta, phi)
        const double r = state.X[1];
        const double theta = state.X[2];
        const double phi = state.X[3];

        TrajectorySample sample;
        if (options.use_state_time) {
            sample.parameter = state.X[0];
        } else {
            sample.parameter = options.parameter_offset + static_cast<double>(i) * options.parameter_scale;
        }
        sample.position = spherical_to_cartesian(r, theta, phi);
        samples.push_back(sample);
    }

    decimate_trajectory_samples(samples, 3000);

    return Trajectory(options.name, std::move(samples), options.style, false);
}

Trajectory adapt_states(const std::vector<State>& states, const TrajectoryAdapterOptions& options) {
    return adapt_states(std::span<const State>(states), options);
}

} // namespace viz
