#pragma once

#include "InitialConditions.h"

#include <metrics/KerrParameters.h>
#include <state/GeodesicState.h>

namespace Simulation {

struct SimulationConfig;

namespace KerrInitialStateBuilders {

State build_bound_orbit(const Spacetime::KerrParameters& metric,
                        const BoundOrbitInitialConditions& initial);

State build_radial_freefall(const Spacetime::KerrParameters& metric,
                            const RadialFreefallInitialConditions& initial);

State build_null_scatter(const Spacetime::KerrParameters& metric,
                         const NullScatterInitialConditions& initial);

State build_custom(const SimulationConfig& config, const Spacetime::KerrParameters& metric,
                   const CustomInitialConditions& initial);

} // namespace KerrInitialStateBuilders
} // namespace Simulation
