# Penrose Architecture

This is the **sole current architecture reference** for Penrose.

Penrose is a modular General Relativity framework: a CPU reference geodesic solver, a trajectory visualization stack (GPU interactive viewer + headless CPU export), and a separate GPU real-time compute ray-march renderer. The design goal is to add new spacetimes (Kerr, SGL, FLRW, …) by extending physics modules—not by rewriting entry points or a catch-all config struct.

**Metric status today:** Schwarzschild and Kerr are both production CPU paths (metrics, ICs, solver, benchmarks, analysis). The GPU realtime app defaults to Kerr via GLSL includes (`kerr_full.glsl`).

---

## 1. Pipelines

```text
                    shared/  (State, MetricKind, parameter vocabulary, units)
                           │
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
   penrose_physics/  visualization/     realtime/
     (CPU science)   (trajectory viz)   (GPU ray march)
```

| Pipeline | Role | Truth for |
|----------|------|-----------|
| **physics/** | Analytic metrics, dynamics, RK4, trajectory solve, validation | Scientific correctness |
| **visualization/** | Stored trajectories → preparation → scene → Stage 3 render backends | Publication / orbit illustrations |
| **realtime/** | OpenGL 4.3 + GLSL **compute** null geodesic ray march | Interactive lensing imagery |

Pipelines are intentionally loosely coupled. Visualization consumes stored `State` histories (never constructs metrics or solvers). Realtime does **not** share the CPU metric implementation; it remains a separate shader-resident path. Trajectory-viz GPU code (`visualization_gpu`) also does **not** include or link `realtime/`.

---

## 2. User-facing CPU workflow

Three production executables. Each is configured by editing a small `main.cpp` under `run/`, then building and running—no CLI flags required for viewer/export.

| Executable | Config | Role |
|------------|--------|------|
| `physics_benchmark` | [`run/benchmark/main.cpp`](../run/benchmark/main.cpp) | Validation suite → CSV under `outputs/benchmark_data/` |
| `visualization_viewer` | [`run/viewer/main.cpp`](../run/viewer/main.cpp) | In-memory integrate → GPU polyline viewer |
| `visualization_export` | [`run/export/main.cpp`](../run/export/main.cpp) | In-memory integrate → CPU PPM still/sequence |

```text
SimulationRequest(s)          // one per independent particle
        ↓
run_all / run_simulation      // Stage 1: physics accuracy only
        ↓
PhysicsTrajectory storage     // SimulationResult histories
        ↓
store_trajectories (run/adapter)  // consumer bridge — not in visualization lib
        ↓
prepare_scene                 // Stage 2: StoredTrajectory → Scene geometry
        ↓
Viewer / Export               // Stage 3: TrajectoryRenderBackend (GPU viewer / CPU export)
```

Entry points never construct concrete metrics (`SchwarzschildMetric`, …). Metrics are created inside the simulation pipeline. Multiple particles are simulated independently and overlaid only in visualization preparation.

GPU ray-march app: `./build/Penrose` — see [`RUNNING.md`](RUNNING.md).

---

## 3. Configuration layers

### Layer 1 — physics settings (`SimulationConfig`)

Spacetime-agnostic controls for **numerical correctness only**:

- `spacetime`, `scenario`, `geodesic`
- `dt`, `max_steps`, `horizon_safety_factor`
- `name`

Visualization resolution does **not** live here.

### Layer 2 — physics parameters

Metric- and scenario-specific numbers live in dedicated types:

| Kind | Example | Location |
|------|---------|----------|
| Metric params | `SchwarzschildParameters`, `KerrParameters` | `shared/metrics/` (POD vocabulary) |
| Initial conditions | `BoundOrbitInitialConditions`, … | `physics/simulation/initial_conditions/` |

Bundle them per particle as a `SimulationRequest` (`config` + `metric` variant + `initial` variant):

```cpp
std::vector<Simulation::SimulationRequest> simulations = {orbit1, orbit2};
auto trajectories = Simulation::run_all(simulations);
auto stored = viz::store_trajectories(trajectories);
auto scene = viz::prepare_scene(stored, viz);
// Or: viz::prepare_scene_from_results(trajectories, viz) in run/viewer and run/export
```

`SimulationRequest::metric` is `std::variant<SchwarzschildParameters, KerrParameters>`. Select spacetime with `config.spacetime` and the matching parameter type (or `make_schwarzschild_request` / `make_kerr_request`).

### Layer 3 — visualization preparation + drawing

| Concern | Type | Notes |
|---------|------|-------|
| Viz resolution | `VisualizationPreparationSettings` | `interpolation_method`, `render_samples_per_segment`, `trajectory_resolution` (pass-through today) |
| Drawing | `VisualizationConfig` | scene, camera, presentation, styles, render options |

Physics `dt` and visualization `trajectory_resolution` are independent concepts.

---

## 4. CPU scientific stack

```text
State { X, U }
    → Metric::christoffel(...)
    → GeodesicDynamics::compute_derivative(...)
    → Integrator::step(...)   // RK4 default
    → TrajectorySolver::solve(...)   // + TerminationPolicy
    → PhysicsTrajectory (SimulationResult + metadata)
```

Built as the `penrose_physics` static library (`CMakeLists.txt`). CPU consumers link it instead of duplicating source lists.

| Module | Responsibility |
|--------|----------------|
| `shared/state/GeodesicState.h` | `State` |
| `shared/spacetime/MetricKind.h` | `MetricKind`, `CoordinateChartKind` |
| `shared/metrics/SchwarzschildParameters.h` | Schwarzschild POD parameters |
| `shared/metrics/KerrParameters.h` | Kerr POD parameters (`mass` = `rs`, `spin` = `a`) |
| `shared/spacetime/Metric.h` | Narrow Christoffel evaluator interface (CPU today) |
| `physics/metrics/` | `SchwarzschildMetric`, `KerrMetric`; `CoordinateChart` |
| `physics/geodesics/` | `DynamicsModel`, `GeodesicDynamics` |
| `physics/integrators/` | `Integrator` interface, RK4 default |
| `physics/simulation/` | `TrajectorySolver`, `TerminationPolicy`, `SimulationConfig`, `SimulationRequest`, `SimulationPipeline`, Schwarzschild + Kerr IC builders |
| `physics/validation/` | Benchmark drivers (consumers of `run_simulation`) + `BenchmarkRunner` |
| `physics/validation/observables/` | Schwarzschild + Kerr invariant / observable helpers |
| `physics/analysis/` | Python benchmark analysis / figures / reports (discovers Sch or Kerr CSVs) |
| `physics/export/` | Benchmark CSV I/O helpers |

`SimulationPipeline` resolves metric + ICs from Layer 2 types and calls the solver. Validation drivers build `SimulationRequest` objects and call `run_simulation` — they no longer construct metrics/solvers directly. `run_all` integrates each `SimulationRequest` independently (no coupled multi-body integration).

---

## 5. Trajectory visualization stack

Three stages after physics:

```text
PhysicsTrajectory(s)          // storage — no render knobs
        ↓
store_trajectories (run/adapter)   // SimulationResult → StoredTrajectory + chart metadata
        ↓
prepare_scene(...)            // Stage 2: StoredTrajectory → Scene geometry
        ↓                     //   (interpolation / resampling hooks reserved)
make_camera(...)
        ↓
TrajectoryRenderBackend       // Stage 3: prepared geometry only
  ├── GpuPolylineBackend      // interactive viewer (OpenGL; visualization_gpu)
  └── CpuRasterizerBackend    // headless export (CPURasterizer + PostProcessor)
        ↓
GLFW window / PPM
```

| Module | Responsibility |
|--------|----------------|
| `visualization/Preparation/` | `StoredTrajectory` (with `CoordinateChartKind`), `prepare_scene` |
| `visualization/Trajectory/` | Chart-to-Cartesian `adapt_states` |
| `visualization/Scene/` | Multi-trajectory scene graph + playback |
| `visualization/Camera/` | Orbit / pan / zoom |
| `visualization/Renderer/` | `TrajectoryRenderBackend`, CPU rasterizer, `Gpu/` polyline backend |
| `visualization/Presentation/` | `VisualizationConfig`, bloom / cosmetic lensing (CPU export path) |
| `visualization/Apps/` | `ViewerApp` (GPU backend), `DisplayBlit` GLFW/GLAD window helper |
| `visualization/IO/` | PPM writer, output paths, starfield from `visualization/resources/` |
| `run/adapter/` | `store_trajectories`, `prepare_scene_from_results` (physics→viz bridge) |

The renderer never cares which integrator, spacetime, or step count produced a curve. Multiple independently simulated trajectories are overlaid into one `Scene`.

**Stage 3 backends:**

| Backend | Library / target | Used by | Output |
|---------|------------------|---------|--------|
| `GpuPolylineBackend` | `visualization_gpu` (viewer only) | `visualization_viewer` | OpenGL draw into GLFW framebuffer |
| `CpuRasterizerBackend` | `visualization` | `visualization_export` | `Framebuffer` → PPM (no OpenGL) |

GPU trajectory shaders and buffers live under `visualization/Renderer/Gpu/` (embedded sources). They are not shared with `realtime/`. Interactive viewer v1 draws starfield, an opaque filled horizon disc, a soft warm ash glow (~22% peak), and an optional decorative photon-sphere ring (~35% peak) via a **two-pass** GPU draw (opaque disc with depth write, then FX with depth test off), plus solid trails and markers. CPU export paints the same disc/glow/ring in `CPURasterizer`. Cosmetic `PostProcessor` bloom / lensing remains on the CPU export path; GPU bloom parity is deferred.

`run/viewer/main.cpp` and `run/export/main.cpp` default to Kerr bound orbits with commented Schwarzschild blocks for easy swap (same pattern as `run/benchmark/main.cpp`).

`auto_frame` distances the camera from scene extent and uses a tilted yaw/pitch (not edge-on to equatorial orbits). Playback scrub in the viewer advances at a time-based rate (~8% of duration per second while Left/Right are held).

Scene horizon radius is geometric (not a metric type):

```text
horizon_radius = max(VisualizationConfig.scene.horizon_radius,
                     max(horizon_radius / characteristic_radius over stored trajectories))
```

All drawing parameters are set in `run/*/main.cpp` via `VisualizationConfig`. Viewer/export **do not load CSV**; benchmark CSVs feed scientific analysis only.

Internal unit tests: `-DPENROSE_BUILD_TESTS=ON` (default **OFF**).

---

## 6. GPU realtime stack

```text
Engine → ShaderManager → RenderPass pipeline → GLSL compute ray march → image → blit
                ↘ FrameCapture (P key) → imagesequence/<timestamp>/
