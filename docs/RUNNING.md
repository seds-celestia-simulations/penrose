# Running Penrose

This guide covers how to install dependencies and run each Penrose pipeline:

1. **GPU real-time ray-march engine** (`Penrose`)
2. **Frame capture** and **PPM → video** utilities
3. **CPU physics benchmarking** (`physics_benchmark`)
4. **Trajectory viewer / export** (`visualization_viewer`, `visualization_export`)

| Pipeline | Location | Executable / tool |
|---|---|---|
| GPU real-time ray march (Kerr default) | `realtime/` | `Penrose` |
| CPU scientific (Schwarzschild production) | `physics/` + `run/benchmark/` | `physics_benchmark` |
| Trajectory visualization | `visualization/` + `run/viewer|export/` | `visualization_viewer` (GPU), `visualization_export` (CPU) |

Architecture: [`ARCHITECTURE.md`](ARCHITECTURE.md) · Trajectory viz UX: [`VISUALIZATION_GUIDE.md`](VISUALIZATION_GUIDE.md).

---

## 1. Real-Time Interactive Engine

The `Penrose` target runs the GPU compute ray-marcher under `realtime/`.

**Default metric:** Kerr (`realtime/shaders/metrics/kerr_full.glsl`), selected by includes in `realtime/shaders/reduced.comp`. Schwarzschild full and reduced+LUT paths remain available by swapping those `#include` lines and rebuilding.

There is no CLI flag or C++ `MetricType` for Kerr yet — metric choice is shader assembly.

### Prerequisites

- C++20 compiler
  - **Windows**: Visual Studio (MSVC) or another CMake-supported toolchain
  - **Linux**: GCC or Clang
  - **macOS**: Clang via Xcode Command Line Tools
