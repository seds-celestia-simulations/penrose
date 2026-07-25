#pragma once

#include <metrics/KerrParameters.h>

void benchmark_freefall(double rs, double r0, double dt, int max_steps);
void benchmark_freefall(const Spacetime::KerrParameters& metric, double r0, double dt,
                        int max_steps);
