"""Deterministic Matplotlib styling for benchmark report figures."""

from __future__ import annotations

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np

from .config import RS

# Fixed output settings for reproducibility.
FIGURE_DPI = 150
SAVE_DPI = 300

COLORS = {
    "trajectory": "#2563eb",
    "analytic": "#dc2626",
    "horizon": "#111827",
    "photon_sphere": "#f59e0b",
    "start": "#16a34a",
    "end": "#9333ea",
    "grid": "#e5e7eb",
    "secondary": "#64748b",
    "capture": "#b91c1c",
    "escape": "#0369a1",
}


def apply_style() -> None:
    mpl.rcParams.update(
        {
            "figure.dpi": FIGURE_DPI,
            "savefig.dpi": SAVE_DPI,
            "font.size": 10,
            "axes.titlesize": 11,
            "axes.labelsize": 10,
            "legend.fontsize": 9,
            "xtick.labelsize": 9,
            "ytick.labelsize": 9,
            "axes.grid": True,
            "grid.alpha": 0.35,
            "grid.linestyle": "--",
            "lines.linewidth": 1.4,
            "figure.constrained_layout.use": True,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "text.usetex": False,
            "font.family": "DejaVu Sans",
            "savefig.bbox": "tight",
            "savefig.pad_inches": 0.05,
        }
    )


def save_figure(fig: plt.Figure, stem: str, out_dir) -> tuple[str, str]:
    out_dir.mkdir(parents=True, exist_ok=True)
    png = out_dir / f"{stem}.png"
    pdf = out_dir / f"{stem}.pdf"
    fig.savefig(png)
    fig.savefig(pdf)
    plt.close(fig)
    return str(png), str(pdf)


def draw_horizon_circle(ax, rs: float = RS, label: str = r"Event horizon ($r_s$)") -> None:
    theta = np.linspace(0, 2 * np.pi, 256)
    ax.plot(
        rs * np.cos(theta),
        rs * np.sin(theta),
        color=COLORS["horizon"],
        lw=1.2,
        ls="--",
        label=label,
    )


def mark_start_end(ax, x_start, y_start, x_end, y_end) -> None:
    ax.scatter([x_start], [y_start], s=48, c=COLORS["start"], marker="o", zorder=5, label="Start")
    ax.scatter([x_end], [y_end], s=48, c=COLORS["end"], marker="s", zorder=5, label="End")
