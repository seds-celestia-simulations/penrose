#define _USE_MATH_DEFINES
#include "null_geodesic.h"

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

void benchmark_null_geodesic(double rs, double r0, double vr, double vph, double dt, int max_steps,
                             double horizon_safety_factor) {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Schwarzschild;
    config.scenario = Simulation::Scenario::Custom;
    config.geodesic = Simulation::GeodesicKind::Null;
    config.dt = dt;
    config.max_steps = max_steps;
    config.horizon_safety_factor = horizon_safety_factor;
    config.solver.null_constraint_projection = true;
    config.solver.null_projection_interval = 1000;
    config.name = "null_geodesic";

    Spacetime::SchwarzschildParameters metric;
    metric.mass = rs;

    Simulation::CustomInitialConditions initial;
    initial.r0 = r0;
    initial.vr = vr;
    initial.vphi = vph;

    const Simulation::SimulationResult result =
        Simulation::run_simulation(config, metric, initial);
    const std::vector<State>& history = result.history;
    const State& first = history.front();

    const double E0 = Physics::Observables::conserved_energy(first, rs);
    const double L0 = Physics::Observables::conserved_angular_momentum(first);
    const double b0 = L0 / E0;

    std::cout << "\n=== Null Geodesic Benchmark ===\n";
    std::cout << std::fixed << std::setprecision(15);
    std::cout << "r0 = " << r0 << " | rs = " << rs << " | dt = " << dt << "\n";
    std::cout << "vr = " << vr << " | vph = " << vph << "\n";
    std::cout << "vt = " << first.U[0] << "\n";
    std::cout << "Impact parameter b = " << b0 << "\n\n";

    const std::string csv_name = "null_b_" + std::to_string(b0) + ".csv";
    std::ofstream csv = open_benchmark_csv(csv_name);
    if (!csv.is_open()) {
        return;
    }
    csv << "lambda,r,phi,vt,vr,vph,H,E,L,dE,dL,dvt,dvph,phi_total\n";

    std::cout << std::setw(8) << "step" << std::setw(12) << "lambda" << std::setw(12) << "r"
              << std::setw(12) << "phi" << std::setw(14) << "H_error" << std::setw(14) << "dE"
              << std::setw(14) << "dL" << "\n";
    std::cout << std::string(80, '-') << "\n";

    double r_min = r0;
    double vt_prev = first.U[0];
    double vph_prev = first.U[3];
    double phi_total = 0.0;
    double phi_prev = first.X[3];

    for (size_t step = 0; step < history.size(); ++step) {
        const State& state = history[step];
        const double lambda = step * dt;

        const double r_ = state.X[1];
        const double phi_ = state.X[3];
        const double vt_ = state.U[0];
        const double vr_ = state.U[1];
        const double vph_ = state.U[3];

        double dphi = phi_ - phi_prev;
        if (dphi < -M_PI) {
            dphi += 2 * M_PI;
        }
        if (dphi > M_PI) {
            dphi -= 2 * M_PI;
        }

        phi_total += dphi;
        phi_prev = phi_;

        if (r_ < r_min) {
            r_min = r_;
        }

        const double H = Physics::Observables::null_hamiltonian(state, rs);
        const double H_error = Physics::Observables::null_hamiltonian_error(state, rs);

        const double dvt = vt_ - vt_prev;
        const double dvph = vph_ - vph_prev;
        vt_prev = vt_;
        vph_prev = vph_;

        const double E_ = Physics::Observables::conserved_energy(state, rs);
        const double L_ = Physics::Observables::conserved_angular_momentum(state);

        const double dE = (E_ - E0) / E0;
        const double dL = (L_ - L0) / L0;

        csv << std::scientific << std::setprecision(15) << lambda << "," << r_ << "," << phi_
            << "," << vt_ << "," << vr_ << "," << vph_ << "," << H << "," << E_ << "," << L_
            << "," << dE << "," << dL << "," << dvt << "," << dvph << "," << phi_total << "\n";

        if (step % 1000 == 0) {
            std::cout << std::setw(8) << step << std::setw(12) << lambda << std::setw(12) << r_
                      << std::setw(12) << phi_ << std::setw(14) << std::scientific << H_error
                      << std::fixed << std::setw(14) << dE << std::setw(14) << dL << " H=" << H
                      << " E=" << E_ << " L=" << L_ << "\n";
        }

        if (r_ <= rs * horizon_safety_factor) {
            std::cout << "\n[Horizon crossed]\n";
            break;
        }

        if (r_ > 1000.0 && vr_ > 0) {
            std::cout << "\n[Escaped]\n";
            break;
        }
    }

    if (history.size() - 1 == static_cast<size_t>(max_steps)) {
        std::cout << "\n[Max steps reached]\n";
    }

    csv.close();

    std::cout << "\nMin radius reached: " << r_min << "\n";
    std::cout << "CSV written to " << (benchmark_data_dir() / csv_name) << "\n";
}