- CMake 3.22 or newer
- [vcpkg](https://github.com/microsoft/vcpkg) for package management
- OpenGL-capable GPU/drivers

Declared C++ dependencies (`vcpkg.json`): `glfw3`, `glad`, `glm`, `eigen3`.

GLAD for the GPU ray-march app is built from `realtime/gpu/`. The trajectory viewer uses a neutral copy under `vendor/glad/` (`penrose_glad` target) and must not include `realtime/`. GLFW is fetched by CMake if needed.

### Install vcpkg

Clone and bootstrap vcpkg somewhere on your machine (not necessarily inside the Penrose tree):

**Windows (PowerShell):**

```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

**Linux / macOS:**

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Remember the absolute path to the vcpkg root. You will pass it to CMake as:

```text
[PATH_TO_VCPKG]/scripts/buildsystems/vcpkg.cmake
```

If a local `vcpkg/` directory exists at the Penrose repository root, `CMakeLists.txt` may pick up its toolchain automatically.

### Build the visualizer

From the Penrose repository root:

**Windows:**

```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[PATH_TO_VCPKG]\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Debug
```

**Linux / macOS:**

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[PATH_TO_VCPKG]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Run the visualizer

Run from the directory that contains the executable so the post-build-copied `shaders/` and `resources/` folders are found.

**Windows:**

```powershell
cd build\Debug
.\Penrose.exe
```

**Linux / macOS:**

```bash
./build/Penrose
```

Depending on generator/platform, the binary may live under a configuration subdirectory such as `build/Debug/Penrose`.

### Switching realtime metrics

Edit `realtime/shaders/reduced.comp`:

| Goal | Includes |
|------|----------|
| Kerr (default) | `metrics/kerr_full.glsl` + `common/march_full.glsl` |
| Schwarzschild 4D | `metrics/schwarzschild_full.glsl` + `common/march_full.glsl` |
| Schwarzschild reduced + LUT | `metrics/schwarzschild_reduced.glsl` + `common/march_reduced.glsl` |

Kerr mass/spin are `const` values inside `kerr_full.glsl`. Rebuild `Penrose` after edits (CMake copies `shaders/` next to the binary).

### Controls

| Input | Action |
|---|---|
| WASD | Move camera |
| Mouse | Look around when cursor is captured |
| Tab | Toggle cursor capture |
| Left / Right Shift | Sprint multiplier |
| P | Toggle frame capture |
| Escape | Exit or release/capture cursor (depending on input state) |

---

## 2. Frame Capture and PPM → Video

### Frame capture (inside the GPU visualizer)

1. Start `Penrose` (see §1).
2. Press **P** to start capturing.
3. Fly / animate as desired.
4. Press **P** again to stop.

Console messages look like:

```text
Started capturing frames to: imagesequence/2026-05-20_14-30-45
Stopped capturing frames. Total frames saved: 300
```

#### Output layout

Frames are written as PPM files under a timestamped session folder:

```text
imagesequence/
└── YYYY-MM-DD_HH-MM-SS/
    ├── frame_000000.ppm
    ├── frame_000001.ppm
    └── ...
```

Notes:

- Capture runs every rendered frame while enabled (mostly disk I/O overhead).
- At 800×600, each PPM is ≈ 1.4 MB; 30 s at 30 fps ≈ 1.26 GB.
- Prefer a fast disk (SSD) for longer recordings.

More detail: [docs/frame_capture/FRAME_CAPTURE.md](frame_capture/FRAME_CAPTURE.md).

### Convert PPM sequences to video

#### Python helper (recommended)

Script: `realtime/visualization/ppm_to_video.py`

**Python dependencies** (also listed in `requirements.txt`):

```bash
pip install opencv-python numpy
```

**FFmpeg** must be installed and on `PATH`:

- **Windows**: `choco install ffmpeg`, or download from https://ffmpeg.org/download.html
- **macOS**: `brew install ffmpeg`
- **Linux**: e.g. `sudo apt install ffmpeg`

**Run** (from the repository root, or any directory):

```bash
python realtime/visualization/ppm_to_video.py
```

The script will:

1. Ask for the path to the `imagesequence` folder
2. List timestamped capture sessions
3. Let you pick a session
4. Ask for frame rate (default 30)
5. Write an MP4 under `videos/`

Example path prompts:

- `imagesequence`
- `/home/user/projects/penrose/imagesequence`
- `C:\Users\user\penrose\imagesequence`

Example output:

```text
videos/2026-05-20_14-30-45.mp4
```

More detail: [docs/frame_capture/PPM_TO_VIDEO_README.md](frame_capture/PPM_TO_VIDEO_README.md).

#### Manual FFmpeg alternative

```bash
ffmpeg -framerate 30 -pattern_type glob \
  -i 'imagesequence/2026-05-20_14-30-45/*.ppm' \
  -c:v libx264 -pix_fmt yuv420p output.mp4
```

> Trajectory-export PPMs (`outputs/rendered_frames/` from `visualization_export`) are a different tree from GPU ray-march `imagesequence/`. The helper above targets ray-march capture sessions.

---

## 3. Physics Benchmarking (CPU)

Configure the suite in [`run/benchmark/main.cpp`](../run/benchmark/main.cpp). The CMake target `physics_benchmark` runs freefall, orbital, and null-geodesic validation drivers from `physics/validation/` via `BenchmarkRunner`.

Today the production path supports **Schwarzschild** and **Kerr**. Select in [`run/benchmark/main.cpp`](../run/benchmark/main.cpp) via `config.spacetime` + `config.metric` (`SchwarzschildParameters` or `KerrParameters`). Kerr writes `freefall_kerr.csv`, `orbital_kerr.csv`, `null_kerr_b_*.csv`.

### Build

If you already configured the project in §1, you only need to build the benchmark target.

**Linux / macOS:**

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[PATH_TO_VCPKG]/scripts/buildsystems/vcpkg.cmake
cmake --build build --target physics_benchmark
```

**Windows:**

```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[PATH_TO_VCPKG]\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Debug --target physics_benchmark
```

`physics_benchmark` links Eigen and does not require OpenGL/GLFW for execution.

### Run

**Linux / macOS:**

```bash
./build/physics_benchmark
```

**Windows:**

```powershell
.\build\Debug\physics_benchmark.exe
```

Null-geodesic cases can take several minutes.

### Outputs

CSV trajectories and diagnostics are written under:

```text
outputs/benchmark_data/<timestamp>/
```

Optional Python analysis (`physics/analysis/`; legacy shim `python -m visualization.scientific.*` still works):

```bash
python -m physics.analysis.analyze_benchmarks
python -m physics.analysis.plot_benchmarks
python -m physics.analysis.generate_report
```

Notebooks / figures land under `outputs/analysis/` and `outputs/validation_figures/`.

---

## 4. Trajectory Viewer and Export

Edit config in [`run/viewer/main.cpp`](../run/viewer/main.cpp) or [`run/export/main.cpp`](../run/export/main.cpp), then:

```bash
cmake --build build --target visualization_viewer visualization_export
./build/visualization_viewer   # GPU polyline backend (needs display + OpenGL)
./build/visualization_export   # CPU rasterizer backend (headless PPM)
```

Pipeline: `SimulationRequest`(s) → `run_all` → `prepare_scene_from_results` → Stage 3 backend.

| Executable | Stage 3 backend | Notes |
|------------|-----------------|-------|
| `visualization_viewer` | `GpuPolylineBackend` (`visualization_gpu`) | Interactive; links `penrose_glad` + GLFW |
| `visualization_export` | `CpuRasterizerBackend` | No OpenGL; writes `outputs/rendered_frames/` |

Full walkthrough: [`VISUALIZATION_GUIDE.md`](VISUALIZATION_GUIDE.md).

| Option | Default | Effect |
|--------|---------|--------|
| `PENROSE_BUILD_VIEWER` | `ON` | Build interactive GPU trajectory viewer |
| `PENROSE_BUILD_TESTS` | `OFF` | Internal visualization unit tests |

Export stills / sequences: `outputs/rendered_frames/<timestamp>/`.

---

## Quick reference

| Goal | Command (after configure) |
|---|---|
| Build GPU ray-march visualizer | `cmake --build build` (Windows: add `--config Debug`) |
| Run GPU ray-march visualizer (Kerr default) | `./build/Penrose` or `build\Debug\Penrose.exe` |
| Build CPU benchmarks | `cmake --build build --target physics_benchmark` |
| Run CPU benchmarks (Schwarzschild) | `./build/physics_benchmark` or `build\Debug\physics_benchmark.exe` |
| Build trajectory viewer / export | `cmake --build build --target visualization_viewer visualization_export` |
| Run trajectory viewer (GPU) | `./build/visualization_viewer` |
| Run trajectory export (CPU) | `./build/visualization_export` |
| PPM → video (ray-march capture) | `python realtime/visualization/ppm_to_video.py` |

---

## Related docs

- [README.md](../README.md) — project overview
- [ARCHITECTURE.md](ARCHITECTURE.md) — current architecture (sole reference)
- [VISUALIZATION_GUIDE.md](VISUALIZATION_GUIDE.md) — trajectory visualization UX
- [frame_capture/FRAME_CAPTURE.md](frame_capture/FRAME_CAPTURE.md) — GPU ray-march frame capture
- [frame_capture/PPM_TO_VIDEO_README.md](frame_capture/PPM_TO_VIDEO_README.md) — PPM → video
- [reviews/](reviews/) — historical architecture reviews
