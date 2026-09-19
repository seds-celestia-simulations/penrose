#include "RK4Integrator.h"

namespace Integration {
namespace {

const RK4Integrator kDefaultIntegrator{};

State rk4_step(const State& state, double dt, const DerivativeFunc& derivative) {
    const State k1 = derivative(state);
    const State k2 = derivative(state + k1 * (dt / 2.0));
    const State k3 = derivative(state + k2 * (dt / 2.0));
    const State k4 = derivative(state + k3 * dt);
    return state + (dt / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
}

} // namespace

State RK4Integrator::step(const State& state, double dt, const DerivativeFunc& derivative) const {
    return rk4_step(state, dt, derivative);
}

const Integrator& default_integrator() {
    return kDefaultIntegrator;
}

State stepRK4(const State& state, double dt, const DerivativeFunc& derivative) {
    return kDefaultIntegrator.step(state, dt, derivative);
}

} // namespace Integration
