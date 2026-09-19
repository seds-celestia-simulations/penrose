#pragma once

#include "Integrator.h"

namespace Integration {

class RK4Integrator final : public Integrator {
public:
    State step(const State& state, double dt, const DerivativeFunc& derivative) const override;
};

// Legacy free-function entry point (delegates to RK4Integrator).
State stepRK4(const State& state, double dt, const DerivativeFunc& derivative);

} // namespace Integration
