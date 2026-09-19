#!/usr/bin/env python3
"""Compute and print benchmark validation metrics (read-only)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from physics.analysis.config import REPO_ROOT, benchmark_data_dir
from physics.analysis.metrics import analyze_all, write_metrics_json


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Analyze Penrose benchmark CSV outputs.")
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=None,
        help="Directory containing benchmark CSV files (default: latest outputs/benchmark_data run).",
    )
    parser.add_argument(
        "--json-out",
        type=Path,
        default=None,
        help="Optional path to write metrics JSON (default: <data-dir>/benchmark_metrics.json).",
    )
    args = parser.parse_args(argv)

    data_dir = args.data_dir or benchmark_data_dir()
    json_out = args.json_out or (data_dir / "benchmark_metrics.json")

    if args.data_dir is not None:
        import physics.analysis.loaders as loaders

        loaders.benchmark_data_dir = lambda: data_dir  # type: ignore[assignment]

    try:
        results = analyze_all(data_dir)
    except (FileNotFoundError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(results, indent=2))
    write_metrics_json(json_out, results)
    print(f"\nMetrics JSON written to {json_out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    if str(REPO_ROOT) not in sys.path:
        sys.path.insert(0, str(REPO_ROOT))
    raise SystemExit(main())
