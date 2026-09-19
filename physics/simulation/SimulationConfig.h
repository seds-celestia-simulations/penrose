#pragma once

#include "initial_conditions/InitialConditions.h"

#include <metrics/KerrParameters.h>
#include <metrics/SchwarzschildParameters.h>
#include <spacetime/MetricKind.h>

#include <state/GeodesicState.h>

#include <string>
#include <vector>

namespace Simulation {

using SpacetimeKind = Spacetime::MetricKind;

enum class Scenario {
    BoundOrbit,
    RadialFreefall,
    NullScatter,
    Custom,
};

enum class GeodesicKind {
    Timelike,
    Null,
};

// Scientific solver options owned by the physics framework (not benchmark-only hooks).
struct SolverOptions {
    bool null_constraint_projection = false;
    int null_projection_interval = 1000;
};

// Layer 1 — physics settings / orchestration only.
struct SimulationConfig {
    SpacetimeKind spacetime = SpacetimeKind::Schwarzschild;
    Scenario scenario = Scenario::BoundOrbit;
    GeodesicKind geodesic = GeodesicKind::Timelike;

    double dt = 0.01;
    int max_steps = 100000;
    double horizon_safety_factor = 1.0;

    SolverOptions solver{};

    std::string name = "trajectory";
};

// Metric-derived metadata carried to consumers without visualization coupling.
struct SimulationMetadata {
    Spacetime::MetricKind metric = Spacetime::MetricKind::Schwarzschild;
    Spacetime::CoordinateChartKind coordinate_chart =
        Spacetime::CoordinateChartKind::SchwarzschildSpherical;
    double horizon_radius = 1.0;
    double photon_sphere_radius = 1.5;
};

// Physics → Trajectory Storage boundary.
struct SimulationResult {
    std::vector<State> history;
    double characteristic_radius = 1.0;
    std::string name;
    SpacetimeKind spacetime = SpacetimeKind::Schwarzschild;
    SimulationMetadata metadata{};
};

using PhysicsTrajectory = SimulationResult;

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const BoundOrbitInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const RadialFreefallInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const NullScatterInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const CustomInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const BoundOrbitInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const RadialFreefallInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const NullScatterInitialConditions& initial);

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const CustomInitialConditions& initial);

} // namespace Simulation
