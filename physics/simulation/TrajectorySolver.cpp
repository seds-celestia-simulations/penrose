#include "TrajectorySolver.h"

namespace Simulation {

std::vector<State> TrajectorySolver::solve(const State& initial_state,
                                             const Dynamics::DynamicsModel& dynamics,
                                             const TerminationPolicy& termination_policy, double dt,
                                             int max_steps, const Integration::Integrator& integrator,
                                             std::function<void(State&, int)> post_step) {
    std::vector<State> history;
    history.reserve(std::min(max_steps, 100000));

    State current = initial_state;
    history.push_back(current);

    Integration::DerivativeFunc derivative = [&dynamics](const State& state) {
        return dynamics.compute_derivative(state);
    };

    for (int i = 0; i < max_steps; ++i) {
        if (termination_policy.should_terminate(current)) {
            break;
        }

        current = integrator.step(current, dt, derivative);
        if (post_step) {
            post_step(current, i);
        }
        history.push_back(current);
    }

    return history;
}

} // namespace Simulation
