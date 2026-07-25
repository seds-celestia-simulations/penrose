#pragma once

#include <metrics/KerrParameters.h>

void benchmark_null_geodesic(double rs, double r0, double vr, double vph, double dt, int max_steps,
                             double horizon_safety_factor = 1.0001);

void benchmark_null_geodesic(const Spacetime::KerrParameters& metric, double r0,
                             double impact_parameter, double dt, int max_steps,
                             double horizon_safety_factor = 1.0001);
