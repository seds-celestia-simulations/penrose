#include "KerrInitialStateBuilders.h"

#include "../SimulationConfig.h"

#include "validation/observables/KerrObservables.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace Simulation::KerrInitialStateBuilders {
namespace {

struct KerrMetricComponents {
    double g_tt = 0.0;
    double g_tphi = 0.0;
    double g_rr = 0.0;
    double g_thetatheta = 0.0;
    double g_phiphi = 0.0;
};

KerrMetricComponents metric_at(double rs, double spin, double r, double theta) {
    const double sin_theta = std::sin(theta);
    const double cos_theta = std::cos(theta);
    const double sin2 = sin_theta * sin_theta;
    const double sigma = r * r + spin * spin * cos_theta * cos_theta;
    const double delta = r * r - rs * r + spin * spin;
    const double rho2 = r * r + spin * spin;
    const double A = rho2 * rho2 - spin * spin * delta * sin2;

    KerrMetricComponents metric;
    metric.g_tt = -(1.0 - rs * r / sigma);
    metric.g_tphi = -rs * spin * r * sin2 / sigma;
    metric.g_rr = sigma / delta;
    metric.g_thetatheta = sigma;
    metric.g_phiphi = sin2 * A / sigma;
    return metric;
}

double outer_horizon_radius(double rs, double spin) {
    const double M = rs / 2.0;
    return M + std::sqrt(std::max(M * M - spin * spin, 0.0));
}

void require_outside_horizon(double r, double rs, double spin, const char* scenario) {
    if (r <= outer_horizon_radius(rs, spin)) {
        throw std::runtime_error(std::string(scenario) + ": r0 must be outside the outer horizon");
    }
}

double solve_timelike_time_component(const KerrMetricComponents& metric, double vr, double vtheta,
                                     double vphi) {
    const double spatial = metric.g_rr * vr * vr + metric.g_thetatheta * vtheta * vtheta +
                           metric.g_phiphi * vphi * vphi;
    const double discriminant =
        metric.g_tphi * metric.g_tphi * vphi * vphi - metric.g_tt * (spatial + 1.0);
    if (discriminant < 0.0) {
        throw std::runtime_error("Kerr initial state: timelike velocity constraint is not solvable");
    }
    const double sqrt_disc = std::sqrt(discriminant);
    // g_tt < 0 outside the horizon; the future-directed root uses the minus branch.
    const double vt = (-metric.g_tphi * vphi - sqrt_disc) / metric.g_tt;
    if (vt <= 0.0) {
        return (-metric.g_tphi * vphi + sqrt_disc) / metric.g_tt;
    }
    return vt;
}

double solve_null_time_component(const KerrMetricComponents& metric, double vr, double vtheta,
                                 double vphi) {
    const double spatial = metric.g_rr * vr * vr + metric.g_thetatheta * vtheta * vtheta +
                           metric.g_phiphi * vphi * vphi;
    const double discriminant = metric.g_tphi * metric.g_tphi * vphi * vphi - metric.g_tt * spatial;
    if (discriminant < 0.0) {
        throw std::runtime_error("Kerr initial state: null velocity constraint is not solvable");
    }
    const double sqrt_disc = std::sqrt(discriminant);
    const double vt = (-metric.g_tphi * vphi - sqrt_disc) / metric.g_tt;
    if (vt <= 0.0) {
        return (-metric.g_tphi * vphi + sqrt_disc) / metric.g_tt;
    }
    return vt;
}

void solve_energy_and_angular(const KerrMetricComponents& metric, double energy, double angular,
                              double& vt, double& vphi) {
    const double determinant = metric.g_tt * metric.g_phiphi - metric.g_tphi * metric.g_tphi;
    if (std::abs(determinant) < 1e-14) {
        throw std::runtime_error("Kerr initial state: degenerate t-phi metric block");
    }
    vt = (-energy * metric.g_phiphi + angular * metric.g_tphi) / determinant;
    vphi = (metric.g_tt * angular - metric.g_tphi * (-energy)) / determinant;
}

} // namespace

State build_bound_orbit(const Spacetime::KerrParameters& metric,
                        const BoundOrbitInitialConditions& initial) {
    require_outside_horizon(initial.r0, metric.mass, metric.spin, "BoundOrbit");
    const KerrMetricComponents g =
        metric_at(metric.mass, metric.spin, initial.r0, initial.theta0);
    const double vt =
        solve_timelike_time_component(g, initial.vr, initial.vtheta, initial.vphi);
    return State(Eigen::Vector4d(initial.t0, initial.r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, initial.vr, initial.vtheta, initial.vphi));
}

State build_radial_freefall(const Spacetime::KerrParameters& metric,
                            const RadialFreefallInitialConditions& initial) {
    require_outside_horizon(initial.r0, metric.mass, metric.spin, "RadialFreefall");
    const KerrMetricComponents g =
        metric_at(metric.mass, metric.spin, initial.r0, initial.theta0);
    const double vt = -1.0 / g.g_tt;
    const double inside = -(1.0 + g.g_tt * vt * vt) / g.g_rr;
    if (inside < 0.0) {
        throw std::runtime_error("RadialFreefall: radial velocity is imaginary");
    }
    const double vr = -std::sqrt(inside);
    return State(Eigen::Vector4d(initial.t0, initial.r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, vr, 0.0, 0.0));
}

State build_null_scatter(const Spacetime::KerrParameters& metric,
                         const NullScatterInitialConditions& initial) {
    require_outside_horizon(initial.r0, metric.mass, metric.spin, "NullScatter");
    const KerrMetricComponents g =
        metric_at(metric.mass, metric.spin, initial.r0, initial.theta0);

    const double b_crit = Physics::Observables::critical_impact_parameter(metric);
    const double b = (initial.impact_parameter > 0.0)
                         ? initial.impact_parameter
                         : (b_crit + initial.impact_parameter_offset);

    double vt = 0.0;
    double vphi = 0.0;
    solve_energy_and_angular(g, 1.0, b, vt, vphi);

    const double norm = g.g_tt * vt * vt + g.g_rr * 0.0 + g.g_phiphi * vphi * vphi +
                        2.0 * g.g_tphi * vt * vphi;
    const double inside = -norm / g.g_rr;
    if (inside < 0.0) {
        throw std::runtime_error("NullScatter: impact parameter yields imaginary radial velocity");
    }
    const double vr = -std::sqrt(inside);
    return State(Eigen::Vector4d(initial.t0, initial.r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, vr, 0.0, vphi));
}

State build_custom(const SimulationConfig& config, const Spacetime::KerrParameters& metric,
                   const CustomInitialConditions& initial) {
    require_outside_horizon(initial.r0, metric.mass, metric.spin, "Custom");
    const KerrMetricComponents g =
        metric_at(metric.mass, metric.spin, initial.r0, initial.theta0);

    double vt = initial.vt;
    if (vt == 0.0) {
        if (config.geodesic == GeodesicKind::Null) {
            vt = solve_null_time_component(g, initial.vr, initial.vtheta, initial.vphi);
        } else {
            vt = solve_timelike_time_component(g, initial.vr, initial.vtheta, initial.vphi);
        }
    }

    return State(Eigen::Vector4d(initial.t0, initial.r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, initial.vr, initial.vtheta, initial.vphi));
}

} // namespace Simulation::KerrInitialStateBuilders
