/**
 * Physics benchmark entry point.
 *
 *   cmake --build build --target physics_benchmark
 *   ./build/physics_benchmark
 *
 * Swap spacetime by setting config.spacetime and config.metric.
 */

#include "validation/BenchmarkRunner.h"

int main() {
    // =========================================================================
    // CONFIGURATION — Schwarzschild (default) or Kerr
    // =========================================================================

    Simulation::BenchmarkConfig config;

    // --- Schwarzschild ---
    // config.spacetime = Simulation::SpacetimeKind::Schwarzschild;
    // config.metric = Spacetime::SchwarzschildParameters{.mass = 1.0};
    // config.orbital.vphi = 0.06;

    // --- Kerr ---
    config.spacetime = Simulation::SpacetimeKind::Kerr;
    config.metric = Spacetime::KerrParameters{.mass = 1.0, .spin = 0.35};
    config.orbital.vphi = 0.055;

    config.run_freefall = true;
    config.freefall.r0 = 10.0;
    config.freefall.dt = 0.001;

    config.run_orbital = true;
    config.orbital.r0 = 6.0;
    config.orbital.vr = 0.0;
    config.orbital.dt = 0.01;
    config.orbital.max_steps = 100000;

    config.run_null_suite = true;
    config.null_suite.r0 = 10.0;
    config.null_suite.dt = 0.0005;
    config.null_suite.max_steps = 200000;
    config.null_suite.escape_offset = 1e-3;
    config.null_suite.capture_offset = -1e-3;
    config.null_suite.near_critical_offset = 1e-5;

    // =========================================================================

    return Simulation::run_physics_benchmarks(config);
}
