# Zenith

Zenith is a high-performance, Medal.tv-style clip recorder for Windows. It uses `libobs` (the core engine behind OBS Studio) to provide pristine capture quality with minimal overhead. It avoids common issues like Windows 11 yellow capture borders by utilizing DXGI Desktop Duplication, and includes native Direct2D overlays for key presses and webcams.

## Features

- **No Yellow Border**: Configurable capture method defaults to DXGI Desktop Duplication instead of WGC.
- **Shadowplay-style Clipping**: Keep a running buffer of the last N minutes without saving to disk until you press the hotkey.
- **Native Key Overlay**: Built-in Direct2D key overlay (like osu! KeyOverlay) that composites directly into the recording.
- **Webcam Overlay**: Sizable, rotatable, and flippable camera overlay that composites directly into the recording without needing a preview window.
- **Robust Hotkeys**: Multi-key combos and toggle states that don't bug out when you hold multiple keys.

## Building Locally

### Prerequisites
- Visual Studio 2022 with C++ Desktop Development workload
- CMake 3.20+
- Git

### Initializing the Repository

1. Clone this repository with submodules:
   ```bash
   git clone --recursive <your-repo-url>
   cd Zenith
   ```
   *If you already cloned without submodules, run: `git submodule update --init --recursive`*

### Compiling

1. **Build the OBS backend (first time only, takes a while):**
   ```bash
   cd third_party/obs-studio
   cmake --preset windows-x64 -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF -DENABLE_UI=OFF
   cmake --build build_x64 --config RelWithDebInfo
   cd ../..
   ```

2. **Build Zenith:**
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build build --config RelWithDebInfo
   ```

3. **Run:**
   The executable will be located at `build/RelWithDebInfo/Zenith.exe`.

## Uploading to GitHub

If you want to use the provided GitHub Actions workflow so you don't have to build locally, follow these steps using the [GitHub CLI](https://cli.github.com/):

1. **Initialize git (if not already done):**
   ```bash
   git init
   git add .
   git commit -m "Initial commit of Zenith"
   ```

2. **Create a new repository on GitHub:**
   ```bash
   gh repo create Zenith --public --source=. --remote=origin --push
   ```

3. **Submodule Setup:**
   If you haven't linked the obs-studio submodule, do so now:
   ```bash
   git submodule add https://github.com/judehek/ascent-obs third_party/obs-studio
   git commit -m "Add obs-studio submodule"
   git push
   ```

Every time you push to `main`, the GitHub Actions workflow will automatically compile OBS and Zenith, and upload the resulting `.exe` as a downloadable artifact.
