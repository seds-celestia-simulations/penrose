#pragma once

#include "../integrators/Integrator.h"
#include "../integrators/RK4Integrator.h"
#include "../geodesics/DynamicsModel.h"
#include "TerminationPolicy.h"

#include <functional>
#include <vector>

namespace Simulation {

class TrajectorySolver {
public:
    static std::vector<State> solve(const State& initial_state, const Dynamics::DynamicsModel& dynamics,
                                    const TerminationPolicy& termination_policy, double dt, int max_steps,
                                    const Integration::Integrator& integrator = Integration::default_integrator(),
                                    std::function<void(State&, int)> post_step = nullptr);
};

} // namespace Simulation
