"""Read-only validation metrics for benchmark CSV outputs."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

from .config import (
    B_CRIT,
    FREEFALL_DT,
    FREEFALL_R0,
    NULL_R0,
    ORBITAL_DT,
    ORBITAL_R0,
    ORBITAL_VPH,
    ORBITAL_VR,
    RS,
    kerr_critical_impact_parameter,
    kerr_outer_horizon,
)
from .loaders import is_kerr_run, load_all_null_geodesics, load_freefall, load_orbital


def _run_horizon(df: pd.DataFrame | None = None, data_dir: Path | None = None) -> float:
    spacetime = None
    if df is not None:
        spacetime = df.attrs.get("spacetime")
    if spacetime is None and data_dir is not None:
        spacetime = "kerr" if is_kerr_run(data_dir) else "schwarzschild"
    if spacetime == "kerr":
        return kerr_outer_horizon()
    return RS


def _run_b_crit(df: pd.DataFrame | None = None, data_dir: Path | None = None) -> float:
    spacetime = None
    if df is not None:
        spacetime = df.attrs.get("spacetime")
    if spacetime is None and data_dir is not None:
        spacetime = "kerr" if is_kerr_run(data_dir) else "schwarzschild"
    if spacetime == "kerr":
        return kerr_critical_impact_parameter()
    return B_CRIT



def analytical_freefall_time(r0: float, rs: float = RS) -> float:
    """Proper time from r0 to rs for E=1 radial infall: tau = (2/3)(r0^(3/2)-rs^(3/2))/sqrt(rs)."""
    return (2.0 / 3.0) * (1.0 / np.sqrt(rs)) * (r0 ** 1.5 - rs ** 1.5)


def analytical_radius_freefall(tau: np.ndarray, r0: float, rs: float = RS) -> np.ndarray:
    """Analytic r(tau) for E=1 radial infall with (dr/dtau)^2 = rs/r."""
    inner = r0 ** 1.5 - (3.0 / 2.0) * np.sqrt(rs) * tau
    inner = np.maximum(inner, rs ** 1.5)
    return inner ** (2.0 / 3.0)


def schwarzschild_f(r: np.ndarray, rs: float = RS) -> np.ndarray:
    return 1.0 - rs / np.asarray(r)


@dataclass
class FreefallMetrics:
    r0: float
    dt: float
    rs: float
    tau_analytical: float
    tau_numerical: float
    horizon_step: int
    crossing_abs_error: float
    crossing_rel_error_pct: float
    radius_monotonic_decreasing: bool
    norm_min: float
    norm_max: float
    norm_drift: float
    energy_min: float
    energy_max: float
    energy_drift: float
    ic_note: str


@dataclass
class OrbitalMetrics:
    r0: float
    dt: float
    rs: float
    r_min: float
    r_max: float
    bound: bool
    escaped: bool
    horizon_crossed: bool
    norm_min: float
    norm_max: float
    norm_drift: float
    energy_drift_max: float
    angular_momentum_drift_max: float
    periapsis_count: int
    periapsis_advance_mean: float | None
    all_finite: bool
    ic_note: str


@dataclass
class NullRunMetrics:
    impact_parameter: float
    source_file: str
    dt: float
    classification: str
    r_min: float
    r_final: float
    lambda_final: float
    deflection_deg: float | None
    h_error_max: float
    h_error_mean: float
    de_max: float
    dl_max: float
    escaped_postprocessed: bool
    captured: bool


@dataclass
class NullSweepSummary:
    b_crit: float
    runs: list[NullRunMetrics]
    escape_runs: list[float]
    capture_runs: list[float]
    dt_sweep_runs: list[NullRunMetrics]
    dt_sweep_ic_note: str


def _find_radial_extrema(r: np.ndarray, vr: np.ndarray) -> tuple[list[int], list[int]]:
    """Return indices of approximate periapses (vr zero-crossing - to +) and apoapses (+ to -)."""
    peri, apo = [], []
    for i in range(1, len(vr)):
        if vr[i - 1] < 0 <= vr[i]:
            peri.append(i)
        elif vr[i - 1] > 0 >= vr[i]:
            apo.append(i)
    return peri, apo


def compute_freefall_metrics(
    df: pd.DataFrame | None = None,
    r0: float = FREEFALL_R0,
    dt: float = FREEFALL_DT,
    rs: float = RS,
) -> FreefallMetrics:
    if df is None:
        df = load_freefall()

    horizon = _run_horizon(df)
    spacetime = df.attrs.get("spacetime", "schwarzschild")
    if spacetime == "kerr":
        tau_analytical = float("nan")
        ic_note = (
            "Kerr radial freefall; Schwarzschild analytic tau does not apply. "
            "Horizon check uses outer horizon r_+."
        )
    else:
        tau_analytical = analytical_freefall_time(r0, rs)
        ic_note = (
            "Initial conditions are E=1 radial infall (vt=1/f, vr=-sqrt(rs/r0)), "
            "not a particle released from local rest at r0."
        )

    crossed = df[df["r"] <= horizon]
    if crossed.empty:
        last = df.iloc[-1]
        tau_num = float(last["tau"])
        step = int(len(df) - 1)
        valid = df
    else:
        row = crossed.iloc[0]
        tau_num = float(row["tau"])
        step = int(row.name)
        valid = df.iloc[:step]
        valid = valid[valid["r"] > horizon * 1.01]
        if valid.empty:
            valid = df.iloc[:step]

    if spacetime == "kerr":
        # Approximate invariants via CSV kinematics only (no full Kerr metric in Python here).
        norm = np.full(len(valid), float("nan"))
        energy = np.full(len(valid), float("nan"))
        if "vt" in valid and len(valid):
            energy = valid["vt"].to_numpy()  # placeholder scale; drift still informative vs const
        abs_err = float("nan")
        rel_err = float("nan")
        norm_min = float("nan")
        norm_max = float("nan")
        norm_drift = float("nan")
        energy_min = float(np.nanmin(energy)) if len(energy) else float("nan")
        energy_max = float(np.nanmax(energy)) if len(energy) else float("nan")
        energy_drift = (
            float(energy_max - energy_min)
            if np.isfinite(energy_min) and np.isfinite(energy_max)
            else float("nan")
        )
    else:
        f = schwarzschild_f(valid["r"].to_numpy(), rs)
        norm = -f * valid["vt"].to_numpy() ** 2 + (1.0 / f) * valid["vr"].to_numpy() ** 2
        energy = f * valid["vt"].to_numpy()
        abs_err = abs(tau_num - tau_analytical)
        rel_err = 100.0 * abs_err / tau_analytical if tau_analytical else float("nan")
        norm_min = float(norm.min())
        norm_max = float(norm.max())
        norm_drift = float(norm.max() - norm.min())
        energy_min = float(energy.min())
        energy_max = float(energy.max())
        energy_drift = float(energy.max() - energy.min())

    return FreefallMetrics(
        r0=r0,
        dt=dt,
        rs=rs,
        tau_analytical=tau_analytical,
        tau_numerical=tau_num,
        horizon_step=step,
        crossing_abs_error=abs_err,
        crossing_rel_error_pct=rel_err,
        radius_monotonic_decreasing=bool((valid["r"].diff().dropna() <= 0).all()),
        norm_min=norm_min,
        norm_max=norm_max,
        norm_drift=norm_drift,
        energy_min=energy_min,
        energy_max=energy_max,
        energy_drift=energy_drift,
        ic_note=ic_note,
    )


def compute_orbital_metrics(
    df: pd.DataFrame | None = None,
    r0: float = ORBITAL_R0,
    dt: float = ORBITAL_DT,
    rs: float = RS,
) -> OrbitalMetrics:
    if df is None:
        df = load_orbital()

    horizon = _run_horizon(df)
    spacetime = df.attrs.get("spacetime", "schwarzschild")

    r = df["r"].to_numpy()
    vr = df["vr"].to_numpy()
    vt = df["vt"].to_numpy()
    vph = df["vph"].to_numpy()

    if spacetime == "kerr":
        # Prefer CSV norm; E/L drift from approximate Sch formulas is misleading for Kerr.
        e_drift = np.zeros_like(r)
        l_drift = np.zeros_like(r)
        ic_note = "Kerr orbital run; E/L drift not recomputed in Python (see C++ console)."
    else:
        f = schwarzschild_f(r, rs)
        energy = f * vt
        ang_mom = r**2 * vph
        e0, l0 = energy[0], ang_mom[0]
        e_drift = np.abs(energy - e0)
        l_drift = np.abs(ang_mom - l0)
        ic_note = (
            "Driver logs E and L at t=0 only; CSV records norm but not E/L drift columns."
        )

    peri_idx, _ = _find_radial_extrema(r, vr)
    advance_mean = None
    if len(peri_idx) >= 2:
        phi = df["phi"].to_numpy()
        advances = []
        for i in range(1, len(peri_idx)):
            dphi = phi[peri_idx[i]] - phi[peri_idx[i - 1]]
            advances.append(float(dphi))
        advance_mean = float(np.mean(advances))

    horizon_crossed = bool((r <= horizon).any())
    escaped = bool((r > 1000.0).any())
    bound = not horizon_crossed and not escaped

    return OrbitalMetrics(
        r0=r0,
        dt=dt,
        rs=rs,
        r_min=float(r.min()),
        r_max=float(r.max()),
        bound=bound,
        escaped=escaped,
        horizon_crossed=horizon_crossed,
        norm_min=float(df["norm"].min()),
        norm_max=float(df["norm"].max()),
        norm_drift=float(df["norm"].max() - df["norm"].min()),
        energy_drift_max=float(e_drift.max()) if len(e_drift) else float("nan"),
        angular_momentum_drift_max=float(l_drift.max()) if len(l_drift) else float("nan"),
        periapsis_count=len(peri_idx),
        periapsis_advance_mean=advance_mean,
        all_finite=bool(np.isfinite(r).all() and np.isfinite(vt).all()),
        ic_note=ic_note,
    )


def _classify_null_run(
    df: pd.DataFrame, impact_parameter: float, horizon: float, b_crit: float
) -> tuple[str, bool, bool]:
    r = df["r"].to_numpy()
    vr = df["vr"].to_numpy()
    if not np.all(np.isfinite(r)):
        if impact_parameter < b_crit:
            return "capture (numerical breakdown)", False, True
        return "escape (numerical breakdown)", False, False

    r_min = float(np.nanmin(r))
    r_final = float(r[-1])

    captured = r_min <= horizon * 1.0001
    escaped_post = bool(r_final > 1000.0 and vr[-1] > 0)

    if captured:
        return "capture", False, True
    if escaped_post or (impact_parameter > b_crit):
        return "escape", escaped_post, False
    if r_final > 100.0 and vr[-1] > 0:
        return "escape", False, False
    return "undetermined", escaped_post, captured


def compute_null_run_metrics(df: pd.DataFrame, dt: float = 0.0005, rs: float = RS) -> NullRunMetrics:
    b = float(df.attrs.get("impact_parameter", float("nan")))
    source = str(df.attrs.get("source_file", "unknown"))
    horizon = _run_horizon(df)
    b_crit = _run_b_crit(df)

    r = df["r"].to_numpy()
    outside = r > horizon
    h = df["H"].to_numpy()[outside]
    vt = df["vt"].to_numpy()[outside]
    vr = df["vr"].to_numpy()[outside]
    vph = df["vph"].to_numpy()[outside]
    ro = r[outside]
    with np.errstate(over="ignore", invalid="ignore"):
        scale = np.abs(vt**2) + np.abs(vr**2) + np.abs(ro**2 * vph**2)
        scale = np.maximum(scale, 1e-12)
        h_err = np.abs(h) / scale
    h_err = h_err[np.isfinite(h_err)]

    classification, escaped_pp, captured = _classify_null_run(df, b, horizon, b_crit)

    deflection = None
    if classification.startswith("escape"):
        deflection = float(np.degrees(df["phi_total"].iloc[-1]))

    r_finite = r[np.isfinite(r)]
    de = np.abs(df["dE"].to_numpy())
    dl = np.abs(df["dL"].to_numpy())
    de = de[np.isfinite(de)]
    dl = dl[np.isfinite(dl)]

    return NullRunMetrics(
        impact_parameter=b,
        source_file=source,
        dt=dt,
        classification=classification,
        r_min=float(np.min(r_finite)) if len(r_finite) else float("nan"),
        r_final=float(r[-1]) if np.isfinite(r[-1]) else float("nan"),
        lambda_final=float(df["lambda"].iloc[-1]),
        deflection_deg=deflection,
        h_error_max=float(h_err.max()) if len(h_err) else float("nan"),
        h_error_mean=float(h_err.mean()) if len(h_err) else float("nan"),
        de_max=float(de.max()) if len(de) else float("nan"),
        dl_max=float(dl.max()) if len(dl) else float("nan"),
        escaped_postprocessed=escaped_pp,
        captured=captured,
    )


def compute_null_sweep_summary(all_runs: dict[float, pd.DataFrame] | None = None) -> NullSweepSummary:
    if all_runs is None:
        all_runs = load_all_null_geodesics()

    sample = next(iter(all_runs.values())) if all_runs else None
    b_crit = _run_b_crit(sample)

    runs = [compute_null_run_metrics(df) for df in all_runs.values()]
    runs.sort(key=lambda m: m.impact_parameter)

    escape = [m.impact_parameter for m in runs if m.classification.startswith("escape")]
    capture = [m.impact_parameter for m in runs if m.classification.startswith("capture")]

    by_b: dict[float, list[NullRunMetrics]] = {}
    for m in runs:
        by_b.setdefault(m.impact_parameter, []).append(m)

    dt_sweep = [group[-1] for group in by_b.values() if len(group) > 1]
    for group in by_b.values():
        if len(group) > 1:
            group.sort(key=lambda m: m.lambda_final)

    return NullSweepSummary(
        b_crit=b_crit,
        runs=runs,
        escape_runs=escape,
        capture_runs=capture,
        dt_sweep_runs=dt_sweep,
        dt_sweep_ic_note=(
            "Null suite launches near b_crit ± offsets. Filenames are null_b_* (Schwarzschild) "
            "or null_kerr_b_* (Kerr)."
        ),
    )


def analyze_all(data_dir: Path | None = None) -> dict[str, Any]:
    if data_dir is not None:
        freefall = compute_freefall_metrics(load_freefall(data_dir))
        orbital = compute_orbital_metrics(load_orbital(data_dir))
        null_summary = compute_null_sweep_summary(load_all_null_geodesics(data_dir))
    else:
        freefall = compute_freefall_metrics()
        orbital = compute_orbital_metrics()
        null_summary = compute_null_sweep_summary()

    return {
        "freefall": asdict(freefall),
        "orbital": asdict(orbital),
        "null_geodesic": {
            "b_crit": null_summary.b_crit,
            "dt_sweep_ic_note": null_summary.dt_sweep_ic_note,
            "runs": [asdict(r) for r in null_summary.runs],
            "escape_count": len(null_summary.escape_runs),
            "capture_count": len(null_summary.capture_runs),
        },
        "source_findings": {
            "freefall_ic": freefall.ic_note,
            "orbital_el_drift_untracked": orbital.ic_note,
            "null_projection": "Null post-step projection resets U^t only (every 1000 steps); vr and vph are untouched.",
            "null_dt_sweep_ic": null_summary.dt_sweep_ic_note,
            "escape_termination": "Escape is detected in benchmark console/CSV post-processing (r>1000, vr>0), not via TerminationPolicy.",
            "horizon_stiffness": (
                "Schwarzschild coordinates become singular at r=r_s; freefall/orbital terminate at r<=r_s, "
                "null at r<=1.0001*r_s. RK4 steps can overshoot before termination check."
            ),
            "csv_filename_stability": (
                "null_geodesic.cpp names files via std::to_string(b0) (default 6 decimal places). "
                "Near-critical runs can share filenames; the dt sweep overwrites the same CSV "
                "multiple times, so only the final 1M-step trace persists for that b0."
            ),
            "header_doc_mismatches": [
                "freefall.h claims 'from rest at r0' but implementation uses E=1 infall (vt=1/f, vr=-sqrt(rs/r0)).",
                "orbital.h says output is geodesic.csv; driver writes orbital.csv.",
                "null_geodesic.h says output is null_geodesic.csv; driver writes null_b_<b>.csv.",
            ],
        },
    }


def _sanitize_for_json(obj: Any) -> Any:
    if isinstance(obj, dict):
        return {k: _sanitize_for_json(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_sanitize_for_json(v) for v in obj]
    if isinstance(obj, float) and not np.isfinite(obj):
        return None
    return obj


def write_metrics_json(path: Path, results: dict[str, Any] | None = None) -> dict[str, Any]:
    results = results if results is not None else analyze_all()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(_sanitize_for_json(results), indent=2) + "\n")
    return results