```

Owned under `realtime/`. Independent build target `Penrose`.

| Area | Location |
|------|----------|
| Engine / window / capture | `realtime/core/` |
| ShaderManager (compute load, include resolve, cache) | `realtime/core/ShaderManager.h/.cpp` |
| RenderPass pipeline (GeodesicPass, UpscalePass) | `realtime/render/` |
| Renderer (compute output texture, particle SSBO, blit) | `realtime/render/Renderer.h/.cpp` |
| Camera / particles / ParticleSystem interface | `realtime/scene/` |
| Spacetime FX helpers (LutBaker, AccretionDisk) | `realtime/spacetime/` |
| Shaders / resources | `realtime/shaders/`, `realtime/resources/` |
| PPM → video script | `realtime/visualization/ppm_to_video.py` |

**Default metric:** `realtime/shaders/reduced.comp` includes `metrics/kerr_full.glsl` + `common/march_full.glsl`. Schwarzschild full / reduced paths remain as commented includes. Kerr `rs` / `a` are hardcoded in the GLSL file.

**RenderPass pipeline:** Engine holds `vector<unique_ptr<RenderPass>>`. Each pass receives a `PassContext` and executes independently. `GeodesicPass` dispatches the compute shader; `UpscalePass` presents the result (placeholder for future foveation).

**ShaderManager:** Loads compute shaders via `loadMetricCompute(type, path)`, recursively resolving `#include`. C++ currently exposes only `MetricType::SCHWARZSCHILD_REDUCED`; Kerr is selected by the includes inside `reduced.comp`, not by a separate `MetricType`.

