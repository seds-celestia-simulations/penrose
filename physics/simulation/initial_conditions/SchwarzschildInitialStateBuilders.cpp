#include "SchwarzschildInitialStateBuilders.h"

#include "../SimulationConfig.h"

#include <cmath>
#include <stdexcept>

namespace Simulation::InitialStateBuilders {

State build_bound_orbit(const Spacetime::SchwarzschildParameters& metric,
                        const BoundOrbitInitialConditions& initial) {
    const double rs = metric.mass;
    const double r0 = initial.r0;
    const double f = 1.0 - rs / r0;
    if (f <= 0.0) {
        throw std::runtime_error("BoundOrbit: r0 must be greater than the Schwarzschild radius");
    }
    const double sin_theta = std::sin(initial.theta0);
    const double inner =
        (1.0 + r0 * r0 * initial.vtheta * initial.vtheta + r0 * r0 * sin_theta * sin_theta * initial.vphi * initial.vphi + (initial.vr * initial.vr) / f) / f;
    const double vt = std::sqrt(std::max(inner, 0.0));
    return State(Eigen::Vector4d(initial.t0, r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, initial.vr, initial.vtheta, initial.vphi));
}

State build_radial_freefall(const Spacetime::SchwarzschildParameters& metric,
                            const RadialFreefallInitialConditions& initial) {
    const double rs = metric.mass;
    const double r0 = initial.r0;
    const double f = 1.0 - rs / r0;
    if (f <= 0.0) {
        throw std::runtime_error("RadialFreefall: r0 must be greater than the Schwarzschild radius");
    }
    const double vt = 1.0 / f;
    const double vr = -std::sqrt(rs / r0);
    return State(Eigen::Vector4d(initial.t0, r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, vr, 0.0, 0.0));
}

State build_null_scatter(const Spacetime::SchwarzschildParameters& metric,
                         const NullScatterInitialConditions& initial) {
    const double rs = metric.mass;
    const double r0 = initial.r0;
    const double f = 1.0 - rs / r0;
    if (f <= 0.0) {
        throw std::runtime_error("NullScatter: r0 must be greater than the Schwarzschild radius");
    }

    const double b_crit = (3.0 * std::sqrt(3.0) / 2.0) * rs;
    const double b = (initial.impact_parameter > 0.0)
                         ? initial.impact_parameter
                         : (b_crit + initial.impact_parameter_offset);

    const double E = 1.0;
    const double L = b * E;
    const double vt = E / f;
    const double sin_theta = std::sin(initial.theta0);
    // For total angular momentum L, if vtheta=0, L^2 = r^4 sin^2(theta) vphi^2
    // So vphi = L / (r^2 sin(theta))
    const double vph = (sin_theta != 0.0) ? (L / (r0 * r0 * sin_theta)) : 0.0;
    const double term_phi = r0 * r0 * sin_theta * sin_theta * vph * vph;
    
    const double inside = E * E - f * (term_phi / (r0 * r0)); // Wait, term_phi is just L^2 / (r^2 sin^2). No, L^2 / (r0^2).
    // Let's use the exact relation: E^2 = f * (vr^2/f + r^2 vtheta^2 + r^2 sin^2 vphi^2)
    // E^2 = vr^2 + f * (L^2 / (r^2 sin^2) * sin^2) = vr^2 + f * (L^2 / r^2)
    // So inside is E^2 - f * (L^2 / r^2), which is independent of theta0!
    // But vph depends on theta0.
    const double inside_vr = E * E - f * (L * L / (r0 * r0));
    if (inside_vr < 0.0) {
        throw std::runtime_error("NullScatter: impact parameter yields imaginary radial velocity");
    }
    const double vr = -std::sqrt(inside_vr);
    return State(Eigen::Vector4d(initial.t0, r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, vr, 0.0, vph));
}

State build_custom(const SimulationConfig& config, const Spacetime::SchwarzschildParameters& metric,
                   const CustomInitialConditions& initial) {
    double vt = initial.vt;
    const double rs = metric.mass;
    const double r0 = initial.r0;
    const double f = 1.0 - rs / r0;
    if (f <= 0.0) {
        throw std::runtime_error("Custom: r0 must be greater than the Schwarzschild radius");
    }

    if (vt == 0.0) {
        const double sin_theta = std::sin(initial.theta0);
        if (config.geodesic == GeodesicKind::Null) {
            const double term_r = (initial.vr * initial.vr) / f;
            const double term_theta = r0 * r0 * initial.vtheta * initial.vtheta;
            const double term_phi = r0 * r0 * sin_theta * sin_theta * initial.vphi * initial.vphi;
            vt = std::sqrt((term_r + term_theta + term_phi) / f);
        } else {
            const double term_r = (initial.vr * initial.vr) / f;
            const double term_theta = r0 * r0 * initial.vtheta * initial.vtheta;
            const double term_phi = r0 * r0 * sin_theta * sin_theta * initial.vphi * initial.vphi;
            const double inner = (1.0 + term_theta + term_phi + term_r) / f;
            vt = std::sqrt(std::max(inner, 0.0));
        }
    }

    return State(Eigen::Vector4d(initial.t0, r0, initial.theta0, initial.phi0),
                 Eigen::Vector4d(vt, initial.vr, initial.vtheta, initial.vphi));
}

} // namespace Simulation::InitialStateBuilders
