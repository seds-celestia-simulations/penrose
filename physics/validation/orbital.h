#pragma once

#include <metrics/KerrParameters.h>

void benchmark_orbital(double rs, double r0, double vr, double vph, double dt, int max_steps);
void benchmark_orbital(const Spacetime::KerrParameters& metric, double r0, double vr, double vph,
                       double dt, int max_steps);
