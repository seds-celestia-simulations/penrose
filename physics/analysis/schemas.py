"""CSV column schemas for the three benchmark drivers."""

from dataclasses import dataclass
from typing import Sequence


@dataclass(frozen=True)
class BenchmarkSchema:
    name: str
    required_columns: tuple[str, ...]
    # Preferred filenames in discovery order (first existing wins).
    filenames: tuple[str, ...] = ()
    filename_globs: tuple[str, ...] = ()

    @property
    def filename(self) -> str | None:
        return self.filenames[0] if self.filenames else None

    @property
    def filename_glob(self) -> str | None:
        return self.filename_globs[0] if self.filename_globs else None


FREEFALL_SCHEMA = BenchmarkSchema(
    name="freefall",
    required_columns=("tau", "r", "vt", "vr"),
    filenames=("freefall.csv", "freefall_kerr.csv"),
)

ORBITAL_SCHEMA = BenchmarkSchema(
    name="orbital",
    required_columns=("tau", "r", "phi", "vt", "vr", "vph", "norm"),
    filenames=("orbital.csv", "orbital_kerr.csv"),
)

NULL_GEODESIC_SCHEMA = BenchmarkSchema(
    name="null_geodesic",
    required_columns=(
        "lambda",
        "r",
        "phi",
        "vt",
        "vr",
        "vph",
        "H",
        "E",
        "L",
        "dE",
        "dL",
        "dvt",
        "dvph",
        "phi_total",
    ),
    filename_globs=("null_b_*.csv", "null_kerr_b_*.csv"),
)


ALL_SCHEMAS: dict[str, BenchmarkSchema] = {
    FREEFALL_SCHEMA.name: FREEFALL_SCHEMA,
    ORBITAL_SCHEMA.name: ORBITAL_SCHEMA,
    NULL_GEODESIC_SCHEMA.name: NULL_GEODESIC_SCHEMA,
}


def validate_columns(columns: Sequence[str], schema: BenchmarkSchema) -> None:
    missing = [c for c in schema.required_columns if c not in columns]
    if missing:
        raise ValueError(
            f"CSV schema mismatch for '{schema.name}': missing columns {missing}. "
            f"Expected {list(schema.required_columns)}, got {list(columns)}."
        )
