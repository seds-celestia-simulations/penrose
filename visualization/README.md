# Penrose Visualization

Trajectory presentation for Penrose: headless CPU export and an interactive GPU polyline viewer. Independent of the GPU ray marcher in `realtime/`.

Physics-agnostic rendering: this library never constructs metrics or solvers and does not include physics headers. Entry points run simulations via `penrose_physics`, convert results in `run/adapter/`, then call `prepare_scene`.

## Pipeline

```text
SimulationRequest(s)  →  run_all  →  SimulationResult
                                          ↓
                          store_trajectories (run/adapter)
                                          ↓
                               prepare_scene (Stage 2)
                                          ↓
                               Scene + Camera → render (Stage 3)
```

Stage 2 is currently a pass-through. `VisualizationPreparationSettings` reserves knobs for future interpolation / resampling without touching the physics engine.

Stage 3 has two backends behind `TrajectoryRenderBackend`:

| Backend | Used by | Output |
|---------|---------|--------|
| `CpuRasterizerBackend` | `visualization_export` | PPM via `CPURasterizer` + optional `PostProcessor` |
| `GpuPolylineBackend` | `visualization_viewer` | OpenGL polylines into the GLFW framebuffer |

OpenGL code is confined to `visualization/Renderer/Gpu/` and the `visualization_gpu` static library (linked only by the viewer). Export does not require a display or OpenGL.

### GPU viewer scope (v1)

- Background clear + starfield (`visualization/resources/`) with mild rim warp
- Opaque filled event-horizon disc (billboard)
- Soft warm glow outside the disc + optional decorative photon-sphere ring (`show_photon_sphere`; two-pass GPU depth handling)
- Solid trajectory ribbons with along-path and distance fall-off
- Current-position markers and playback highlight
- Deferred: GPU bloom / cosmetic lensing matching CPU `PostProcessor`

`run/viewer` and `run/export` default to Kerr orbits; Schwarzschild blocks are commented for easy swap.

## User-facing workflows

| Executable | Config | Command |
|------------|--------|---------|
| `physics_benchmark` | [`run/benchmark/main.cpp`](../run/benchmark/main.cpp) | `./build/physics_benchmark` |
| `visualization_viewer` | [`run/viewer/main.cpp`](../run/viewer/main.cpp) | `./build/visualization_viewer` |
| `visualization_export` | [`run/export/main.cpp`](../run/export/main.cpp) | `./build/visualization_export` |

Full guide: [`docs/VISUALIZATION_GUIDE.md`](../docs/VISUALIZATION_GUIDE.md) · Architecture: [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md)

## Module layout

```text
visualization/
├── Preparation/    StoredTrajectory + prepare_scene (Stage 2)
├── Trajectory/     Chart-to-Cartesian adapt_states
├── Scene/          Scene graph, playback (multi-trajectory)
├── Camera/         Orbit / pan / zoom
├── Geometry/       Math, spherical→Cartesian, meshes
├── Renderer/       TrajectoryRenderBackend, CPU rasterizer, Gpu/ polyline backend
├── Presentation/   VisualizationConfig + optional post-process (CPU export)
├── IO/             PPM export, starfield (visualization/resources/)
├── Apps/           ViewerApp (GPU) + DisplayBlit window helper (penrose_glad)
├── scientific/     Compatibility shims → physics/analysis/
└── Tests/          Unit tests (`-DPENROSE_BUILD_TESTS=ON`)

visualization_gpu   (CMake static lib) GpuPolylineBackend — viewer only
run/adapter/        SimulationResult → StoredTrajectory bridge (linked by viewer/export)
```

### In-memory API (entry points)

```cpp
std::vector<Simulation::SimulationRequest> simulations = {orbit1, orbit2, orbit3};
auto trajectories = Simulation::run_all(simulations);

viz::VisualizationConfig viz;
// set viz.preparation / viz.scene / viz.camera / styles / ...

viz::Scene scene = viz::prepare_scene_from_results(trajectories, viz);
viz::Camera camera = viz::make_camera(scene, viz.camera);
viz::run_interactive_viewer(std::move(scene), camera, viz);
```

Per-particle styles: pass a parallel `std::span<const TrajectoryStyle>` to `prepare_scene_from_results`, or set `StoredTrajectory::use_style`.

Rules:

- Only `StoredTrajectory` crosses into the visualization library.
- Drawing / viz-resolution parameters come from `VisualizationConfig` in `run/*/main.cpp`.
- Trajectories are immutable after adaptation.
- Viewer and export entry points never load CSV.
- Benchmark analysis lives in `physics/analysis/` (not presentation).
- `realtime/` is not a dependency of visualization GPU code.
- Prefer tilted camera yaw/pitch under `auto_frame` so equatorial orbits are not edge-on.
