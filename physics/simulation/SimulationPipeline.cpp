#include "SimulationConfig.h"
#include "SimulationRequest.h"
#include "TerminationPolicy.h"
#include "TrajectorySolver.h"

#include "../geodesics/GeodesicDynamics.h"
#include "../metrics/KerrMetric.h"
#include "../metrics/SchwarzschildMetric.h"
#include "initial_conditions/KerrInitialStateBuilders.h"
#include "initial_conditions/SchwarzschildInitialStateBuilders.h"

#include "validation/observables/KerrObservables.h"

#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

namespace Simulation {
namespace {

void require_spacetime(const SimulationConfig& config, SpacetimeKind expected, Scenario scenario) {
    if (config.spacetime != expected) {
        throw std::runtime_error(
            "run_simulation: metric parameters do not match SimulationConfig::spacetime");
    }
    if (config.scenario != scenario) {
        throw std::runtime_error(
            "run_simulation: initial-condition type does not match SimulationConfig::scenario");
    }
}

std::unique_ptr<Spacetime::Metric> make_schwarzschild_metric(
    const Spacetime::SchwarzschildParameters& params) {
    return std::make_unique<Spacetime::SchwarzschildMetric>(params.mass);
}

std::unique_ptr<Spacetime::Metric> make_kerr_metric(const Spacetime::KerrParameters& params) {
    return std::make_unique<Spacetime::KerrMetric>(params.mass, params.spin);
}

SimulationMetadata schwarzschild_metadata(const Spacetime::SchwarzschildParameters& metric) {
    SimulationMetadata metadata;
    metadata.metric = Spacetime::MetricKind::Schwarzschild;
    metadata.coordinate_chart = Spacetime::CoordinateChartKind::SchwarzschildSpherical;
    metadata.horizon_radius = metric.mass;
    metadata.photon_sphere_radius = 1.5 * metric.mass;
    return metadata;
}

SimulationMetadata kerr_metadata(const Spacetime::KerrParameters& metric) {
    SimulationMetadata metadata;
    metadata.metric = Spacetime::MetricKind::Kerr;
    metadata.coordinate_chart = Spacetime::CoordinateChartKind::KerrBoyerLindquist;
    metadata.horizon_radius = Physics::Observables::outer_horizon_radius(metric);
    metadata.photon_sphere_radius = Physics::Observables::equatorial_photon_sphere_radius(metric);
    return metadata;
}

std::function<void(State&, int)> make_schwarzschild_post_step(const SimulationConfig& config,
                                                              double rs) {
    if (!config.solver.null_constraint_projection) {
        return nullptr;
    }

    const int interval = std::max(1, config.solver.null_projection_interval);
    return [rs, interval](State& state, int step) {
        const double r = state.X[1];
        if (r <= rs) {
            return;
        }
        const double f = 1.0 - rs / r;
        const double vr = state.U[1];
        const double vph = state.U[3];
        if (step % interval == 0) {
            state.U[0] = std::sqrt((vr * vr / f + r * r * vph * vph) / f);
        }
        if (state.U[0] < 0.0) {
            state.U[0] = std::abs(state.U[0]);
        }
    };
}

std::function<void(State&, int)> make_kerr_post_step(const SimulationConfig& config,
                                                     const Spacetime::KerrParameters& metric) {
    if (!config.solver.null_constraint_projection) {
        return nullptr;
    }

    const int interval = std::max(1, config.solver.null_projection_interval);
    return [metric, interval](State& state, int step) {
        if (state.U[0] < 0.0) {
            state.U[0] = std::abs(state.U[0]);
        }
        if (step % interval != 0) {
            return;
        }

        const double horizon = Physics::Observables::outer_horizon_radius(metric);
        if (state.X[1] <= horizon) {
            return;
        }

        double g_tt = 0.0;
        double g_tphi = 0.0;
        double g_rr = 0.0;
        double g_thetatheta = 0.0;
        double g_phiphi = 0.0;
        Physics::Observables::kerr_metric_components(metric, state, g_tt, g_tphi, g_rr,
                                                     g_thetatheta, g_phiphi);

        const double vr = state.U[1];
        const double vtheta = state.U[2];
        const double vphi = state.U[3];
        const double spatial =
            g_rr * vr * vr + g_thetatheta * vtheta * vtheta + g_phiphi * vphi * vphi;
        const double discriminant = g_tphi * g_tphi * vphi * vphi - g_tt * spatial;
        if (discriminant < 0.0 || g_tt == 0.0) {
            return;
        }

        const double sqrt_disc = std::sqrt(discriminant);
        double vt = (-g_tphi * vphi - sqrt_disc) / g_tt;
        if (vt <= 0.0) {
            vt = (-g_tphi * vphi + sqrt_disc) / g_tt;
        }
        state.U[0] = vt;
    };
}

SimulationResult integrate_schwarzschild(const SimulationConfig& config,
                                         std::unique_ptr<Spacetime::Metric> metric_impl,
                                         const Spacetime::SchwarzschildParameters& metric_params,
                                         const State& initial) {
    Dynamics::GeodesicDynamics dynamics(*metric_impl);
    HorizonTermination policy(metric_params.mass, config.horizon_safety_factor);

    SimulationResult result;
    result.history = TrajectorySolver::solve(
        initial, dynamics, policy, config.dt, config.max_steps, Integration::default_integrator(),
        make_schwarzschild_post_step(config, metric_params.mass));
    result.characteristic_radius = metric_params.mass;
    result.name = config.name;
    result.spacetime = config.spacetime;
    result.metadata = schwarzschild_metadata(metric_params);
    return result;
}

SimulationResult integrate_kerr(const SimulationConfig& config,
                                std::unique_ptr<Spacetime::Metric> metric_impl,
                                const Spacetime::KerrParameters& metric_params,
                                const State& initial) {
    Dynamics::GeodesicDynamics dynamics(*metric_impl);
    const double horizon = Physics::Observables::outer_horizon_radius(metric_params);
    HorizonTermination policy(horizon, config.horizon_safety_factor);

    SimulationResult result;
    result.history = TrajectorySolver::solve(
        initial, dynamics, policy, config.dt, config.max_steps, Integration::default_integrator(),
        make_kerr_post_step(config, metric_params));
    result.characteristic_radius = metric_params.mass;
    result.name = config.name;
    result.spacetime = config.spacetime;
    result.metadata = kerr_metadata(metric_params);
    return result;
}

} // namespace

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const BoundOrbitInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Schwarzschild, Scenario::BoundOrbit);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_bound_orbit(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const RadialFreefallInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Schwarzschild, Scenario::RadialFreefall);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_radial_freefall(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const NullScatterInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Schwarzschild, Scenario::NullScatter);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_null_scatter(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::SchwarzschildParameters& metric,
                                const CustomInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Schwarzschild, Scenario::Custom);
    return integrate_schwarzschild(config, make_schwarzschild_metric(metric), metric,
                                   InitialStateBuilders::build_custom(config, metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const BoundOrbitInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Kerr, Scenario::BoundOrbit);
    return integrate_kerr(config, make_kerr_metric(metric), metric,
                          KerrInitialStateBuilders::build_bound_orbit(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const RadialFreefallInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Kerr, Scenario::RadialFreefall);
    return integrate_kerr(config, make_kerr_metric(metric), metric,
                          KerrInitialStateBuilders::build_radial_freefall(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const NullScatterInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Kerr, Scenario::NullScatter);
    return integrate_kerr(config, make_kerr_metric(metric), metric,
                          KerrInitialStateBuilders::build_null_scatter(metric, initial));
}

SimulationResult run_simulation(const SimulationConfig& config,
                                const Spacetime::KerrParameters& metric,
                                const CustomInitialConditions& initial) {
    require_spacetime(config, SpacetimeKind::Kerr, Scenario::Custom);
    return integrate_kerr(config, make_kerr_metric(metric), metric,
                          KerrInitialStateBuilders::build_custom(config, metric, initial));
}

SimulationResult run_simulation(const SimulationRequest& request) {
    return std::visit(
        [&](const auto& metric) -> SimulationResult {
            return std::visit(
                [&](const auto& initial) -> SimulationResult {
                    return run_simulation(request.config, metric, initial);
                },
                request.initial);
        },
        request.metric);
}

std::vector<SimulationResult> run_all(std::span<const SimulationRequest> requests) {
    std::vector<SimulationResult> results;
    results.reserve(requests.size());
    for (const SimulationRequest& request : requests) {
        results.push_back(run_simulation(request));
    }
    return results;
}

std::vector<SimulationResult> run_all(const std::vector<SimulationRequest>& requests) {
    return run_all(std::span<const SimulationRequest>(requests));
}

SimulationRequest make_schwarzschild_request(SimulationConfig config,
                                             const Spacetime::SchwarzschildParameters& metric,
                                             InitialConditions initial) {
    config.spacetime = SpacetimeKind::Schwarzschild;
    return SimulationRequest{std::move(config), metric, std::move(initial)};
}

SimulationRequest make_kerr_request(SimulationConfig config, const Spacetime::KerrParameters& metric,
                                    InitialConditions initial) {
    config.spacetime = SpacetimeKind::Kerr;
    return SimulationRequest{std::move(config), metric, std::move(initial)};
}

} // namespace Simulation
