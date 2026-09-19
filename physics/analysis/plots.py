"""Matplotlib batch plotting for benchmark validation figures."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from .config import (
    B_CRIT,
    FREEFALL_DT,
    FREEFALL_R0,
    NULL_R0,
    ORBITAL_DT,
    ORBITAL_R0,
    ORBITAL_VPH,
    ORBITAL_VR,
    PHOTON_SPHERE_R,
    RS,
    benchmark_data_dir,
    kerr_critical_impact_parameter,
    kerr_outer_horizon,
    kerr_photon_sphere_radius,
)
from .loaders import is_kerr_run, load_all_null_geodesics, load_freefall, load_orbital
from .metrics import analytical_radius_freefall, schwarzschild_f
from .style import COLORS, apply_style, draw_horizon_circle, mark_start_end, save_figure


def _geometry(data_dir: Path | None) -> tuple[float, float, float, str]:
    root = data_dir or benchmark_data_dir()
    if is_kerr_run(root):
        return (
            kerr_outer_horizon(),
            kerr_photon_sphere_radius(),
            kerr_critical_impact_parameter(),
            "kerr",
        )
    return RS, PHOTON_SPHERE_R, B_CRIT, "schwarzschild"


def _pick_null_representatives(
    all_runs: dict[float, object], horizon: float
) -> tuple[float, float, str, str]:
    above = sorted(b for b, df in all_runs.items() if float(df["r"].min()) > horizon * 1.0001)
    below = sorted(
        (b for b, df in all_runs.items() if float(df["r"].min()) <= horizon * 1.0001),
        reverse=True,
    )
    if above and below:
        return above[0], below[0], "escape side", "capture side"

    bs = sorted(all_runs)
    if len(bs) < 2:
        raise RuntimeError(
            "Need at least two null geodesic CSVs to plot escape/capture representatives. "
            f"Found b values: {bs}"
        )
    return bs[-1], bs[0], "largest b", "smallest b"



def plot_orbital(out_dir: Path, data_dir: Path | None = None) -> list[tuple[str, str]]:
    apply_style()
    df = load_orbital(data_dir)
    horizon, _, _, spacetime = _geometry(data_dir)
    metric_label = "Kerr" if spacetime == "kerr" else "Schwarzschild"
    saved: list[tuple[str, str]] = []

    x = df["r"] * np.cos(df["phi"])
    y = df["r"] * np.sin(df["phi"])

    # --- Top view ---
    fig, ax = plt.subplots(figsize=(6.5, 6.5))
    draw_horizon_circle(ax, rs=horizon, label=r"Horizon")
    ax.plot(x, y, color=COLORS["trajectory"], lw=1.2, label="Numerical orbit")
    mark_start_end(ax, x.iloc[0], y.iloc[0], x.iloc[-1], y.iloc[-1])
    ax.set_aspect("equal")
    ax.set_xlabel(r"$x$ [$r_s$]")
    ax.set_ylabel(r"$y$ [$r_s$]")
    ax.set_title(
        f"Orbital benchmark — {metric_label} (equatorial plane)\n"
        rf"$r_0={ORBITAL_R0}\,r_s$, $v_r={ORBITAL_VR}$, $v_\phi={ORBITAL_VPH}$, $\Delta\tau={ORBITAL_DT}$"
    )
    ax.legend(loc="upper right", framealpha=0.9)
    saved.append(save_figure(fig, "orbital_top_view", out_dir))

    # --- Radius and velocity vs proper time ---
    fig, axes = plt.subplots(2, 1, figsize=(7.5, 6.5), sharex=True)
    tau = df["tau"]
    axes[0].plot(tau, df["r"], color=COLORS["trajectory"], label=r"$r(\tau)$")
    axes[0].axhline(horizon, color=COLORS["horizon"], ls="--", lw=1.0, label=r"horizon")
    axes[0].set_ylabel(r"$r$ [$r_s$]")
    axes[0].legend(loc="upper right")
    axes[0].set_title("Orbital radius and velocity components")

    axes[1].plot(tau, df["vr"], label=r"$v^r$", color=COLORS["trajectory"])
    axes[1].plot(tau, df["vph"], label=r"$v^\phi$", color=COLORS["secondary"])
    axes[1].set_xlabel(r"Proper time $\tau$ [$r_s/c$]")
    axes[1].set_ylabel("Velocity components")
    axes[1].legend(loc="upper right")
    saved.append(save_figure(fig, "orbital_radius_velocity", out_dir))

    # --- Invariant / error panel ---
    r = df["r"].to_numpy()
    fig, axes = plt.subplots(3, 1, figsize=(7.5, 7.0), sharex=True)
    axes[0].plot(tau, df["norm"], color=COLORS["trajectory"])
    axes[0].axhline(-1.0, color=COLORS["analytic"], ls=":", lw=1.0, label=r"$g_{\mu\nu} U^\mu U^\nu = -1$")
    axes[0].set_ylabel("Norm")
    axes[0].legend(loc="upper right")
    axes[0].set_title("Massive geodesic constraint and conserved quantities (post-hoc)")

    if spacetime == "kerr":
        axes[1].plot(tau, np.zeros_like(r), color=COLORS["trajectory"])
        axes[1].set_ylabel(r"$E$ drift (n/a)")
        axes[2].plot(tau, np.zeros_like(r), color=COLORS["secondary"])
        axes[2].set_ylabel(r"$L$ drift (n/a)")
    else:
        f = schwarzschild_f(r)
        energy = f * df["vt"].to_numpy()
        ang_mom = (r**2) * df["vph"].to_numpy()
        e0, l0 = energy[0], ang_mom[0]
        axes[1].plot(tau, energy - e0, color=COLORS["trajectory"])
        axes[1].set_ylabel(r"$E - E_0$")
        axes[2].plot(tau, ang_mom - l0, color=COLORS["secondary"])
        axes[2].set_ylabel(r"$L - L_0$")
    axes[2].set_xlabel(r"Proper time $\tau$ [$r_s/c$]")
    saved.append(save_figure(fig, "orbital_invariants", out_dir))

    return saved


def plot_freefall(out_dir: Path, data_dir: Path | None = None) -> list[tuple[str, str]]:
    apply_style()
    df = load_freefall(data_dir)
    saved: list[tuple[str, str]] = []

    # Radial path in equatorial plane (motion along +x).
    x = df["r"].to_numpy()
    y = np.zeros_like(x)

    fig, ax = plt.subplots(figsize=(7.0, 3.5))
    draw_horizon_circle(ax, label=r"Horizon ($r=r_s$)")
    ax.plot(x, y, color=COLORS["trajectory"], lw=1.4, label="Numerical infall")
    mark_start_end(ax, x[0], y[0], x[-1], y[-1])
    ax.set_aspect("equal")
    ax.set_xlabel(r"$x$ [$r_s$]")
    ax.set_ylabel(r"$y$ [$r_s$]")
    ax.set_title(
        f"Free-fall benchmark: E=1 radial infall\n"
        rf"$r_0={FREEFALL_R0}\,r_s$, $\Delta\tau={FREEFALL_DT}$ (not local rest)"
    )
    ax.legend(loc="upper right")
    saved.append(save_figure(fig, "freefall_radial_path", out_dir))

    # Numerical vs analytic radius.
    tau = df["tau"].to_numpy()
    r_analytic = analytical_radius_freefall(tau, FREEFALL_R0, RS)

    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    ax.plot(tau, df["r"], label="Numerical", color=COLORS["trajectory"])
    ax.plot(tau, r_analytic, label="Analytic (E=1 infall)", color=COLORS["analytic"], ls="--")
    ax.axhline(RS, color=COLORS["horizon"], ls=":", lw=1.0, label=r"$r_s$")
    ax.set_xlabel(r"Proper time $\tau$ [$r_s/c$]")
    ax.set_ylabel(r"Radius $r$ [$r_s$]")
    ax.set_title("Free-fall radius: numerical vs analytic")
    ax.legend()
    saved.append(save_figure(fig, "freefall_radius_comparison", out_dir))

    # Velocities and invariants.
    r = df["r"].to_numpy()
    valid = r > RS
    f = schwarzschild_f(r[valid], RS)
    norm = -f * df["vt"].to_numpy()[valid] ** 2 + (1.0 / f) * df["vr"].to_numpy()[valid] ** 2
    energy = f * df["vt"].to_numpy()[valid]
    # Coordinate acceleration proxy: d(vr)/d(tau) from finite differences.
    dvr_dtau = np.gradient(df["vr"].to_numpy(), tau)

    fig, axes = plt.subplots(3, 1, figsize=(7.5, 7.0), sharex=True)
    axes[0].plot(tau, df["vr"], label=r"$v^r$", color=COLORS["trajectory"])
    axes[0].plot(tau, df["vt"], label=r"$v^t$", color=COLORS["secondary"])
    axes[0].set_ylabel("Velocity")
    axes[0].legend(loc="upper right")
    axes[0].set_title("Free-fall velocities and constraint diagnostics")

    axes[1].plot(tau, dvr_dtau, color=COLORS["trajectory"])
    axes[1].set_ylabel(r"$dv^r/d\tau$")

    axes[2].plot(tau[valid], norm + 1.0, label=r"Norm $+1$", color=COLORS["trajectory"])
    axes[2].plot(tau[valid], energy - 1.0, label=r"$E-1$", color=COLORS["analytic"])
    axes[2].set_ylabel("Drift")
    axes[2].set_xlabel(r"Proper time $\tau$ [$r_s/c$]")
    axes[2].legend(loc="upper right")
    saved.append(save_figure(fig, "freefall_velocity_invariants", out_dir))

    return saved


def _plot_null_xy(
    df, title: str, b: float, out_stem: str, out_dir: Path, horizon: float, photon_r: float, b_crit: float
) -> tuple[str, str]:
    x = df["r"] * np.cos(df["phi"])
    y = df["r"] * np.sin(df["phi"])
    r_min = float(df["r"].min())

    fig, ax = plt.subplots(figsize=(6.5, 6.5))
    draw_horizon_circle(ax, rs=horizon, label=r"Horizon")
    theta = np.linspace(0, 2 * np.pi, 256)
    ax.plot(
        photon_r * np.cos(theta),
        photon_r * np.sin(theta),
        color=COLORS["photon_sphere"],
        ls=":",
        lw=1.0,
        label=r"Photon sphere",
    )
    ax.plot(x, y, color=COLORS["trajectory"], lw=1.0, label="Null geodesic")
    mark_start_end(ax, x.iloc[0], y.iloc[0], x.iloc[-1], y.iloc[-1])
    ax.scatter([0], [0], s=20, c=COLORS["horizon"], marker="x", zorder=4)

    idx_min = int(df["r"].idxmin())
    ax.scatter([x.iloc[idx_min]], [y.iloc[idx_min]], s=36, c=COLORS["analytic"], marker="*",
               zorder=5, label=rf"Closest approach $r_{{min}}={r_min:.3f}\,r_s$")

    ax.annotate(
        rf"$b={b:.6f}\,r_s$" + "\n" + rf"$b_{{crit}}={b_crit:.6f}\,r_s$",
        xy=(0.02, 0.98),
        xycoords="axes fraction",
        va="top",
        fontsize=9,
        bbox=dict(boxstyle="round", fc="white", alpha=0.85),
    )

    if "escape" in title.lower():
        bend = float(np.degrees(df["phi_total"].iloc[-1]))
        ax.annotate(
            rf"$\Delta\phi_{{tot}}\approx{bend:.1f}^\circ$",
            xy=(x.iloc[-1], y.iloc[-1]),
            xytext=(12, 12),
            textcoords="offset points",
            fontsize=9,
            arrowprops=dict(arrowstyle="->", lw=0.8),
        )

    ax.set_aspect("equal")
    ax.set_xlabel(r"$x$ [$r_s$]")
    ax.set_ylabel(r"$y$ [$r_s$]")
    ax.set_title(title)
    ax.legend(loc="upper right", fontsize=8, framealpha=0.9)
    return save_figure(fig, out_stem, out_dir)


def _plot_null_radius_velocity(
    df, stem: str, b: float, out_dir: Path, horizon: float, photon_r: float
) -> tuple[str, str]:
    lam = df["lambda"]
    fig, axes = plt.subplots(2, 1, figsize=(7.5, 6.0), sharex=True)
    axes[0].plot(lam, df["r"], color=COLORS["trajectory"])
    axes[0].axhline(horizon, color=COLORS["horizon"], ls="--", lw=1.0, label=r"horizon")
    axes[0].axhline(photon_r, color=COLORS["photon_sphere"], ls=":", lw=1.0, label=r"photon sphere")
    axes[0].set_ylabel(r"$r$ [$r_s$]")
    axes[0].legend(loc="upper right")
    axes[0].set_title(rf"Null geodesic ($b={b:.6f}\,r_s$): radius and velocities")

    axes[1].plot(lam, df["vr"], label=r"$v^r$", color=COLORS["trajectory"])
    axes[1].plot(lam, df["vph"], label=r"$v^\phi$", color=COLORS["secondary"])
    axes[1].set_xlabel(r"Affine parameter $\lambda$ [$r_s/c$]")
    axes[1].set_ylabel("Velocity components")
    axes[1].legend(loc="upper right")
    return save_figure(fig, stem, out_dir)



def _plot_null_constraints(df, stem: str, b: float, out_dir: Path) -> tuple[str, str]:
    r = df["r"].to_numpy()
    h = df["H"].to_numpy()
    scale = np.abs(df["vt"].to_numpy() ** 2) + np.abs(df["vr"].to_numpy() ** 2)
    scale += np.abs(r**2 * df["vph"].to_numpy() ** 2) + 1e-12
    h_err = np.abs(h) / scale

    fig, axes = plt.subplots(3, 1, figsize=(7.5, 7.0), sharex=True)
    axes[0].plot(df["lambda"], h, color=COLORS["trajectory"])
    axes[0].axhline(0.0, color=COLORS["analytic"], ls=":", lw=1.0)
    axes[0].set_ylabel(r"Null Hamiltonian $H$")
    axes[0].set_title(rf"Null constraint and invariant drift ($b={b:.6f}\,r_s$)")

    axes[1].plot(df["lambda"], h_err, color=COLORS["trajectory"])
    axes[1].set_ylabel(r"$|H|/\mathrm{scale}$")

    axes[2].plot(df["lambda"], df["dE"], label=r"$\delta E/E_0$", color=COLORS["trajectory"])
    axes[2].plot(df["lambda"], df["dL"], label=r"$\delta L/L_0$", color=COLORS["secondary"])
    axes[2].set_xlabel(r"Affine parameter $\lambda$ [$r_s/c$]")
    axes[2].set_ylabel("Relative drift")
    axes[2].legend(loc="upper right")
    return save_figure(fig, stem, out_dir)


def plot_null_geodesic(out_dir: Path, data_dir: Path | None = None) -> list[tuple[str, str]]:
    apply_style()
    all_runs = load_all_null_geodesics(data_dir)
    horizon, photon_r, b_crit, spacetime = _geometry(data_dir)
    b_hi, b_lo, hi_label, lo_label = _pick_null_representatives(all_runs, horizon)
    saved: list[tuple[str, str]] = []

    df_hi = all_runs[b_hi]
    df_lo = all_runs[b_lo]
    metric = "Kerr" if spacetime == "kerr" else "Schwarzschild"

    saved.append(
        _plot_null_xy(
            df_hi,
            rf"Null geodesic — {hi_label}\n{metric}, $r_0={NULL_R0}\,r_s$",
            b_hi,
            "null_escape_trajectory",
            out_dir,
            horizon,
            photon_r,
            b_crit,
        )
    )
    saved.append(
        _plot_null_xy(
            df_lo,
            rf"Null geodesic — {lo_label}\n{metric}, $r_0={NULL_R0}\,r_s$",
            b_lo,
            "null_capture_trajectory",
            out_dir,
            horizon,
            photon_r,
            b_crit,
        )
    )
    saved.extend(
        [
            _plot_null_radius_velocity(
                df_hi, "null_escape_radius_velocity", b_hi, out_dir, horizon, photon_r
            ),
            _plot_null_constraints(df_hi, "null_escape_constraints", b_hi, out_dir),
            _plot_null_radius_velocity(
                df_lo, "null_capture_radius_velocity", b_lo, out_dir, horizon, photon_r
            ),
            _plot_null_constraints(df_lo, "null_capture_constraints", b_lo, out_dir),
        ]
    )

    bs = sorted(all_runs)
    r_mins = [float(all_runs[b]["r"].min()) for b in bs]
    fig, ax = plt.subplots(figsize=(7.0, 4.5))
    colors = [COLORS["escape"] if b > b_crit else COLORS["capture"] for b in bs]
    ax.scatter(bs, r_mins, c=colors, s=36, zorder=3)
    ax.axvline(b_crit, color=COLORS["horizon"], ls="--", lw=1.0, label=rf"$b_{{crit}}={b_crit:.4f}$")
    ax.axhline(horizon, color=COLORS["horizon"], ls=":", lw=1.0, label=r"horizon")
    ax.axhline(photon_r, color=COLORS["photon_sphere"], ls=":", lw=1.0, label=r"photon sphere")
    ax.set_xlabel(r"Impact parameter $b$ [$r_s$]")
    ax.set_ylabel(r"Closest approach $r_{min}$ [$r_s$]")
    ax.set_title(f"{metric} null geodesic impact-parameter sweep")
    ax.legend(loc="upper right")
    saved.append(save_figure(fig, "null_impact_parameter_sweep", out_dir))

    return saved


def plot_all(out_dir: Path, data_dir: Path | None = None) -> list[tuple[str, str]]:
    saved: list[tuple[str, str]] = []
    saved.extend(plot_orbital(out_dir, data_dir))
    saved.extend(plot_freefall(out_dir, data_dir))
    saved.extend(plot_null_geodesic(out_dir, data_dir))
    return saved
