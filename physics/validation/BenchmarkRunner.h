#pragma once

#include "simulation/SimulationConfig.h"
#include "simulation/SimulationRequest.h"

namespace Simulation {

// Benchmark suite orchestration. Case-specific numbers live in nested structs
// (and MetricParameters), not flattened onto SimulationConfig.
struct BenchmarkConfig {
    SpacetimeKind spacetime = SpacetimeKind::Schwarzschild;
    MetricParameters metric = Spacetime::SchwarzschildParameters{};

    bool run_freefall = true;
    bool run_orbital = true;
    bool run_null_suite = true;

    struct FreefallCase {
        double r0 = 10.0;
        double dt = 0.001;
        int max_steps = 100000;
    } freefall;

    struct OrbitalCase {
        double r0 = 6.0;
        double vr = 0.0;
        double vphi = 0.06;
        double dt = 0.01;
        int max_steps = 100000;
    } orbital;

    struct NullSuiteCase {
        double r0 = 10.0;
        double dt = 0.0005;
        int max_steps = 200000;
        double escape_offset = 1e-3;
        double capture_offset = -1e-3;
        double near_critical_offset = 1e-5;
    } null_suite;
};

int run_physics_benchmarks(const BenchmarkConfig& config);

} // namespace Simulation
