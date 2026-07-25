# Penrose — AI Agent Guide

## What this project is

A General Relativity framework with three independent pipelines:

- **CPU science** (`physics/`): geodesic integration, validation benchmarks, Python analysis
- **Trajectory visualization** (`visualization/` + `run/viewer|export/`): stored trajectories → GPU polyline viewer or headless CPU PPM export
- **GPU realtime** (`realtime/`): OpenGL 4.3 compute-shader null geodesic ray marching

They share only `shared/` (GR type definitions). `physics/`, `visualization/`, and `realtime/` are peers — they do **not** link each other. Trajectory-viz GPU code (`visualization_gpu`) also does **not** include or link `realtime/`.

## Tech stack

- C++20, CMake 3.22+
- OpenGL 4.3+ (GLSL 430, GLAD loader, GLFW)
- Eigen3 (CPU physics), GLM (GPU math)
- vcpkg for Eigen3 and GLM; GLFW via FetchContent

## Build

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg.cmake
cmake --build build
```

| Target | Role |
|--------|------|
| `Penrose` | GPU realtime ray march (`./build/Penrose`) |
| `physics_benchmark` | CPU validation suite |
| `visualization_viewer` | Interactive trajectory viewer |
| `visualization_export` | Headless PPM export |

On Windows multi-config generators, binaries typically land under `build/Debug/`.

## File layout

```
shared/                  # Cross-module GR definitions (header-only)
  state/GeodesicState.h  # State (Eigen types)
  spacetime/Metric.h     # Abstract Spacetime::Metric interface
  spacetime/MetricKind.h # MetricKind, CoordinateChartKind
  metrics/               # SchwarzschildParameters, KerrParameters (POD)
  constants/             # G, c, schwarzschild_radius()
  observer/              # Observer stub
  units/                 # Placeholder for unit system

physics/                 # CPU scientific pipeline
  metrics/               # SchwarzschildMetric, KerrMetric (both production)
  geodesics/             # GeodesicDynamics (implements DynamicsModel)
  integrators/           # RK4Integrator
  simulation/            # TrajectorySolver, SimulationPipeline, Sch + Kerr IC builders
  validation/            # Benchmarks + observables (Sch + Kerr)
  analysis/              # Python CSV analysis / figures / reports

realtime/                # GPU real-time engine
  core/                  # Engine, Window, Shader, ShaderManager, Texture, …
  render/                # Renderer (compute image + blit), GeodesicPass, UpscalePass
  scene/                 # Camera, Particle, ParticleBuffer, ParticleSystem
  spacetime/             # LutBaker (Schwarzschild), AccretionDisk
  shaders/               # Modular compute GLSL (see below)
  gpu/                   # GLAD loader
  resources/             # Textures (starfield)
  visualization/         # ppm_to_video.py helper

visualization/           # Trajectory presentation (CPU export + GPU viewer)
  Preparation/           # StoredTrajectory, prepare_scene
  Trajectory/            # Trajectory, TrajectoryAdapter
  Scene/                 # Scene, SceneBuilder, playback
  Camera/                # viz::Camera
  Renderer/              # CPURasterizer, GpuPolylineBackend
  Presentation/          # VisualizationConfig, PostProcessor (CPU export)
  Apps/                  # ViewerApp, DisplayBlit
  IO/                    # PPM writer, output paths
  Tests/                 # viz unit tests (optional)

run/
  benchmark/main.cpp     → physics_benchmark
  viewer/main.cpp        → visualization_viewer
  export/main.cpp        → visualization_export
  adapter/               → SimulationResult → StoredTrajectory bridge

vendor/                  # stb_image.h; neutral glad for trajectory viewer
docs/                    # ARCHITECTURE.md is the sole current architecture reference
```

## Critical: Module dependency rules

```
penrose_shared (header-only)
    ↑
    ├── physics         (links: Eigen3)
    ├── visualization   (links: Eigen3; viewer also visualization_gpu + penrose_glad + glfw)
    └── realtime        (links: glfw, glad, glm)
```

- Pipelines are peers. Never create cross-module links between them.
- `realtime/` owns its own metric GLSL — it does **not** use `physics/*Metric`.
- Entry points configure in `run/*/main.cpp`; they should not construct concrete `*Metric` types.
- `run/benchmark`, `run/viewer`, and `run/export` default to Kerr with commented Schwarzschild blocks for easy swap.

## Metric status (important)

| Pipeline | Schwarzschild | Kerr |
|----------|---------------|------|
| CPU `physics/` | Production path (wired end-to-end) | Production path (wired end-to-end; swap via `SpacetimeKind::Kerr` + `KerrParameters`) |
| GPU `realtime/` | Available via `#include` swap in `reduced.comp` | **Default** active metric (`metrics/kerr_full.glsl` + `march_full.glsl`); spin/mass hardcoded in GLSL |
| Trajectory viz | Chart-agnostic rendering of stored states | Accepts `KerrBoyerLindquist` (same `(t,r,θ,φ)` projection) |

