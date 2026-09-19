#!/usr/bin/env python3
"""Compatibility shim — delegates to physics.analysis.plot_benchmarks."""

from physics.analysis.plot_benchmarks import main

if __name__ == "__main__":
    raise SystemExit(main())
