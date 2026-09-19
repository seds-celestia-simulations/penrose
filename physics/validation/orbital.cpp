#define _USE_MATH_DEFINES
#include "orbital.h"

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

void benchmark_orbital(double rs, double r0, double vr, double vph, double dt, int max_steps) {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Schwarzschild;
    config.scenario = Simulation::Scenario::BoundOrbit;
    config.geodesic = Simulation::GeodesicKind::Timelike;
    config.dt = dt;
    config.max_steps = max_steps;
    config.horizon_safety_factor = 1.0;
    config.name = "orbital";

    Spacetime::SchwarzschildParameters metric;
    metric.mass = rs;

    Simulation::BoundOrbitInitialConditions initial;
    initial.r0 = r0;
    initial.vr = vr;
    initial.vphi = vph;

    const Simulation::SimulationResult result =
        Simulation::run_simulation(config, metric, initial);
    const std::vector<State>& history = result.history;
    const State& first = history.front();

    {
        const double norm = Physics::Observables::timelike_norm(first, rs);
        const double E = Physics::Observables::conserved_energy(first, rs);
        const double L = Physics::Observables::conserved_angular_momentum(first);

        std::cout << "\n=== Orbital Benchmark ===\n";
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "r0               = " << r0 << "\n";
        std::cout << "rs               = " << rs << "\n";
        std::cout << "dt               = " << dt << "\n";
        std::cout << "vr               = " << vr << "\n";
        std::cout << "vph              = " << vph << "\n";
        std::cout << "vt (computed)    = " << first.U[0] << "\n";
        std::cout << "max_steps        = " << max_steps << "\n";
        std::cout << "norm (should -1) = " << norm << "\n";
        std::cout << "E  (conserved)   = " << E << "\n";
        std::cout << "L  (conserved)   = " << L << "\n\n";
    }

    std::ofstream csv = open_benchmark_csv("orbital.csv");
    if (!csv.is_open()) {
        return;
    }
    csv << "tau,r,phi,vt,vr,vph,norm\n";

    std::cout << std::setw(10) << "step" << std::setw(12) << "tau" << std::setw(12) << "r"
              << std::setw(12) << "phi" << std::setw(12) << "vr" << std::setw(14) << "norm"
              << "\n";
    std::cout << std::string(72, '-') << "\n";

    for (size_t step = 0; step < history.size(); ++step) {
        const State& state = history[step];
        const double tau = step * dt;
        const double r_ = state.X[1];
        const double phi_ = state.X[3];
        const double vt_ = state.U[0];
        const double vr_ = state.U[1];
        const double vph_ = state.U[3];

        double norm = 0.0;
        if (r_ > rs) {
            norm = Physics::Observables::timelike_norm(state, rs);
        }

        csv << std::fixed << std::setprecision(8) << tau << "," << r_ << "," << phi_ << "," << vt_
            << "," << vr_ << "," << vph_ << "," << norm << "\n";

        if (step % 1000 == 0) {
            std::cout << std::setw(10) << step << std::setw(12) << tau << std::setw(12) << r_
                      << std::setw(12) << phi_ << std::setw(12) << vr_ << std::setw(14) << norm
                      << "\n";
        }

        if (r_ <= rs) {
            std::cout << "\n[Horizon crossed] step = " << step << "  tau = " << tau << "\n";
        }
        if (r_ > 1000.0) {
            std::cout << "\n[Escaped to infinity] step = " << step << "  tau = " << tau << "\n";
            break;
        }
    }

    if (history.size() - 1 == static_cast<size_t>(max_steps)) {
        std::cout << "\n[Max steps reached] tau = " << (history.size() - 1) * dt << "\n";
    }

    csv.close();
    std::cout << "\nCSV written to " << (benchmark_data_dir() / "orbital.csv") << "\n";
    std::cout << "=== Orbital complete ===\n";
}

