#pragma once

namespace Spacetime {

// Backend-independent Kerr metric parameters (POD vocabulary).
// mass is the Schwarzschild radius rs (same convention as SchwarzschildParameters).
// spin is the Kerr spin parameter a with |spin| < mass / 2 for a subextremal hole.
struct KerrParameters {
    double mass = 1.0;
    double spin = 0.0;
};

} // namespace Spacetime