void benchmark_null_geodesic(const Spacetime::KerrParameters& metric, double r0,
                             double impact_parameter, double dt, int max_steps,
                             double horizon_safety_factor) {
    Simulation::SimulationConfig config;
    config.spacetime = Simulation::SpacetimeKind::Kerr;
    config.scenario = Simulation::Scenario::NullScatter;
    config.geodesic = Simulation::GeodesicKind::Null;
    config.dt = dt;
    config.max_steps = max_steps;
    config.horizon_safety_factor = horizon_safety_factor;
    config.solver.null_constraint_projection = true;
    config.solver.null_projection_interval = 1000;
    config.name = "null_geodesic_kerr";

    Simulation::NullScatterInitialConditions initial;
    initial.r0 = r0;
    initial.impact_parameter = impact_parameter;

    const Simulation::SimulationResult result =
        Simulation::run_simulation(config, metric, initial);
    const std::vector<State>& history = result.history;
    const State& first = history.front();
    const double horizon = Physics::Observables::outer_horizon_radius(metric);

    const double E0 = Physics::Observables::conserved_energy(first, metric);
    const double L0 = Physics::Observables::conserved_angular_momentum(first, metric);
    const double b0 = L0 / E0;

    std::cout << "\n=== Null Geodesic Benchmark (Kerr) ===\n";
    std::cout << std::fixed << std::setprecision(15);
    std::cout << "r0 = " << r0 << " | rs = " << metric.mass << " | spin = " << metric.spin
              << " | dt = " << dt << "\n";
    std::cout << "r_+ = " << horizon << "\n";
    std::cout << "vr = " << first.U[1] << " | vph = " << first.U[3] << "\n";
    std::cout << "vt = " << first.U[0] << "\n";
    std::cout << "Impact parameter b = " << b0 << " (requested " << impact_parameter << ")\n\n";

    const std::string csv_name = "null_kerr_b_" + std::to_string(b0) + ".csv";
    std::ofstream csv = open_benchmark_csv(csv_name);
    if (!csv.is_open()) {
        return;
    }
    csv << "lambda,r,phi,vt,vr,vph,H,E,L,dE,dL,dvt,dvph,phi_total\n";

    std::cout << std::setw(8) << "step" << std::setw(12) << "lambda" << std::setw(12) << "r"
              << std::setw(12) << "phi" << std::setw(14) << "H_error" << std::setw(14) << "dE"
              << std::setw(14) << "dL" << "\n";
    std::cout << std::string(80, '-') << "\n";

    double r_min = r0;
    double vt_prev = first.U[0];
    double vph_prev = first.U[3];
    double phi_total = 0.0;
    double phi_prev = first.X[3];

    for (size_t step = 0; step < history.size(); ++step) {
        const State& state = history[step];
        const double lambda = step * dt;

        const double r_ = state.X[1];
        const double phi_ = state.X[3];
        const double vt_ = state.U[0];
        const double vr_ = state.U[1];
        const double vph_ = state.U[3];

        double dphi = phi_ - phi_prev;
        if (dphi < -M_PI) {
            dphi += 2 * M_PI;
        }
        if (dphi > M_PI) {
            dphi -= 2 * M_PI;
        }

        phi_total += dphi;
        phi_prev = phi_;

        if (r_ < r_min) {
            r_min = r_;
        }

        const double H = Physics::Observables::null_hamiltonian(state, metric);
        const double H_error = Physics::Observables::null_hamiltonian_error(state, metric);

        const double dvt = vt_ - vt_prev;
        const double dvph = vph_ - vph_prev;
        vt_prev = vt_;
        vph_prev = vph_;

        const double E_ = Physics::Observables::conserved_energy(state, metric);
        const double L_ = Physics::Observables::conserved_angular_momentum(state, metric);

        const double dE = (E_ - E0) / E0;
        const double dL = (L_ - L0) / L0;

        csv << std::scientific << std::setprecision(15) << lambda << "," << r_ << "," << phi_
            << "," << vt_ << "," << vr_ << "," << vph_ << "," << H << "," << E_ << "," << L_
            << "," << dE << "," << dL << "," << dvt << "," << dvph << "," << phi_total << "\n";

        if (step % 1000 == 0) {
            std::cout << std::setw(8) << step << std::setw(12) << lambda << std::setw(12) << r_
                      << std::setw(12) << phi_ << std::setw(14) << std::scientific << H_error
                      << std::fixed << std::setw(14) << dE << std::setw(14) << dL << " H=" << H
                      << " E=" << E_ << " L=" << L_ << "\n";
        }

        if (r_ <= horizon * horizon_safety_factor) {
            std::cout << "\n[Horizon crossed]\n";
            break;
        }

        if (r_ > 1000.0 && vr_ > 0) {
            std::cout << "\n[Escaped]\n";
            break;
        }
    }

    if (history.size() - 1 == static_cast<size_t>(max_steps)) {
        std::cout << "\n[Max steps reached]\n";
    }

    csv.close();

    std::cout << "\nMin radius reached: " << r_min << "\n";
    std::cout << "CSV written to " << (benchmark_data_dir() / csv_name) << "\n";
}
