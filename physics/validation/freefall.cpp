#define _USE_MATH_DEFINES
#include "freefall.h"

#include "../export/benchmark_io.h"
#include "../simulation/SimulationRequest.h"
#include "../validation/observables/KerrObservables.h"
#include "../validation/observables/SchwarzschildObservables.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void benchmark_freefall(double rs, double r0, double dt, int max_steps) {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Schwarzschild;
    config.scenario = Simulation::Scenario::RadialFreefall;
    config.geodesic = Simulation::GeodesicKind::Timelike;
    config.dt = dt;
    config.max_steps = max_steps;
    config.horizon_safety_factor = 1.0;
    config.name = "freefall";

    Spacetime::SchwarzschildParameters metric;
    metric.mass = rs;

    Simulation::RadialFreefallInitialConditions initial;
    initial.r0 = r0;

    const Simulation::SimulationResult result =
        Simulation::run_simulation(config, metric, initial);
    const std::vector<State>& history = result.history;

    const double f = Physics::Observables::schwarzschild_f(r0, rs);
    const double vt = 1.0 / f;
    const double vr = -std::sqrt(rs / r0);
    const double tau_analytical = Physics::Observables::analytical_freefall_time(r0, rs);

    {
        const double norm = Physics::Observables::timelike_norm(history.front(), rs);
        const double E = Physics::Observables::conserved_energy(history.front(), rs);
        std::cout << "\n=== Free-Fall Benchmark ===\n";
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "r0                       = " << r0 << "\n";
        std::cout << "rs                       = " << rs << "\n";
        std::cout << "dt                       = " << dt << "\n";
        std::cout << "vt                       = " << vt << "\n";
        std::cout << "vr                       = " << vr << "\n";
        std::cout << "norm     (should be -1)  = " << norm << "\n";
        std::cout << "E        (should be  1)  = " << E << "\n";
        std::cout << "tau_analytical           = " << tau_analytical << "\n\n";
    }

    std::ofstream csv = open_benchmark_csv("freefall.csv");
    if (!csv.is_open()) {
        return;
    }
    csv << "tau,r,vt,vr\n";

    std::cout << std::setw(10) << "step" << std::setw(14) << "tau" << std::setw(12) << "r"
              << std::setw(14) << "vt" << std::setw(14) << "vr" << "\n";
    std::cout << std::string(64, '-') << "\n";

    for (size_t step = 0; step < history.size(); ++step) {
        const State& state = history[step];
        const double tau = step * dt;
        const double r_ = state.X[1];
        const double vt_ = state.U[0];
        const double vr_ = state.U[1];

        csv << std::fixed << std::setprecision(8) << tau << "," << r_ << "," << vt_ << "," << vr_
            << "\n";

        if (step % 500 == 0) {
            std::cout << std::setw(10) << step << std::setw(14) << tau << std::setw(12) << r_
                      << std::setw(14) << vt_ << std::setw(14) << vr_ << "\n";
        }

        if (std::isnan(r_)) {
            std::cout << "TERMINATED: r became NaN at step " << step << "\n";
            break;
        }

        if (r_ <= rs) {
            const double error = std::abs(tau - tau_analytical);
            const double error_percent = 100.0 * error / tau_analytical;
            std::cout << "\n--- Horizon crossed at step " << step << " ---\n";
            std::cout << "tau_numerical  = " << tau << "\n";
            std::cout << "tau_analytical = " << tau_analytical << "\n";
            std::cout << "absolute error = " << error << "\n";
            std::cout << "relative error = " << error_percent << " %\n";
            break;
        }
    }

    if (history.size() == max_steps + 1) {
        const State& last_state = history.back();
        std::cout << "TERMINATED: Reached max steps. r is currently: " << last_state.X[1] << "\n";
    }

    csv.close();
    std::cout << "\nCSV written to " << (benchmark_data_dir() / "freefall.csv") << "\n";
    std::cout << "=== Free-fall complete ===\n";
}

void benchmark_freefall(const Spacetime::KerrParameters& metric, double r0, double dt,
                        int max_steps) {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Kerr;
    config.scenario = Simulation::Scenario::RadialFreefall;
    config.geodesic = Simulation::GeodesicKind::Timelike;
    config.dt = dt;
    config.max_steps = max_steps;
    config.horizon_safety_factor = 1.0;
    config.name = "freefall_kerr";

    Simulation::RadialFreefallInitialConditions initial;
    initial.r0 = r0;

    const Simulation::SimulationResult result =
        Simulation::run_simulation(config, metric, initial);
    const std::vector<State>& history = result.history;
    const double horizon = Physics::Observables::outer_horizon_radius(metric);

    {
        const double norm = Physics::Observables::timelike_norm(history.front(), metric);
        const double E = Physics::Observables::conserved_energy(history.front(), metric);
        std::cout << "\n=== Free-Fall Benchmark (Kerr) ===\n";
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "r0                       = " << r0 << "\n";
        std::cout << "rs                       = " << metric.mass << "\n";
        std::cout << "spin                     = " << metric.spin << "\n";
        std::cout << "r_+                      = " << horizon << "\n";
        std::cout << "dt                       = " << dt << "\n";
        std::cout << "vt                       = " << history.front().U[0] << "\n";
        std::cout << "vr                       = " << history.front().U[1] << "\n";
        std::cout << "norm     (should be -1)  = " << norm << "\n";
        std::cout << "E        (conserved)     = " << E << "\n\n";
    }

    std::ofstream csv = open_benchmark_csv("freefall_kerr.csv");
    if (!csv.is_open()) {
        return;
    }
    csv << "tau,r,vt,vr\n";

    std::cout << std::setw(10) << "step" << std::setw(14) << "tau" << std::setw(12) << "r"
              << std::setw(14) << "vt" << std::setw(14) << "vr" << "\n";
    std::cout << std::string(64, '-') << "\n";

    for (size_t step = 0; step < history.size(); ++step) {
        const State& state = history[step];
        const double tau = step * dt;
        const double r_ = state.X[1];
        const double vt_ = state.U[0];
        const double vr_ = state.U[1];

        csv << std::fixed << std::setprecision(8) << tau << "," << r_ << "," << vt_ << "," << vr_
            << "\n";

        if (step % 500 == 0) {
            std::cout << std::setw(10) << step << std::setw(14) << tau << std::setw(12) << r_
                      << std::setw(14) << vt_ << std::setw(14) << vr_ << "\n";
        }

        if (std::isnan(r_)) {
            std::cout << "TERMINATED: r became NaN at step " << step << "\n";
            break;
        }

        if (r_ <= horizon) {
            std::cout << "\n--- Outer horizon crossed at step " << step << " ---\n";
            std::cout << "tau_numerical  = " << tau << "\n";
            break;
        }
    }

    if (history.size() == max_steps + 1) {
        const State& last_state = history.back();
        std::cout << "TERMINATED: Reached max steps. r is currently: " << last_state.X[1] << "\n";
    }

    csv.close();
    std::cout << "\nCSV written to " << (benchmark_data_dir() / "freefall_kerr.csv") << "\n";
    std::cout << "=== Free-fall (Kerr) complete ===\n";
}
