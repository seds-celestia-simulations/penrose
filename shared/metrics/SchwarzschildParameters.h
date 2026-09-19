#pragma once

namespace Spacetime {

// Backend-independent Schwarzschild metric parameters (POD vocabulary).
struct SchwarzschildParameters {
    double mass = 1.0; // Schwarzschild radius rs in code units where G=c=1
};

} // namespace Spacetime
