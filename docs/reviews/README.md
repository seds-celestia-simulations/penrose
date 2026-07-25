# Architecture reviews

**Historical / non-normative.** These reviews describe feasibility and earlier layouts; they are not the source of truth for the current design.

For the current architecture, see [`../ARCHITECTURE.md`](../ARCHITECTURE.md). Agent conventions: [`../../AGENTS.md`](../../AGENTS.md).

## Status relative to current tree (2026-07)

| Topic | Current reality |
|-------|-----------------|
| CPU Schwarzschild | Production path end-to-end |
| CPU Kerr | Production path end-to-end (`MetricKind`, pipeline variant, CMake, benchmarks, analysis) |
| Realtime metrics | Compute-shader pipeline; **Kerr default** via `reduced.comp` includes; Schwarzschild paths selectable by include swap |
| Trajectory viz Stage 3 | Dual backends: `GpuPolylineBackend` viewer / `CpuRasterizerBackend` export under `run/` |
| Shared vocabulary in realtime | Still largely unused by `realtime/` sources (reviews remain directionally valid) |

| Document | Topic | Notes |
|----------|--------|-------|
| [`shared_architecture_review.md`](shared_architecture_review.md) | Feasibility of shared GR abstractions vs dual pipelines | Dated; predates `run/` entry points, dual Stage 3 backends, and Kerr scaffolding / realtime Kerr default |
| [`legacy_architecture_review.md`](legacy_architecture_review.md) | Pre-refactor layout review | Dated; describes earlier `src/`-style organization |
| [`realtime_architecture_review.md`](realtime_architecture_review.md) | Realtime backend vs framework architecture | Still useful on ownership boundaries; metric section predates Kerr-as-default shader path |
| [`realtime_roadmap.md`](realtime_roadmap.md) | Realtime evolution toward shared contracts | Directional; “add Kerr before fixing identity” risk is partially realized (Kerr via GLSL includes without shared `MetricType`) |