**LutBaker:** Still bakes a Schwarzschild predictive LUT at startup. The active **full** march path does not sample it (`march_reduced.glsl` does).

**ParticleSystem:** Polymorphic interface implemented by `FallingParticleSystem` and `AccretionDisk`. Engine merges systems into one SSBO.

Docs: [`frame_capture/`](frame_capture/).

---

## 7. Repository map

```text
penrose/
├── run/
│   ├── benchmark/main.cpp      → physics_benchmark
│   ├── viewer/main.cpp         → visualization_viewer (GPU Stage 3)
│   ├── export/main.cpp         → visualization_export (CPU Stage 3)
│   └── adapter/                → SimulationResult → StoredTrajectory bridge
├── physics/                    # CPU reference solver (+ analysis Python)
├── visualization/              # Trajectory presentation (CPU export + GPU viewer)
├── realtime/                   # GPU ray-march product (independent)
├── shared/                     # GR vocabulary (State, MetricKind, parameters)
├── vendor/glad/                # Neutral OpenGL loader for trajectory viewer
├── outputs/                    # Generated CSVs, figures, PPMs, notebooks
├── docs/
│   ├── ARCHITECTURE.md         # this file (current)
│   ├── RUNNING.md              # install / run all pipelines
│   ├── VISUALIZATION_GUIDE.md  # trajectory viz three-executable UX
│   ├── frame_capture/          # GPU ray-march capture helpers
│   ├── reviews/                # historical architecture reviews
│   └── reports/                # long-form science notes (historical layout)
├── notes/                      # working notes / screenshots
└── CMakeLists.txt
```

