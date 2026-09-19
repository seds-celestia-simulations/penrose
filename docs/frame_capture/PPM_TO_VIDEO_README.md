# PPM to Video Converter

A Python utility to convert PPM image sequences from the Penrose **GPU** frame capture feature into MP4 videos.

> This targets `imagesequence/<timestamp>/` from the `Penrose` ray-march app (**P** key).
> Trajectory-export stills/sequences from `visualization_export` live under `outputs/rendered_frames/` and are a separate tree.

## Features

- ✅ Handles PPM image sequences (from frame capture feature)
- ✅ Interactive path selection for imagesequence folder
- ✅ Lists all available timestamped capture sessions
- ✅ Configurable frame rate (fps)
- ✅ Automatic output folder creation (`videos/`)
- ✅ Progress tracking during conversion
- ✅ Calculates and displays video duration

## Prerequisites

Install required Python packages:

```bash
pip install opencv-python numpy
```

You'll also need FFmpeg installed for video encoding:
- **Windows**: `choco install ffmpeg` or download from https://ffmpeg.org/download.html
- **macOS**: `brew install ffmpeg`
- **Linux**: `sudo apt install ffmpeg` (Ubuntu/Debian)

## Usage

### Basic Usage

```bash
python realtime/visualization/ppm_to_video.py
```

The script will:
1. Prompt you for the path to the `imagesequence` folder
2. Show all available timestamped capture sessions
3. Let you select which session to convert
4. Ask for desired frame rate (fps)
5. Convert to MP4 and save to `videos/` folder

### Path Examples

When prompted for the imagesequence folder path, you can enter:
- `imagesequence` - Current directory
- `./imagesequence` - Current directory (explicit)
- `/home/user/projects/penrose/imagesequence` - Full path
- `C:\Users\user\penrose\imagesequence` - Windows full path
- `/Volumes/Storage/imagesequence` - Network or external drive

### Frame Rate Examples

Common frame rates:
- **24 fps** - Film standard
- **30 fps** - Standard for many videos
- **60 fps** - Smooth motion, larger file size
- **Custom** - Enter any positive number

## Workflow Example

1. Run your Penrose application
2. Press **P** to start recording frames
3. Run your simulation/animation
4. Press **P** again to stop recording
5. Open terminal and run `python realtime/visualization/ppm_to_video.py`
6. Follow the prompts to select your capture and convert to video

```
Path to imagesequence folder: /home/user/projects/penrose/imagesequence
✓ Found imagesequence folder at: /home/user/projects/penrose/imagesequence

Available Image Sequences:
1. 2026-05-20_14-30-45 (300 frames)
2. 2026-05-20_13-45-22 (450 frames)

Select a folder (1-2) or 'q' to quit: 1

Enter frame rate (fps) [default: 30]: 30

Converting 300 frames to video...
Resolution: 800x600
Frame rate: 30 fps
Output: videos/2026-05-20_14-30-45.mp4
Progress: 10%
Progress: 20%
...
Progress: 100%

✓ Video successfully created: videos/2026-05-20_14-30-45.mp4
  Duration: 10.00 seconds
```

## Output

Videos are saved to the `videos/` folder (created automatically if it doesn't exist) with the timestamp folder name as the filename:
- Input: `imagesequence/2026-05-20_14-30-45/frame_000000.ppm`
- Output: `videos/2026-05-20_14-30-45.mp4`

## Tips

- **File Size**: MP4 files are significantly smaller than the original PPM sequence
  - 300 PPM frames at 800x600 ≈ 420 MB
  - Same frames as MP4 ≈ 5-15 MB (depending on quality)
  
- **Codec**: Uses `mp4v` (MPEG-4 Part 2) for better compatibility

- **Quality**: If you need higher quality, modify the codec in the script to use `libx264` or `libx265`

## Troubleshooting

### "Error: Could not create video writer"
- **Solution**: Make sure FFmpeg is installed and in your system PATH
- Test with: `ffmpeg -version`

### "No PPM images found"
- **Solution**: Make sure the selected folder contains `.ppm` files, not `.png`
- Check that frame capture was successful

### "Permission denied"
- **Solution**: Make sure you have read access to the imagesequence folder
- Try running with appropriate permissions

### Videos folder permission error
- **Solution**: Make sure you have write access to the current directory
- Try running from a directory where you have write permissions

## Advanced Usage

To modify codec or quality, edit these lines in the script:

```python
# Change codec (before line with cv2.VideoWriter)
# For H.264 (libx264) - slower but better compression:
fourcc = cv2.VideoWriter_fourcc(*'avc1')

# For better quality, add these before out.release():
# out.set(cv2.VIDEOWRITER_PROP_QUALITY, 95)
```

## Conversion Command (Manual with FFmpeg)

If you prefer to use FFmpeg directly:

```bash
ffmpeg -framerate 30 -pattern_type glob -i 'imagesequence/2026-05-20_14-30-45/*.ppm' \
  -c:v libx264 -pix_fmt yuv420p output.mp4
```
