# Penrose Visualization — User Guide

Trajectory visualization exposes **three** production executables. Edit a `main.cpp` under `run/`, build, run. No CSV paths or CLI flags for viewer/export.

Architecture overview: [`ARCHITECTURE.md`](ARCHITECTURE.md).

---

## Workflows

| Executable | Config | Purpose |
|------------|--------|---------|
| `physics_benchmark` | [`run/benchmark/main.cpp`](../run/benchmark/main.cpp) | Validation → `outputs/benchmark_data/<timestamp>/` |
| `visualization_viewer` | [`run/viewer/main.cpp`](../run/viewer/main.cpp) | Integrate in memory → interactive GPU viewer |
| `visualization_export` | [`run/export/main.cpp`](../run/export/main.cpp) | Integrate in memory → CPU PPM still/sequence |

```text
Open run/<workflow>/main.cpp
        ↓
Edit SimulationRequest(s) + VisualizationConfig
        ↓
cmake --build build --target <executable>
        ↓
./build/<executable>
```

### Pipeline stages

```text
Physics (SimulationRequest per particle)
        ↓
Trajectory storage (SimulationResult)
        ↓
Visualization preparation (prepare_scene — pass-through today)
        ↓
Stage 3 rendering
  ├── visualization_viewer  → GpuPolylineBackend (OpenGL)
  └── visualization_export  → CpuRasterizerBackend (PPM)
```

### Three config layers

```cpp
// Layer 1+2 — one SimulationRequest per independent particle
Simulation::SimulationRequest orbit;
orbit.config.scenario  = Simulation::Scenario::BoundOrbit;
orbit.config.dt = 0.01;          // physics resolution

// Kerr (default in run/viewer and run/export):
orbit.config.spacetime = Simulation::SpacetimeKind::Kerr;
orbit.metric = Spacetime::KerrParameters{.mass = 1.0, .spin = 0.35};
orbit.initial = Simulation::BoundOrbitInitialConditions{ .r0 = 6.0, .vphi = 0.055 };

// Schwarzschild (commented alternative in run/*/main.cpp):
// orbit.config.spacetime = Simulation::SpacetimeKind::Schwarzschild;
// orbit.metric = Spacetime::SchwarzschildParameters{.mass = 1.0};
// orbit.initial = Simulation::BoundOrbitInitialConditions{ .r0 = 6.0, .vphi = 0.06 };

std::vector<Simulation::SimulationRequest> simulations = {orbit};
auto trajectories = Simulation::run_all(simulations);

// Layer 3 — visualization preparation + drawing (independent of physics dt)
viz::VisualizationConfig viz;
viz.preparation.interpolation_method = viz::InterpolationMethod::PassThrough;
viz.preparation.trajectory_resolution = 1.0f; // reserved for future resampling
// ... camera, presentation, trajectory_style, render ...

viz::Scene scene = viz::prepare_scene_from_results(trajectories, viz);
viz::Camera camera = viz::make_camera(scene, viz.camera);
```

Do **not** put metric-specific fields on `SimulationConfig`. Physics and visualization resolutions are separate. Multiple particles are simulated independently and overlaid only in `prepare_scene`. The visualization library accepts `StoredTrajectory` only — conversion from `SimulationResult` lives in `run/adapter/`. See [`ARCHITECTURE.md`](ARCHITECTURE.md) §2–§5.

Per-particle styles: pass a parallel `std::span<const TrajectoryStyle>` to `prepare_scene_from_results`, or set `StoredTrajectory::use_style`.

---

## Build once

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --target physics_benchmark visualization_viewer visualization_export
```

| Option | Default | Effect |
|--------|---------|--------|
| `PENROSE_BUILD_VIEWER` | `ON` | Build interactive GPU trajectory viewer |
| `PENROSE_BUILD_TESTS` | `OFF` | Internal unit tests |

---

## 1. Physics benchmark

```bash
cmake --build build --target physics_benchmark
./build/physics_benchmark
```

Writes CSVs under `outputs/benchmark_data/<timestamp>/`. Optional analysis:

```bash
python -m physics.analysis.analyze_benchmarks
python -m physics.analysis.plot_benchmarks
python -m physics.analysis.generate_report
```

(`python -m visualization.scientific.*` remains as a compatibility shim.)

---

## 2. Interactive viewer

```bash
cmake --build build --target visualization_viewer
./build/visualization_viewer
```

The viewer renders prepared trajectories with `GpuPolylineBackend` (`visualization_gpu` static library): starfield, opaque filled horizon disc, soft warm glow, optional decorative photon-sphere ring (two-pass GPU draw so FX stay visible), solid trails with distance fall-off, and markers. It does **not** run the CPU rasterizer each frame. Headless export uses `CpuRasterizerBackend` with matching disc/glow/ring painting — see §3.

`run/viewer/main.cpp` defaults to a **Kerr bound orbit** (`mass=1.0`, `spin=0.35`) with a commented Schwarzschild block for easy swap. Add more particles by pushing additional `SimulationRequest` entries into the `simulations` vector.

| Input | Action |
|-------|--------|
| Left drag | Orbit |
| Middle drag | Pan |
| Scroll | Zoom |
| Space | Play / pause |
| Left / Right | Scrub (~8% of duration per second while held) |
| Home / End | Jump to start / end (edge-triggered) |
| 1–9 | Select trajectory highlight |
| Escape | Quit |

Raise `playback_speed` in `run/viewer/main.cpp` to change play rate. Prefer a tilted `camera.yaw` / `auto_frame_pitch` so equatorial orbits are not viewed edge-on.

---

## 3. Export

```bash
cmake --build build --target visualization_export
./build/visualization_export
```

Uses `CpuRasterizerBackend` (`CPURasterizer` + optional `PostProcessor`) — no OpenGL or display required. Paints the same horizon disc / soft glow / optional photon-sphere ring as the GPU viewer. Defaults to the same Kerr bound orbit as `run/viewer` (Schwarzschild block commented for swap).

`frame_count = 1` → still; `>1` → `frame_XXXXXX.ppm` under `outputs/rendered_frames/<timestamp>/`.

GPU ray-march capture PPMs use a different tree (`imagesequence/`). Helpers: [`frame_capture/`](frame_capture/).

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| No display for viewer | Use export, or `-DPENROSE_BUILD_VIEWER=OFF` |
| Want freefall / bound orbit | Change `scenario` + IC type on each `SimulationRequest` in `run/viewer/main.cpp` |
| Add another particle | Push another `SimulationRequest` into the `simulations` vector |
| Benchmark CSVs missing | Run `./build/physics_benchmark` |
| Build fails looking for `examples/` | Entry points live under `run/` — reconfigure CMake from a current checkout |
| Orbit looks like a line | Increase `camera.auto_frame_pitch` / set a non-zero `camera.yaw` (tilted 3/4 view) |
