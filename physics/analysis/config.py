"""Shared paths and physical constants for benchmark analysis."""

from pathlib import Path
import math

from physics.analysis.output_paths import (
    BENCHMARK_DATA_ROOT,
    ensure_output_tree,
    latest_run_dir,
)

# Schwarzschild radius in geometric units (physics convention).
RS = 1.0

# Critical photon impact parameter: b_crit = (3*sqrt(3)/2) * r_s
B_CRIT = (3.0 * (3.0**0.5) / 2.0) * RS

# Photon sphere radius: 1.5 * r_s
PHOTON_SPHERE_R = 1.5 * RS

# Default Kerr spin used by run/benchmark/main.cpp when spacetime=Kerr.
KERR_SPIN = 0.35

REPO_ROOT = Path(__file__).resolve().parents[2]
ensure_output_tree()


def benchmark_data_dir() -> Path:
    """Most recent benchmark CSV run, or the root awaiting data."""
    return latest_run_dir(BENCHMARK_DATA_ROOT) or BENCHMARK_DATA_ROOT


def kerr_outer_horizon(mass: float = RS, spin: float = KERR_SPIN) -> float:
    m = mass / 2.0
    return m + (max(m * m - spin * spin, 0.0) ** 0.5)


def kerr_photon_sphere_radius(mass: float = RS, spin: float = KERR_SPIN) -> float:
    m = mass / 2.0
    if m <= 0.0:
        return 0.0
    spin_ratio = max(-1.0, min(1.0, spin / m))
    return 2.0 * m * (1.0 + math.cos(2.0 * math.acos(-spin_ratio) / 3.0))


def kerr_critical_impact_parameter(mass: float = RS, spin: float = KERR_SPIN) -> float:
    m = mass / 2.0
    if m <= 0.0:
        return 0.0
    a = max(-m, min(m, spin))
    arg = max(-1.0, min(1.0, -a / m))
    c = math.cos(math.acos(arg) / 3.0)
    return -a + 8.0 * m * c * c * c


# Hardcoded initial conditions from run/benchmark/main.cpp (read-only reference).
FREEFALL_R0 = 10.0
FREEFALL_DT = 0.001

ORBITAL_R0 = 6.0
ORBITAL_VR = 0.0
ORBITAL_VPH = 0.06
ORBITAL_DT = 0.01
ORBITAL_MAX_STEPS = 100_000

NULL_R0 = 10.0
NULL_E = 1.0
NULL_DT_DEFAULT = 0.0005
NULL_MAX_STEPS = 200_000

NULL_EPS_ESCAPE = 1e-3
NULL_EPS_CAPTURE = 1e-3
NULL_EPS_NEAR_CRITICAL = 1e-5
