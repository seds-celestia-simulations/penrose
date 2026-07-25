#pragma once

#include "SimulationConfig.h"

#include <metrics/KerrParameters.h>
#include <metrics/SchwarzschildParameters.h>

#include <span>
#include <variant>
#include <vector>

namespace Simulation {

using InitialConditions = std::variant<BoundOrbitInitialConditions, RadialFreefallInitialConditions,
                                       NullScatterInitialConditions, CustomInitialConditions>;

using MetricParameters =
    std::variant<Spacetime::SchwarzschildParameters, Spacetime::KerrParameters>;

struct SimulationRequest {
    SimulationConfig config;
    MetricParameters metric = Spacetime::SchwarzschildParameters{};
    InitialConditions initial = BoundOrbitInitialConditions{};
};

SimulationResult run_simulation(const SimulationRequest& request);
std::vector<SimulationResult> run_all(std::span<const SimulationRequest> requests);
std::vector<SimulationResult> run_all(const std::vector<SimulationRequest>& requests);

SimulationRequest make_schwarzschild_request(SimulationConfig config,
                                             const Spacetime::SchwarzschildParameters& metric,
                                             InitialConditions initial);

SimulationRequest make_kerr_request(SimulationConfig config, const Spacetime::KerrParameters& metric,
                                    InitialConditions initial);

} // namespace Simulation