## When modifying code, always reference

| Area | Read first |
|------|-----------|
| Any type that crosses modules | `shared/` headers |
| Adding a CPU metric | `shared/metrics/`, `shared/spacetime/MetricKind.h`, `physics/metrics/`, `physics/simulation/SimulationPipeline.cpp`, IC builders, validation overloads, `CMakeLists.txt` |
| Adding a realtime metric | Create `.glsl` under `realtime/shaders/metrics/`, wire `#include` in `realtime/shaders/reduced.comp` (and optionally extend `MetricType` / `ShaderManager`) |
| Modifying physics solver | `physics/simulation/TrajectorySolver.h`, `physics/geodesics/GeodesicDynamics.h` |
| Modifying GPU rendering | `realtime/render/Renderer.h`, `realtime/core/Engine.cpp`, `realtime/render/GeodesicPass.cpp` |
| Modifying GLSL shaders | `realtime/shaders/` (see shader conventions below) |
| Adding a new render pass | `realtime/render/RenderPass.h`, new `*Pass` under `realtime/render/`, register in `Engine::initAssets()` |
| CPU visualization pipeline | `visualization/Presentation/VisualizationConfig.h`, `visualization/Preparation/` |
| CMake changes | Top-level `CMakeLists.txt` — never create cross-module links |

## Shader conventions (`realtime/shaders/`)

- `#version 430 core` — GLSL 430, OpenGL 4.3 required
- Active entry is the **compute** shader `reduced.comp` (not a fragment ray march)
- `ShaderManager` loads one compute path, recursively resolving `#include "…"` with circular-include protection
- Screen presentation uses `common/screen.vert` + `common/screen.frag`
- Resource paths in `Engine.cpp::initAssets()` must match CMake `POST_BUILD` copies: `shaders/` and `resources/` next to the `Penrose` binary (not `realtime/shaders/`)

### Module layout

```
shaders/
  reduced.comp                 # Assembly: uniforms + metric + scene + march
  common/
    uniforms.glsl              # Compute layout + camera/LUT uniforms
    scene.glsl                 # Particles + volume include (disk / no_volume)
    disk.glsl / no_volume.glsl # Accretion volume toggle
    march_full.glsl            # 4D Boyer–Lindquist RK4 march
    march_reduced.glsl         # Reduced-orbit + LUT path (Schwarzschild)
    skybox.glsl, noise.glsl
    screen.vert / screen.frag  # Blit compute image to screen
  metrics/
    kerr_full.glsl             # Default in reduced.comp
    schwarzschild_full.glsl
    schwarzschild_reduced.glsl
```

### How assembly works

`reduced.comp` includes metric + march modules. Example (current default):

```glsl
#include "common/uniforms.glsl"
#include "metrics/kerr_full.glsl"
#include "common/scene.glsl"
#include "common/march_full.glsl"
```

To switch Schwarzschild reduced+LUT, comment Kerr/full and include `schwarzschild_reduced.glsl` + `march_reduced.glsl`, then rebuild (or add hot-reload). Kerr spin/mass are `const` values inside `kerr_full.glsl` (`rs = 0.25`, near-extremal `a_kerr`).

`Engine` still registers `MetricType::SCHWARZSCHILD_REDUCED` and still bakes a Schwarzschild LUT; the active **full** march path does not use that LUT.

## RenderPass pipeline

- `RenderPass` — `execute(PassContext&)`, `name()`
- `GeodesicPass` — binds compute shader, textures, particle SSBO; `glDispatchCompute`
- `UpscalePass` — presents / blits the compute image (placeholder for future foveation)
- Engine holds `vector<unique_ptr<RenderPass>> passes`

## ParticleSystem interface

- `ParticleSystem` — `update(float dt)`, `getParticles() const`
- `FallingParticleSystem`, `AccretionDisk` implement it
- Engine merges all systems into one SSBO payload each frame

## ShaderManager

- `loadMetricCompute(type, computePath)` — resolve includes and compile compute shader
- `setMetric(type)` / `getActive()` — select cached shader
- Today only `MetricType::SCHWARZSCHILD_REDUCED` exists in C++; Kerr is selected by the `#include` inside `reduced.comp`

## C++ conventions

- OpenGL objects use raw C API — follow existing pattern
- Realtime classes: no namespaces (global `Renderer`, `Engine`, `Camera`)
- Physics classes: namespaced (`Spacetime::`, `Dynamics::`, `Simulation::`, `Physics::`)
- Visualization classes: `viz::` namespace
- Header-only where possible; `.cpp` only for non-trivial implementation
- No comments unless asked; keep code self-documenting
- Use `std::make_unique` for heap-allocated objects
- Never add a `shared/` dependency to `physics/` that pulls in GL or GLFW headers
