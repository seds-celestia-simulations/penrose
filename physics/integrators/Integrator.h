#pragma once

#include <state/GeodesicState.h>

#include <functional>

namespace Integration {

using DerivativeFunc = std::function<State(const State&)>;

// CPU-side integrator extension point. RK4 is the default implementation.
class Integrator {
public:
    virtual ~Integrator() = default;
    virtual State step(const State& state, double dt, const DerivativeFunc& derivative) const = 0;
};

// Default integrator (RK4) used by the simulation pipeline.
const Integrator& default_integrator();

} // namespace Integration
