#pragma once

#include <algorithm>
#include <cmath>

#include <metrics/KerrParameters.h>
#include <state/GeodesicState.h>

namespace Physics::Observables {

inline void kerr_metric_components(const Spacetime::KerrParameters& metric, const State& state,
                                   double& g_tt, double& g_tphi, double& g_rr, double& g_thetatheta,
                                   double& g_phiphi) {
    const double r = state.X[1];
    const double theta = state.X[2];
    const double sin_theta = std::sin(theta);
    const double cos_theta = std::cos(theta);
    const double sin2 = sin_theta * sin_theta;
    const double sigma = r * r + metric.spin * metric.spin * cos_theta * cos_theta;
    const double delta = r * r - metric.mass * r + metric.spin * metric.spin;
    const double rho2 = r * r + metric.spin * metric.spin;
    const double A = rho2 * rho2 - metric.spin * metric.spin * delta * sin2;

    g_tt = -(1.0 - metric.mass * r / sigma);
    g_tphi = -metric.mass * metric.spin * r * sin2 / sigma;
    g_rr = sigma / delta;
    g_thetatheta = sigma;
    g_phiphi = sin2 * A / sigma;
}

inline double timelike_norm(const State& state, const Spacetime::KerrParameters& metric) {
    double g_tt = 0.0;
    double g_tphi = 0.0;
    double g_rr = 0.0;
    double g_thetatheta = 0.0;
    double g_phiphi = 0.0;
    kerr_metric_components(metric, state, g_tt, g_tphi, g_rr, g_thetatheta, g_phiphi);

    const double vt = state.U[0];
    const double vr = state.U[1];
    const double vtheta = state.U[2];
    const double vphi = state.U[3];
    return g_tt * vt * vt + g_rr * vr * vr + g_thetatheta * vtheta * vtheta +
           g_phiphi * vphi * vphi + 2.0 * g_tphi * vt * vphi;
}

inline double conserved_energy(const State& state, const Spacetime::KerrParameters& metric) {
    double g_tt = 0.0;
    double g_tphi = 0.0;
    double g_rr = 0.0;
    double g_thetatheta = 0.0;
    double g_phiphi = 0.0;
    kerr_metric_components(metric, state, g_tt, g_tphi, g_rr, g_thetatheta, g_phiphi);
    return -(g_tt * state.U[0] + g_tphi * state.U[3]);
}

inline double conserved_angular_momentum(const State& state,
                                         const Spacetime::KerrParameters& metric) {
    double g_tt = 0.0;
    double g_tphi = 0.0;
    double g_rr = 0.0;
    double g_thetatheta = 0.0;
    double g_phiphi = 0.0;
    kerr_metric_components(metric, state, g_tt, g_tphi, g_rr, g_thetatheta, g_phiphi);
    return g_tphi * state.U[0] + g_phiphi * state.U[3];
}

inline double null_hamiltonian(const State& state, const Spacetime::KerrParameters& metric) {
    return timelike_norm(state, metric);
}

inline double null_hamiltonian_error(const State& state, const Spacetime::KerrParameters& metric) {
    const double H = null_hamiltonian(state, metric);
    const double r = state.X[1];
    const double scale = std::abs(state.U[0] * state.U[0]) + std::abs(state.U[1] * state.U[1]) +
                         std::abs(r * r * state.U[3] * state.U[3]) + 1e-12;
    return std::abs(H) / scale;
}

inline double outer_horizon_radius(const Spacetime::KerrParameters& metric) {
    const double M = metric.mass / 2.0;
    return M + std::sqrt(std::max(M * M - metric.spin * metric.spin, 0.0));
}

inline double equatorial_photon_sphere_radius(const Spacetime::KerrParameters& metric) {
    const double M = metric.mass / 2.0;
    if (M <= 0.0) {
        return 0.0;
    }
    const double spin_ratio = std::clamp(metric.spin / M, -1.0, 1.0);
    return 2.0 * M * (1.0 + std::cos(2.0 * std::acos(-spin_ratio) / 3.0));
}

// Prograde equatorial critical impact parameter (Bardeen–Press–Teukolsky).
// Reduces to (3√3 / 2) * mass at zero spin.
inline double critical_impact_parameter(const Spacetime::KerrParameters& metric) {
    const double M = metric.mass / 2.0;
    if (M <= 0.0) {
        return 0.0;
    }
    const double a = std::clamp(metric.spin, -M, M);
    const double arg = std::clamp(-a / M, -1.0, 1.0);
    const double c = std::cos(std::acos(arg) / 3.0);
    return -a + 8.0 * M * c * c * c;
}

} // namespace Physics::Observables