void benchmark_orbital(const Spacetime::KerrParameters& metric, double r0, double vr, double vph,
                       double dt, int max_steps) {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Kerr;
    config.scenario = Simulation::Scenario::BoundOrbit;
    config.geodesic = Simulation::GeodesicKind::Timelike;
    config.dt = dt;
    config.max_steps = max_steps;
    config.horizon_safety_factor = 1.0;
    config.name = "orbital_kerr";

    Simulation::BoundOrbitInitialConditions initial;
    initial.r0 = r0;
    initial.vr = vr;
    initial.vphi = vph;

    const Simulation::SimulationResult result =
        Simulation::run_simulation(config, metric, initial);
    const std::vector<State>& history = result.history;
    const State& first = history.front();
    const double horizon = Physics::Observables::outer_horizon_radius(metric);

    {
        const double norm = Physics::Observables::timelike_norm(first, metric);
        const double E = Physics::Observables::conserved_energy(first, metric);
        const double L = Physics::Observables::conserved_angular_momentum(first, metric);

        std::cout << "\n=== Orbital Benchmark (Kerr) ===\n";
        std::cout << std::fixed << std::setprecision(8);
        std::cout << "r0               = " << r0 << "\n";
        std::cout << "rs               = " << metric.mass << "\n";
        std::cout << "spin             = " << metric.spin << "\n";
        std::cout << "r_+              = " << horizon << "\n";
        std::cout << "dt               = " << dt << "\n";
        std::cout << "vr               = " << vr << "\n";
        std::cout << "vph              = " << vph << "\n";
        std::cout << "vt (computed)    = " << first.U[0] << "\n";
        std::cout << "max_steps        = " << max_steps << "\n";
        std::cout << "norm (should -1) = " << norm << "\n";
        std::cout << "E  (conserved)   = " << E << "\n";
        std::cout << "L  (conserved)   = " << L << "\n\n";
    }

    std::ofstream csv = open_benchmark_csv("orbital_kerr.csv");
    if (!csv.is_open()) {
        return;
    }
    csv << "tau,r,phi,vt,vr,vph,norm\n";

    std::cout << std::setw(10) << "step" << std::setw(12) << "tau" << std::setw(12) << "r"
              << std::setw(12) << "phi" << std::setw(12) << "vr" << std::setw(14) << "norm"
              << "\n";
    std::cout << std::string(72, '-') << "\n";

    for (size_t step = 0; step < history.size(); ++step) {
        const State& state = history[step];
        const double tau = step * dt;
        const double r_ = state.X[1];
        const double phi_ = state.X[3];
        const double vt_ = state.U[0];
        const double vr_ = state.U[1];
        const double vph_ = state.U[3];

        double norm = 0.0;
        if (r_ > horizon) {
            norm = Physics::Observables::timelike_norm(state, metric);
        }

        csv << std::fixed << std::setprecision(8) << tau << "," << r_ << "," << phi_ << "," << vt_
            << "," << vr_ << "," << vph_ << "," << norm << "\n";

        if (step % 1000 == 0) {
            std::cout << std::setw(10) << step << std::setw(12) << tau << std::setw(12) << r_
                      << std::setw(12) << phi_ << std::setw(12) << vr_ << std::setw(14) << norm
                      << "\n";
        }

        if (r_ <= horizon) {
            std::cout << "\n[Horizon crossed] step = " << step << "  tau = " << tau << "\n";
        }
        if (r_ > 1000.0) {
            std::cout << "\n[Escaped to infinity] step = " << step << "  tau = " << tau << "\n";
            break;
        }
    }

    if (history.size() - 1 == static_cast<size_t>(max_steps)) {
        std::cout << "\n[Max steps reached] tau = " << (history.size() - 1) * dt << "\n";
    }

    csv.close();
    std::cout << "\nCSV written to " << (benchmark_data_dir() / "orbital_kerr.csv") << "\n";
    std::cout << "=== Orbital (Kerr) complete ===\n";
}
