#include "BenchmarkRunner.h"

#include "freefall.h"
#include "null_geodesic.h"
#include "orbital.h"

#include "../validation/observables/KerrObservables.h"
#include "../validation/observables/SchwarzschildObservables.h"

#include <cmath>
#include <iostream>
#include <variant>

namespace Simulation {
namespace {

void run_null_at_b(double rs, double r0, double b, double dt, int max_steps) {
    const double f = 1.0 - rs / r0;
    const double E = 1.0;
    const double L = b * E;
    const double vph = L / (r0 * r0);
    const double vr = -std::sqrt(E * E - f * (L * L / (r0 * r0)));

    std::cout << "\n=== Running b = " << b << " (dt = " << dt << ") ===\n";
    benchmark_null_geodesic(rs, r0, vr, vph, dt, max_steps);
}

void run_schwarzschild(const BenchmarkConfig& config) {
    const auto* metric = std::get_if<Spacetime::SchwarzschildParameters>(&config.metric);
    if (!metric) {
        std::cerr << "run_physics_benchmarks: Schwarzschild spacetime requires "
                     "SchwarzschildParameters\n";
        return;
    }

    const double rs = metric->mass;

    if (config.run_freefall) {
        benchmark_freefall(rs, config.freefall.r0, config.freefall.dt, config.freefall.max_steps);
    }

    if (config.run_orbital) {
        benchmark_orbital(rs, config.orbital.r0, config.orbital.vr, config.orbital.vphi,
                          config.orbital.dt, config.orbital.max_steps);
    }

    if (config.run_null_suite) {
        const double b_crit = Physics::Observables::critical_impact_parameter(rs);
        run_null_at_b(rs, config.null_suite.r0, b_crit + config.null_suite.escape_offset,
                      config.null_suite.dt, config.null_suite.max_steps);
        run_null_at_b(rs, config.null_suite.r0, b_crit + config.null_suite.capture_offset,
                      config.null_suite.dt, config.null_suite.max_steps);
        run_null_at_b(rs, config.null_suite.r0, b_crit + config.null_suite.near_critical_offset,
                      config.null_suite.dt, config.null_suite.max_steps);
    }
}

void run_kerr(const BenchmarkConfig& config) {
    const auto* metric = std::get_if<Spacetime::KerrParameters>(&config.metric);
    if (!metric) {
        std::cerr << "run_physics_benchmarks: Kerr spacetime requires KerrParameters\n";
        return;
    }

    if (config.run_freefall) {
        benchmark_freefall(*metric, config.freefall.r0, config.freefall.dt,
                           config.freefall.max_steps);
    }

    if (config.run_orbital) {
        benchmark_orbital(*metric, config.orbital.r0, config.orbital.vr, config.orbital.vphi,
                          config.orbital.dt, config.orbital.max_steps);
    }

    if (config.run_null_suite) {
        const double b_crit = Physics::Observables::critical_impact_parameter(*metric);
        const double bs[] = {
            b_crit + config.null_suite.escape_offset,
            b_crit + config.null_suite.capture_offset,
            b_crit + config.null_suite.near_critical_offset,
        };
        for (double b : bs) {
            std::cout << "\n=== Running Kerr b = " << b << " (dt = " << config.null_suite.dt
                      << ") ===\n";
            benchmark_null_geodesic(*metric, config.null_suite.r0, b, config.null_suite.dt,
                                    config.null_suite.max_steps);
        }
    }
}

} // namespace

int run_physics_benchmarks(const BenchmarkConfig& config) {
    if (config.spacetime == SpacetimeKind::Schwarzschild) {
        run_schwarzschild(config);
        return 0;
    }
    if (config.spacetime == SpacetimeKind::Kerr) {
        run_kerr(config);
        return 0;
    }

    std::cerr << "run_physics_benchmarks: unsupported spacetime\n";
    return 1;
}

} // namespace Simulation
