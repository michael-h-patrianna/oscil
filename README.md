# Oscil - Multi-channel Oscilloscope Effect (VST3)

Oscil is a cross-platform VST3 analyzer effect that displays a highly customizable multi-track oscilloscope with a modern, responsive UI. Built with JUCE and CMake, designed for stability across major DAWs and hosts.

## Features

- VST3 effect, multi-channel waveform visualization
- Modern, resizable UI
- Cross-platform: macOS, Windows, Linux (x86_64)
- TDD-ready: Catch2 unit tests and CTest integration
- Linting: clang-tidy and clang-format configs

## Requirements

- CMake >= 3.20
- A C++20 toolchain
- JUCE is fetched automatically; VST3 SDK is handled via JUCE

On macOS: Xcode or clang
On Windows: Visual Studio 2022 Build Tools and CMake
On Linux: gcc/clang with ALSA/JACK dev packages

## Configure & Build

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build plugin and tests
cmake --build build --config Release

# Run unit tests
ctest --test-dir build -C Release --output-on-failure
```

The built VST3 will be copied to your system VST3 folder after build (COPY_PLUGIN_AFTER_BUILD=TRUE). On macOS it ends up under `~/Library/Audio/Plug-Ins/VST3/`.

## Development Tips

- Edit code in `src/`
- Tests live in `tests/`
- You can enable sanitizers by building Debug and leaving `OSCIL_ENABLE_SANITIZERS=ON` (default)
- Use clang-tidy via CMake by keeping `OSCIL_ENABLE_CLANG_TIDY=ON`

## Roadmap

- Customizable oscilloscope settings (triggering, timebase, colors, averaging)
- HiDPI/Theme polish
- Windows + Linux CI and packaging
- Audio Unit (AUv3) and standalone target optional