---

## 8. Design rules

1. **Entry points configure; pipelines implement.** No concrete `*Metric` in `run/*/main.cpp`.
2. **Do not grow `SimulationConfig` into a spacetime kitchen sink.** New physics → new parameter structs + overloads.
3. **Physics resolution ≠ visualization resolution.** `dt` / integrator settings stay in physics; resampling / interpolation knobs stay in `VisualizationPreparationSettings`.
4. **Particles do not couple.** Independent `SimulationRequest`s; overlay only in `prepare_scene`.
5. **Visualization never mutates physics state.** Trajectories are immutable after adaptation. The visualization library does not include physics headers.
6. **Presentation ≠ GR.** Screen-space lensing / bloom are cosmetic.
7. **CPU solver is the reference.** Realtime ray-march and trajectory-viz GPU drawing are for interaction / presentation until shared abstractions catch up.
8. **Preserve validated math.** Architecture changes extract modules; they do not silently rewrite Christoffel formulas.
9. **Ownership boundaries:** `shared/` = vocabulary; `physics/` (`penrose_physics`) = CPU science; `visualization/` (+ `visualization_gpu` for the viewer) = presentation; `physics/analysis/` = benchmark analysis; `realtime/` = ray-march engine (independent).
10. **OpenGL stays out of headless export.** Only `visualization_viewer` links `visualization_gpu` / `penrose_glad` / GLFW for trajectory viz.

---

## 9. Extensibility (metrics)

Kerr is already wired as a first-class CPU swap next to Schwarzschild:

1. `MetricKind::Kerr` / `CoordinateChartKind::KerrBoyerLindquist`
2. `KerrParameters` + `KerrMetric` + IC builders + observables
3. `SimulationRequest::metric` as `std::variant<SchwarzschildParameters, KerrParameters>`
4. Benchmark overloads writing `*_kerr.csv` / `null_kerr_b_*.csv`
5. Analysis loaders discover Kerr or Schwarzschild CSVs by filename

To add another spacetime, follow the same additive pattern — new parameter POD, metric class, IC builders, pipeline overloads, CMake sources, and analysis filename candidates — without growing `SimulationConfig` into a kitchen sink.

---

## 10. Related documentation

| Document | Status |
|----------|--------|
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | **Current** — this file |
| [`RUNNING.md`](RUNNING.md) | **Current** — build / run all pipelines |
| [`VISUALIZATION_GUIDE.md`](VISUALIZATION_GUIDE.md) | **Current** — trajectory viz UX |
| [`frame_capture/`](frame_capture/) | **Current** — GPU ray-march capture |
| [`../visualization/README.md`](../visualization/README.md) | **Current** — visualization module |
| [`../AGENTS.md`](../AGENTS.md) | **Current** — contributor / agent conventions |
| [`reports/Penrose_from_First_Principles.md`](reports/Penrose_from_First_Principles.md) | Science walkthrough (layout / shader assumptions may be dated; see banner) |
| [`reviews/`](reviews/) | **Historical** reviews (not normative; see reviews README for 2026-07 status) |
