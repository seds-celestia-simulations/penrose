#!/usr/bin/env python3
"""Compatibility shim — delegates to physics.analysis.generate_report."""

from physics.analysis.generate_report import main

if __name__ == "__main__":
    raise SystemExit(main())
