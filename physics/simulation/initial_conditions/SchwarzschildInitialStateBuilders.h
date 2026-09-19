#pragma once

#include "InitialConditions.h"

#include <metrics/SchwarzschildParameters.h>
#include <state/GeodesicState.h>

namespace Simulation {

struct SimulationConfig;

namespace InitialStateBuilders {

State build_bound_orbit(const Spacetime::SchwarzschildParameters& metric,
                        const BoundOrbitInitialConditions& initial);

State build_radial_freefall(const Spacetime::SchwarzschildParameters& metric,
                            const RadialFreefallInitialConditions& initial);

State build_null_scatter(const Spacetime::SchwarzschildParameters& metric,
                         const NullScatterInitialConditions& initial);

State build_custom(const SimulationConfig& config, const Spacetime::SchwarzschildParameters& metric,
                   const CustomInitialConditions& initial);

} // namespace InitialStateBuilders
} // namespace Simulation
