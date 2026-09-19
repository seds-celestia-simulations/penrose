"""Timestamped output directories under outputs/."""

from __future__ import annotations

from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUTS_ROOT = REPO_ROOT / "outputs"
BENCHMARK_DATA_ROOT = OUTPUTS_ROOT / "benchmark_data"
VALIDATION_FIGURES_ROOT = OUTPUTS_ROOT / "validation_figures"
RENDERED_FRAMES_ROOT = OUTPUTS_ROOT / "rendered_frames"


def timestamp_string() -> str:
    return datetime.now().strftime("%Y-%m-%d_%H-%M-%S")


def create_run_dir(category_root: Path) -> Path:
    run_dir = category_root / timestamp_string()
    run_dir.mkdir(parents=True, exist_ok=True)
    return run_dir


def latest_run_dir(category_root: Path) -> Path | None:
    if not category_root.exists():
        return None
    runs = sorted([p for p in category_root.iterdir() if p.is_dir()], reverse=True)
    return runs[0] if runs else None


def ensure_output_tree() -> None:
    for path in (BENCHMARK_DATA_ROOT, VALIDATION_FIGURES_ROOT, RENDERED_FRAMES_ROOT):
        path.mkdir(parents=True, exist_ok=True)
