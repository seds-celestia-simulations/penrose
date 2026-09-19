"""Parse benchmark_test console log for null geodesic sweep summaries."""

from __future__ import annotations

import re
from pathlib import Path

from .config import B_CRIT, REPO_ROOT

_RUN_B = re.compile(r"^=== Running b = ([0-9.eE+-]+)")
_NULL_HDR = re.compile(r"^=== Null Geodesic Benchmark ===")
_IMPACT = re.compile(r"^Impact parameter b = ([0-9.eE+-]+)")
_DT = re.compile(r"^Running dt = ([0-9.eE+-]+)")
_RMIN = re.compile(r"^Min radius reached: ([0-9.eE+-]+)")


def parse_null_sweep_log(log_path: Path) -> dict[str, list[dict[str, float | str | None]]]:
    text = log_path.read_text()
    full_sweep: list[dict[str, float | str | None]] = []
    dt_sweep: list[dict[str, float | str | None]] = []

    current_b: float | None = None
    current_dt = 0.0005
    in_null = False
    outcome: str | None = None
    r_min: float | None = None

    def flush() -> None:
        nonlocal current_b, in_null, outcome, r_min
        if current_b is None or not in_null:
            return
        entry = {
            "b": current_b,
            "r_min": r_min,
            "outcome": outcome or "unknown",
            "dt": current_dt,
            "side": "above" if current_b > B_CRIT else "below",
        }
        if current_dt == 0.0005:
            full_sweep.append(entry)
        else:
            dt_sweep.append(entry)
        in_null = False
        outcome = None
        r_min = None

    for line in text.splitlines():
        if m := _DT.match(line):
            flush()
            current_dt = float(m.group(1))
            continue
        if m := _RUN_B.match(line):
            flush()
            current_b = float(m.group(1))
            continue
        if _NULL_HDR.match(line):
            in_null = True
            outcome = None
            r_min = None
            continue
        if in_null and (m := _IMPACT.match(line)):
            current_b = float(m.group(1))
            continue
        if "[Horizon crossed]" in line:
            outcome = "capture"
        elif "[Escaped]" in line:
            outcome = "escape (post-processed)"
        elif "[Max steps reached]" in line:
            if outcome is None:
                outcome = "max steps"
        if m := _RMIN.match(line):
            r_min = float(m.group(1))

    flush()
    return {"full_sweep": full_sweep, "dt_sweep": dt_sweep}


def default_log_path() -> Path:
    full = REPO_ROOT / "build" / "benchmark_run.log"
    if full.is_file() and full.stat().st_size > 1000:
        return full
    return REPO_ROOT / "build" / "benchmark_run_reduced.log"
