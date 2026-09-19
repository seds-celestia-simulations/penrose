"""Scientific analysis helpers for Penrose benchmark validation."""

from .config import RS, B_CRIT, REPO_ROOT, benchmark_data_dir
from .output_paths import (
    BENCHMARK_DATA_ROOT,
    OUTPUTS_ROOT,
    VALIDATION_FIGURES_ROOT,
    RENDERED_FRAMES_ROOT,
)

__all__ = [
    "RS",
    "B_CRIT",
    "REPO_ROOT",
    "benchmark_data_dir",
    "BENCHMARK_DATA_ROOT",
    "OUTPUTS_ROOT",
    "VALIDATION_FIGURES_ROOT",
    "RENDERED_FRAMES_ROOT",
]
