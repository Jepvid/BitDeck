# BitDeck

A modding tool for packaging mods for HarbourMasters games. Successor to
[Retro](https://github.com/HarbourMasters/Retro), rewritten in C++ / Qt6.

Status: early scaffolding, not yet functional.

## Nightly builds

Always the latest successful build of `main` (rebuilt on every commit via
[GitHub Actions](.github/workflows/build.yml)):

- [Windows](https://nightly.link/Jepvid/BitDeck/workflows/build.yml/main/BitDeck-windows.zip)
- [macOS](https://nightly.link/Jepvid/BitDeck/workflows/build.yml/main/BitDeck-macos.zip)
- [Linux (.AppImage)](https://nightly.link/Jepvid/BitDeck/workflows/build.yml/main/BitDeck-linux.zip)

All three are self-contained (Qt/libzip runtime bundled in) — the Linux
download is a zip containing a `.AppImage`; make it executable
(`chmod +x BitDeck-x86_64.AppImage`) and run it directly. Pull requests get
their own set of links posted automatically in the PR description.

## Building from source

BitDeck uses CMake + Qt6 Widgets, plus StormLib (vendored as a git submodule,
for `.otr`/MPQ archives) and libzip (system package, for `.o2r`/zip archives).

Clone with submodules, or if you already cloned without `--recurse-submodules`:
```bash
git submodule update --init
```

Pick your OS below.

### Linux

**Ubuntu / Debian**
```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  git \
  qt6-base-dev \
  qt6-base-dev-tools \
  libgl1-mesa-dev \
  libzip-dev \
  libpng-dev \
  libjpeg-turbo8-dev
```

**Fedora**
```bash
sudo dnf install -y \
  gcc-c++ \
  cmake \
  ninja-build \
  git \
  qt6-qtbase-devel \
  mesa-libGL-devel \
  libzip-devel \
  libpng-devel \
  libjpeg-turbo-devel
```

**Arch**
```bash
sudo pacman -S --needed \
  base-devel \
  cmake \
  ninja \
  git \
  qt6-base \
  libzip \
  libpng \
  libjpeg-turbo
```

Then build:
```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/BitDeck
```

### macOS

Install Xcode Command Line Tools and Qt6 via Homebrew:
```bash
xcode-select --install
brew install cmake ninja qtbase libzip libpng jpeg-turbo
```
(`qtbase` rather than the full `qt` meta-formula — it's all BitDeck's Widgets UI
needs, and keeps unrelated modules like QtPdf/QtSvg/QtVirtualKeyboard out of
`macdeployqt`'s way when packaging.)

Homebrew's Qt is keg-only, so point CMake at it explicitly:
```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"
cmake --build build
open build/BitDeck.app
```

### Windows

1. Install [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/) (or full Visual Studio) with the **Desktop development with C++** workload. This provides the MSVC compiler and CMake; check the **C++ CMake tools for Windows** component too, which also bundles Ninja.
2. Install Qt6 via the [Qt Online Installer](https://www.qt.io/download-qt-installer) or [aqtinstall](https://github.com/miurahr/aqtinstall), selecting the MSVC 2022 64-bit kit.
3. Install [vcpkg](https://vcpkg.io) and get libzip/libpng/libjpeg-turbo through it (no plain installers for these on Windows):
```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install libzip:x64-windows libpng:x64-windows libjpeg-turbo:x64-windows
```

From a "Developer PowerShell for VS" prompt:
```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build build
.\build\BitDeck.exe
```
(Adjust the Qt and vcpkg paths to match your install locations.)

## Tests

Each layer of the port has its own test executable under [`tests/`](tests/).
They're written as readable, descriptive checks (plain `[PASS]`/`[FAIL]`
output per assertion, not just a pass/fail count) so they double as a
runnable demonstration of what each piece actually does.

| Executable | Covers |
|---|---|
| `bitdeckcore_tests` | OTR resource header, Texture/Background/Sequence binary formats, SHA-256, BK64 conventions |
| `image_codec_tests` | PNG (RGBA + palette) and JPEG decode/encode round-trips |
| `texture_conversion_tests` | HD texture scaling/tiling pipeline (`exactMultiple`/`padCanvas`, additive font glyphs, palette rejection) |
| `bitdeckarchive_tests` | `.otr`/`.o2r` archive read/write, zip-slip extraction safety |
| `app_worker_tests` | Background worker threading helper (progress/error/finished signals) |
| `archive_generator_tests` | Full archive generation from staged entries, including HD texture resizing end-to-end |
| `texture_extraction_tests` | OTR -> PNG + `manifest.json` extraction pipeline |

Run everything via CTest after building:
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Or run a single executable directly for its full descriptive output:
```bash
./build/texture_conversion_tests
```
(`build\texture_conversion_tests.exe` on Windows.)

## VS Code

Complete the OS-specific dependency install steps above first (compiler,
CMake, Ninja, Qt6, libzip) — CMake Tools wraps the same `cmake`
configure/build, it doesn't install system packages on its own.

Install the **CMake Tools** extension (`ms-vscode.cmake-tools`) and the
**C/C++** extension (`ms-vscode.cpptools`).

1. Open the repo root folder in VS Code.
2. CMake Tools detects `CMakeLists.txt` and prompts you to pick a kit (your
   compiler) and configure. Choose Ninja as the generator when asked, or set
   it explicitly (see below).
3. If Qt6/libzip/vcpkg aren't auto-discoverable (macOS Homebrew, Windows
   vcpkg — see the OS-specific `-D` flags above), add them as
   `cmake.configureArgs` in `.vscode/settings.json`:
   ```json
   {
     "cmake.generator": "Ninja",
     "cmake.configureArgs": [
       "-DCMAKE_PREFIX_PATH=/path/to/qt",
       "-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
     ]
   }
   ```
   (Linux distro packages usually need none of this — plain configure works.)
4. Build/run/debug from the CMake Tools status bar at the bottom of the
   window, or via the Command Palette: `CMake: Build`, `CMake: Run Without
   Debugging`, `CMake: Debug`.
5. Tests: `CMake: Run Tests` (wraps ctest) -- see [Tests](#tests) above.

`.vscode/` is gitignored, so these settings stay local to your machine.

## Notes

- If `find_package(Qt6)` fails with `Could not find a package configuration file provided by "Qt6"`, Qt6 isn't installed or `CMAKE_PREFIX_PATH`/`Qt6_DIR` isn't pointing at it. Linux distro packages register themselves automatically; macOS (Homebrew) and Windows (Qt installer) do not, and need the `CMAKE_PREFIX_PATH` shown above.
