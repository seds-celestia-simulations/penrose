#pragma once

namespace Simulation {

// Scenario-specific initial conditions. Metric-independent field layout where possible;
// the pipeline interprets them for the selected spacetime.

struct BoundOrbitInitialConditions {
    double t0 = 0.0;
    double r0 = 6.0;
    double theta0 = 1.5707963267948966; // π/2
    double phi0 = 0.0;
    double vr = 0.0;
    double vtheta = 0.0;
    double vphi = 0.06;
};

struct RadialFreefallInitialConditions {
    double t0 = 0.0;
    double r0 = 10.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
};

struct NullScatterInitialConditions {
    double t0 = 0.0;
    double r0 = 10.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
    // Absolute impact parameter. If <= 0, use b_crit + impact_parameter_offset.
    double impact_parameter = 0.0;
    double impact_parameter_offset = 1e-3;
};

struct CustomInitialConditions {
    double t0 = 0.0;
    double r0 = 6.0;
    double theta0 = 1.5707963267948966;
    double phi0 = 0.0;
    double vt = 0.0; // if 0, pipeline may complete from geodesic constraints
    double vr = 0.0;
    double vtheta = 0.0;
    double vphi = 0.0;
};

} // namespace Simulation
