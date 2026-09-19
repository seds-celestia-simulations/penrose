#!/usr/bin/env python3
"""Write benchmark_validation.md from measured CSV metrics."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from physics.analysis.config import REPO_ROOT, benchmark_data_dir
from physics.analysis.report import write_report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate benchmark validation report.")
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=None,
        help="Directory containing benchmark CSV files.",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Report output path (default: <data-dir>/benchmark_validation.md).",
    )
    args = parser.parse_args(argv)

    data_dir = args.data_dir or benchmark_data_dir()
    out_path = args.out or (data_dir / "benchmark_validation.md")

    if args.data_dir is not None:
        import physics.analysis.loaders as loaders

        loaders.benchmark_data_dir = lambda: data_dir  # type: ignore[assignment]

    try:
        path = write_report(out_path, data_dir)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"Report written to {path}")
    return 0


if __name__ == "__main__":
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    raise SystemExit(main())
