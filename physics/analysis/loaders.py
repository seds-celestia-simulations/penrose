"""Schema-aware CSV loading for benchmark outputs."""

from __future__ import annotations

import re
from pathlib import Path

import pandas as pd

from .config import benchmark_data_dir
from .schemas import (
    FREEFALL_SCHEMA,
    NULL_GEODESIC_SCHEMA,
    ORBITAL_SCHEMA,
    BenchmarkSchema,
    validate_columns,
)

_NULL_FILENAME_RE = re.compile(r"null_(?:kerr_)?b_(.+)\.csv")


def _load_csv(path: Path, schema: BenchmarkSchema) -> pd.DataFrame:
    if not path.is_file():
        raise FileNotFoundError(
            f"Benchmark CSV not found: {path}\n"
            f"Run ./build/physics_benchmark from the repository root first."
        )
    if path.stat().st_size == 0:
        raise ValueError(f"Benchmark CSV is empty (still being written?): {path}")
    df = pd.read_csv(path)
    if df.empty:
        raise ValueError(f"Benchmark CSV has no data rows: {path}")
    validate_columns(df.columns, schema)
    return df


def resolve_benchmark_csv(data_dir: Path, schema: BenchmarkSchema) -> Path:
    """Pick the first existing filename candidate for a single-file schema."""
    if not schema.filenames:
        raise ValueError(f"Schema '{schema.name}' has no single-file candidates")
    tried: list[str] = []
    for name in schema.filenames:
        path = data_dir / name
        tried.append(name)
        if path.is_file() and path.stat().st_size > 0:
            return path
    raise FileNotFoundError(
        f"Benchmark CSV not found for '{schema.name}' in {data_dir}. "
        f"Tried: {tried}. Run ./build/physics_benchmark first."
    )


def is_kerr_run(data_dir: Path | None = None) -> bool:
    data_dir = data_dir or benchmark_data_dir()
    return any(data_dir.glob("*kerr*"))


def load_freefall(data_dir: Path | None = None) -> pd.DataFrame:
    data_dir = data_dir or benchmark_data_dir()
    path = resolve_benchmark_csv(data_dir, FREEFALL_SCHEMA)
    df = _load_csv(path, FREEFALL_SCHEMA)
    df.attrs["source_file"] = path.name
    df.attrs["spacetime"] = "kerr" if "kerr" in path.name else "schwarzschild"
    return df


def load_orbital(data_dir: Path | None = None) -> pd.DataFrame:
    data_dir = data_dir or benchmark_data_dir()
    path = resolve_benchmark_csv(data_dir, ORBITAL_SCHEMA)
    df = _load_csv(path, ORBITAL_SCHEMA)
    df.attrs["source_file"] = path.name
    df.attrs["spacetime"] = "kerr" if "kerr" in path.name else "schwarzschild"
    return df


def impact_parameter_from_filename(path: Path) -> float:
    match = _NULL_FILENAME_RE.fullmatch(path.name)
    if not match:
        raise ValueError(f"Cannot parse impact parameter from filename: {path.name}")
    return float(match.group(1))


def list_null_geodesic_csvs(data_dir: Path | None = None) -> list[Path]:
    data_dir = data_dir or benchmark_data_dir()
    paths: list[Path] = []
    seen: set[Path] = set()
    for pattern in NULL_GEODESIC_SCHEMA.filename_globs:
        for path in sorted(data_dir.glob(pattern)):
            if path not in seen:
                paths.append(path)
                seen.add(path)
    if not paths:
        raise FileNotFoundError(
            f"No null geodesic CSVs matching {NULL_GEODESIC_SCHEMA.filename_globs} "
            f"in {data_dir}. Run ./build/physics_benchmark first."
        )
    return paths


def load_null_geodesic(path: Path) -> pd.DataFrame:
    df = _load_csv(path, NULL_GEODESIC_SCHEMA)
    df.attrs["impact_parameter"] = impact_parameter_from_filename(path)
    df.attrs["source_file"] = path.name
    df.attrs["spacetime"] = "kerr" if "kerr" in path.name else "schwarzschild"
    return df


def load_all_null_geodesics(data_dir: Path | None = None) -> dict[float, pd.DataFrame]:
    data_dir = data_dir or benchmark_data_dir()
    out: dict[float, pd.DataFrame] = {}
    skipped: list[str] = []
    for path in list_null_geodesic_csvs(data_dir):
        try:
            df = load_null_geodesic(path)
        except (ValueError, pd.errors.EmptyDataError) as exc:
            skipped.append(f"{path.name} ({exc})")
            continue
        out[df.attrs["impact_parameter"]] = df
    if not out:
        raise FileNotFoundError(
            f"No readable null geodesic CSVs in {data_dir}."
            + (f" Skipped: {skipped}" if skipped else "")
        )
    return out
