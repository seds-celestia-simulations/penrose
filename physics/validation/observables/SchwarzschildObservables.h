#pragma once

#include <cmath>

#include <metrics/SchwarzschildParameters.h>
#include <state/GeodesicState.h>

namespace Physics::Observables {

// Exact proper time from r0 to rs for radial free-fall with E=1.
inline double analytical_freefall_time(double r0, double rs) {
    return (2.0 / 3.0) * (1.0 / std::sqrt(rs)) * (std::pow(r0, 1.5) - std::pow(rs, 1.5));
}

inline double schwarzschild_f(double r, double rs) {
    return 1.0 - rs / r;
}

inline double timelike_norm(const State& state, double rs) {
    const double r = state.X[1];
    const double f = schwarzschild_f(r, rs);
    const double vt = state.U[0];
    const double vr = state.U[1];
    const double vph = state.U[3];
    return -f * vt * vt + (1.0 / f) * vr * vr + r * r * vph * vph;
}

inline double conserved_energy(const State& state, double rs) {
    const double r = state.X[1];
    return schwarzschild_f(r, rs) * state.U[0];
}

inline double conserved_angular_momentum(const State& state) {
    const double r = state.X[1];
    return r * r * state.U[3];
}

inline double null_hamiltonian(const State& state, double rs) {
    const double r = state.X[1];
    const double f = schwarzschild_f(r, rs);
    const double vt = state.U[0];
    const double vr = state.U[1];
    const double vph = state.U[3];
    return -f * vt * vt + (1.0 / f) * vr * vr + r * r * vph * vph;
}

inline double null_hamiltonian_error(const State& state, double rs) {
    const double r = state.X[1];
    const double f = schwarzschild_f(r, rs);
    const double vt = state.U[0];
    const double vr = state.U[1];
    const double vph = state.U[3];
    const double H = null_hamiltonian(state, rs);
    const double scale = std::abs(vt * vt) + std::abs(vr * vr) + std::abs(r * r * vph * vph) + 1e-12;
    return std::abs(H) / scale;
}

inline double critical_impact_parameter(double rs) {
    return (3.0 * std::sqrt(3.0) / 2.0) * rs;
}

} // namespace Physics::Observables
