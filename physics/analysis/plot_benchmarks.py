#!/usr/bin/env python3
"""Deterministic batch figure generation for benchmark validation reports."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from physics.analysis.config import REPO_ROOT, benchmark_data_dir
from physics.analysis.output_paths import VALIDATION_FIGURES_ROOT, create_run_dir
from physics.analysis.plots import plot_all


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate benchmark validation figures.")
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=None,
        help="Directory containing benchmark CSV files.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory for PNG/PDF figures (default: new outputs/validation_figures run).",
    )
    args = parser.parse_args(argv)

    data_dir = args.data_dir or benchmark_data_dir()
    out_dir = args.out_dir or create_run_dir(VALIDATION_FIGURES_ROOT)

    if args.data_dir is not None:
        import physics.analysis.loaders as loaders

        loaders.benchmark_data_dir = lambda: data_dir  # type: ignore[assignment]

    try:
        saved = plot_all(out_dir, data_dir)
    except (FileNotFoundError, ValueError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(f"Generated {len(saved)} figure files under {out_dir}:")
    seen = set()
    for png, pdf in saved:
        stem = Path(png).stem
        if stem not in seen:
            print(f"  - {png}")
            print(f"  - {pdf}")
            seen.add(stem)
    return 0


if __name__ == "__main__":
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    raise SystemExit(main())
