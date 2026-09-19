# Frame Capture Feature

## Overview

The frame capture feature records sequences of GPU-rendered frames to disk. Useful for animations, debugging visualizations, or generating video sequences.

This documents the **GPU ray-march** (`Penrose`) capture path. Trajectory-export PPMs go to `outputs/rendered_frames/` via `visualization_export` (CPU `CpuRasterizerBackend`) — a separate workflow from both this capture path and the interactive `visualization_viewer`.

## How to Use

### Starting and Stopping Capture

- **Press the "P" key** to toggle frame capture mode:
  - **First press**: Starts capturing frames
  - **Second press**: Stops capturing frames

### Output Format

- **Frame Format**: PPM (Portable Pixmap)
- **Naming Convention**: `frame_XXXXXX.ppm` (6-digit zero-padded numbering)
- **Output Location**: timestamped directory under `imagesequence/`:

```text
imagesequence/
├── 2026-05-20_14-30-45/
│   ├── frame_000000.ppm
│   ├── frame_000001.ppm
│   └── ...
└── 2026-05-20_15-22-10/
    ├── frame_000000.ppm
    └── ...
```

### Timestamps

Directory name format: `YYYY-MM-DD_HH-MM-SS`. Each capture session creates a new folder.

## Technical Details

### Implementation Files

| File | Role |
|------|------|
| `realtime/core/FrameCapture.h` | Capture toggle, timestamped directory creation, frame path generation |
| `realtime/core/Engine.cpp` / `Engine.h` | Owns `FrameCapture`; calls `Renderer::captureFrame` each frame while capturing |
| `realtime/render/Renderer.cpp` | `captureFrame`: `glReadPixels`, PPM write, vertical flip |
| `realtime/core/Window.cpp` | `processInput`: **P** key toggles capture via `toggleCapture()` |
| `realtime/main.cpp` | Starts `Engine` (capture is owned by the engine, not `main`) |

## Console Output

When you press **P**:

- **Start**: `Started capturing frames to: imagesequence/2026-05-20_14-30-45`
- **Stop**: `Stopped capturing frames. Total frames saved: 300`

## Converting PPM to Video

Recommended helper:

```bash
python realtime/visualization/ppm_to_video.py
```

Or FFmpeg directly:

```bash
ffmpeg -framerate 30 -pattern_type glob \
  -i 'imagesequence/2026-05-20_14-30-45/*.ppm' \
  -c:v libx264 -pix_fmt yuv420p output.mp4
```

See [PPM_TO_VIDEO_README.md](PPM_TO_VIDEO_README.md).

## Performance Notes

- Capture runs every rendered frame while enabled
- Overhead is mostly disk I/O
- Prefer an SSD for longer recordings

## File Size Estimation

For 800×600 resolution:

- Each PPM frame ≈ 1.4 MB
- 30 seconds ≈ 1.26 GB (at 30 fps)
- 60 seconds ≈ 2.52 GB (at 30 fps)

## Related Documentation

- [PPM_TO_VIDEO_README.md](PPM_TO_VIDEO_README.md) — PPM → MP4 (`realtime/visualization/ppm_to_video.py`)
- [../RUNNING.md](../RUNNING.md) — full run guide
- [../ARCHITECTURE.md](../ARCHITECTURE.md) — current architecture
