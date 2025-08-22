# Oscil Plugin Architecture

Version: 0.1 (Derived from `prd.md`, `tasks.md`, `ui-prd.md`)
Status: Draft — foundational decisions suitable for immediate refactor & incremental expansion.

## 1. Architecture Overview

Oscil is a high‑performance multi‑track oscilloscope audio plugin (VST3/AU, future AAX) targeting up to 64 simultaneous waveform streams at 60–120 FPS with low CPU (<16% @ 64 tracks) and low latency (≤10 ms visual).

The system is decomposed into *real‑time audio core*, *rendering pipeline*, *state & persistence*, *UI component library*, *theme & styling*, and *infrastructure (build, tests, tooling)*. Hard real‑time constraints isolate the audio thread from dynamic allocations, locks, and blocking I/O.

### Primary Quality Drivers

- Real‑time determinism (no audio dropouts)
- Rendering throughput & scalability (64 tracks)
- Memory efficiency & predictable allocation patterns
- Cross‑DAW format compatibility & robust state persistence
- Extensibility for later features (timing modes, signal processing, theming)

## 2. High-Level Layered Model (JUCE‑Only Rendering)

```text
+-----------------------------------------------------------+
|            Host / DAW Integration Layer (JUCE)            |
+-------------------------+---------------------------------+
|   Plugin Processor      |       Editor / UI Root          |
| (Audio Thread, RT Core) |  (Message Thread)               |
+-------------------------+---------------------------------+
|   Real-Time Engine Subsystems (Lock-Free Data Flow)       |
|   - MultiTrackEngine / TrackCapture                       |
|   - SignalProcessor (modes)                               |
|   - TimingEngine (sync, triggers)                         |
+-----------------------------------------------------------+
|        Rendering & Visualization (JUCE Graphics API)      |
|   - WaveformDataBridge (intermediate snapshot)            |
|   - (Optional) Decimation/Geometry helper                 |
|   - OscilloscopeComponent (paint; optional OpenGL ctx)    |
+-----------------------------------------------------------+
|            State, Theme, Layout & Interaction Layer       |
|   - OscilState (ValueTree)                                |
|   - TrackState, LayoutState, ThemeState                   |
|   - ThemeManager, LayoutManager                           |
+-----------------------------------------------------------+
|                Utilities & Infrastructure                 |
|   - Lock-free containers, ring buffers, profiling         |
|   - Build system (CMake), Tests (Catch2)                  |
+-----------------------------------------------------------+
```

The former renderer abstraction + bgfx backend has been removed. We rely solely on JUCE CPU drawing with an optional `juce::OpenGLContext` attachment for GPU compositing (no custom shaders).

## 3. Core Modules & Responsibilities

| Module | Purpose | Thread Context | Key Constraints |
|--------|---------|---------------|-----------------|
| AudioProcessor (PluginProcessor) | Entry point, audio I/O, invokes RT subsystems | Audio | No blocking, no heap per block |
| MultiTrackEngine | Manages N track capture buffers | Audio | Lock-free writes |
| TrackCapture / RingBuffer | Per-track audio sample storage (~200 ms) | Audio | Wait-free overwrite-oldest |
| SignalProcessor | Stereo modes, correlation, mid/side, difference | Audio | Branch-light, SIMD-ready |
| TimingEngine | Transport sync, window sizing, trigger detection | Audio | Sample-accurate decisions |
| WaveformDataBridge | Copies reduced data to UI-safe structure | Audio→Bridge | Single-producer / single-consumer |
| WaveformDecimator (background, optional) | Reduces samples (min/max, stride) to lightweight arrays | Background Worker | Avoids UI stalls |
| Rendering (OscilloscopeComponent) | CPU waveform drawing using JUCE Graphics (optionally accelerated by JUCE OpenGL context) | Message | Avoid blocking; minimize allocations |
| LayoutManager | Overlay / Split / Grid / Tabbed arrangement | UI | O(visibleTracks) layout cost |
| ThemeManager | Loads & applies themes, caches colors | UI |  <50 ms theme swap |
| State (OscilState) | ValueTree persistence, versioning | UI (atomic snapshot) | Safe serialization |
| PerformanceMonitor | Lightweight frame & CPU metrics | UI | No extra allocations per frame |

## 4. Data Flow & Threading Model

```text
[Audio Thread]
  host audio block -> MultiTrackEngine (per-track RingBuffer write)
                  -> SignalProcessor (mode transform optional)
                  -> TimingEngine (window/trigger decisions)
                  -> WaveformDataBridge.enqueueBlock(meta+samples)

[Bridge / Lock-Free Queue]
  SPSC wait-free queue or ring of fixed-size slots (atomic indices)

[Message Thread]
  Timer / Async trigger -> Drain new packets -> aggregate per track
                        -> (Optional) dispatch decimation worker (large data)
                        -> repaint() of OscilloscopeComponent

[Optional Background Worker]
  Decimate / compute min-max pairs -> light arrays for paint

[JUCE Paint]
  OscilloscopeComponent::paint iterates tracks and draws waveforms via JUCE Graphics.
  If an OpenGL context is attached, JUCE handles GPU compositing transparently.
```

### Concurrency Primitives

- Single-producer/single-consumer ring for audio→UI (atomic head/tail)
- Atomic flags for track dirty state
- Pooled buffers (fixed-capacity freelists) to avoid new/delete churn
- JUCE MessageThread enqueues minimal work; heavy lifting offloaded to optional decimation worker

## 5. Performance Strategy

| Concern | Strategy |
|---------|----------|
| CPU Scaling (64 tracks) | Per-track decimation & LOD; dynamic sample density based on zoom |
| Memory Cap | Preallocate ring buffers sized by max window; reuse decimation scratch buffers |
| Draw Cost | Minimize per-paint allocations; optional cached Paths or pre-decimated min/max pairs; optional JUCE OpenGL context for GPU compositing |
| Latency | Push-based bridge; UI wakes ≤ one frame interval after audio write |
| GC / Allocations | Pre-size ValueTree nodes; custom small object pools (optional phase 2) |
| Cache Locality | SoA for amplitude arrays; contiguous per-track metadata block |
| SIMD | Optional vectorized min/max & decimation (AVX2 / NEON) |

## 6. Directory & File Structure (Updated)

Current root already contains `src/`, `tests/`. We extend & segment logically.

```text
root/
  CMakeLists.txt
  CMakePresets.json
  architecture.md
  prd.md
  tasks.md
  ui-prd.md
  README.md
  LICENSE
  scripts/
    install_debug_vst3_macos.sh
    generate_version_header.py          # (future) auto versioning
    package_release.sh                  # (future) packaging
  cmake/
    JUCEConfig.cmake                    # (optional vendor overrides)
    Toolchains/                         # cross compile configs

  src/
    plugin/
      PluginProcessor.h
      PluginProcessor.cpp
      PluginEditor.h
      PluginEditor.cpp
      Version.h.in                      # configured at build
    audio/
      MultiTrackEngine.h
      MultiTrackEngine.cpp
      TrackCapture.h                    # (inline templated ring usage)
      SignalProcessor.h
      SignalProcessor.cpp
      TimingEngine.h
      TimingEngine.cpp
      ProcessingModes.h                 # enum & helpers
      WindowConfig.h
      Correlation.h / .cpp
    dsp/
      RingBuffer.h                      # existing
      Downsampler.h / .cpp (future LOD)
      SIMDUtils.h
    render/
      OscilloscopeComponent.h
      OscilloscopeComponent.cpp
      WaveformDecimator.h            # optional (min/max, stride)
      WaveformDecimator.cpp
      GridOverlayComponent.h
      GridOverlayComponent.cpp
      CursorOverlayComponent.h
      MeasurementOverlayComponent.h / .cpp
    state/
      OscilState.h
      OscilState.cpp
      TrackState.h
      LayoutState.h
      ThemeState.h
      Serialization.h / .cpp
      Versioning.h / .cpp
    theme/
      ThemeManager.h
      ThemeManager.cpp
      ColorTheme.h
      BuiltInThemes.cpp
      Palette.h
    ui/
      components/
        ModernButton.h / .cpp
        ModernSlider.h / .cpp
        ModernComboBox.h / .cpp
        PerformanceMonitorComponent.h / .cpp
        TrackSelectorComponent.h / .cpp
        LayoutModeSelectorComponent.h / .cpp
        SignalProcessingComponent.h / .cpp
        TimingControlsComponent.h / .cpp
        DisplayControlsComponent.h / .cpp
      layout/
        LayoutManager.h
        LayoutManager.cpp
        FlexContainer.h / .cpp (optional abstraction)
        GridContainer.h / .cpp
    util/
      AtomicRingQueue.h
      BufferPool.h
      Profiling.h
      ScopedTimer.h
      Logging.h
      Platform.h
      Assertions.h
      CompileOptions.h

  tests/
    CMakeLists.txt
    test_ringbuffer.cpp
    test_downsampler.cpp
    test_multitrackengine.cpp
    test_signalprocessor.cpp
    test_timingengine.cpp
    test_state_serialization.cpp
    perf/
      benchmark_geometry.cpp            # microbenchmarks (optional)

  resources/
    icons/
    themes/                             # optional JSON custom themes


  docs/
    api/                                # Doxygen / reference (future)
    design/                             # Additional design notes

  build/                                # (generated)
```

### Naming Conventions

- Headers: `PascalCase.h`; Implementations: `PascalCase.cpp`
- Enums: `EnumName`, values `PascalCase` (e.g., `ProcessingMode::MidSide`)
- Namespaces: `oscil::audio`, `oscil::render`, `oscil::state`, etc.
- Internal (non-exported) helpers in anonymous namespace within `.cpp`.

## 7. State Management Design

- Root: `ValueTree oscilState { "OscilState" }`
  - `Tracks` (child list) → per-track `ValueTree` with: id, name, colorIndex, visible, gain, offset
  - `Layout` → layoutMode, gridRows, gridCols, overlayOpacity
  - `Theme` → themeId, customThemeData (JSON blob if user-defined)
  - `Processing` → globalMode, correlationEnabled
  - `Timing` → mode, windowSizeMs, triggerType, triggerThreshold
  - `Version` → semantic version & schema

Serialization pipeline: ValueTree → XML (JUCE) for plugin state; separate JSON for theme export.
Version migration: `Versioning::migrate(ValueTree&)` invoked on load; N-to-latest transformations.

## 8. Rendering Pipeline Detail (JUCE‑Only)

1. Audio thread accumulates samples into per‑track ring buffers.
2. UI timer (~60 FPS) asks WaveformDataBridge for latest snapshot window.
3. Optional decimation (min/max pair, stride reduction) performed inline (few tracks) or via background worker (many tracks / high SR).
4. `OscilloscopeComponent::paint` maps samples (or min/max pairs) to y coordinates and draws using JUCE Graphics primitives (`Path`, `drawLine`, or polyline helper).
5. Overlays (grid, cursors, measurements) drawn after waveforms.
6. If `OSCIL_ENABLE_OPENGL` and a context is attached, JUCE performs compositing on GPU automatically—no plugin shader maintenance.

Removed: bgfx pipeline, custom shaders, vertex buffer management, submission batching complexity.

\n## 9. Signal Processing Modes
`ProcessingModes.h` enumerates: Stereo, MonoSum, MidSide, LeftOnly, RightOnly, Difference.
Optional correlation calculation: incremental covariance / energy accumulation per block.

\n## 10. Timing & Trigger System

TimingEngine responsibilities:

- Interpret host transport (via `AudioPlayHead`) each block.
- Maintain sample counter aligned to musical grid when in musical/windowed modes.
- Trigger detection: edge/level/slope using a small state struct per track (or per selected trigger channel) with hysteresis.
- Provide target window length (samples) to UI snapshot requests.

## 11. Extensibility Points

| Future Feature | Hook Point |
|----------------|-----------|
| AAX Support | Build system + plugin wrapper factory |
| Custom Themes Import | `ThemeManager::importTheme(json)` |
| Scripting / Automation | Action / Command dispatcher layer |
| Network / Remote Sync | Additional transport provider impl |
| Offline Export (PNG/Video) | Offscreen framebuffer path |

## 12. Testing Strategy

| Test Type | Focus | Location |
|-----------|-------|----------|
| Integration | MultiTrackEngine + Bridge correctness | `tests/` |
| Performance (micro) | Geometry build timing, correlation calc cost | `tests/perf` |
| Stress | 64 track memory & frame pacing simulation harness | (add script) |
| Serialization | Round-trip ValueTree version migrations | `test_state_serialization.cpp` |

Automate with CTest; optional performance tests excluded from default unless `-DENABLE_PERF_TESTS=ON`.

## 13. Build & Tooling

- C++20 (`target_compile_features(... cxx_std_20)`)
- JUCE modules via FetchContent (already bootstrapped)
- No external rendering library (BGFX removed)
- Configuration flags (current / planned):
  - `OSCIL_ENABLE_PROFILING`
  - `OSCIL_ENABLE_ASSERTS`
  - `OSCIL_ENABLE_OPENGL` (attach JUCE OpenGL context if ON)
  - `OSCIL_MAX_TRACKS` (default 64)
- Platform abstraction in `util/Platform.h` (OS & compiler macros)
- No shader compilation or external GPU asset pipeline required.

## 14. Memory Budget & Allocation Plan

| Item | Approx / Track | Strategy |
|------|----------------|----------|
| Audio Ring (float) | 200 ms \* 192 kHz \* 4 B ≈ 153 KB (stereo doubled) | Preallocate at sample rate change |
| Geometry Buffers | Up to ~64 KB typical (decimated) | Pool & reuse |
| Decimation Scratch | <32 KB | Thread-local buffer |
| State Metadata | <4 KB | Small ValueTree nodes |

## 15. Coding Guidelines (Concise)

- No allocations in audio callback; use pre-sized containers.
- Use `[[nodiscard]]` for functions returning status.
- Use `gsl::span`-like views (or custom) to avoid copies (optional).
- Prefer `enum class` over raw enums.
- Document complexity/perf notes in headers for critical paths.
- Use assertions only in debug builds; guard with `OSCIL_ENABLE_ASSERTS`.

## 16. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Optional OpenGL context overhead | Possible redundant CPU→GPU copies | Runtime toggle; disable if driver issues |
| Excessive CPU at 64 tracks | Miss perf targets | Early perf harness, LOD + batching, profiling gates in CI |
| State schema churn | Migration complexity | Central `Versioning` unit tests + semantic version gating |
| Thread contention on UI updates | Frame drops | SPSC queue + background geometry thread |
| Correlation calc overhead | Real-time cost | Incremental sums + optional per-frame throttle |

## 17. Mapping to Task Phases

| Phase Task | Architecture Element |
|------------|----------------------|
| 1.4 Rendering foundation | `render/` (OscilloscopeComponent) |
| 2.1 Audio↔UI comms | `AtomicRingQueue`, `WaveformDataBridge` |
| 2.4 Perf optimization | Decimation helper, LOD strategies |
| 3.1 Multi-Track Engine | `MultiTrackEngine`, TrackCapture |
| 3.2 Multi-Track Rendering | Optimized paint traversal / optional decimation |
| 3.3 Layout System | `LayoutManager`, grid containers |
| 4.x Signal/Timing | `SignalProcessor`, `TimingEngine` |
| 5.x Themes | `ThemeManager`, `ColorTheme` |
| 6.x State/Persistence | `OscilState`, `Serialization` |

## 18. Incremental Implementation Path (Recommended Ordering)

1. Restructure directories; move existing code into `plugin/`, `render/`, `audio/` segments.
2. Introduce `MultiTrackEngine` (initially single track facade wrapping existing ring buffer).
3. Implement JUCE oscilloscope rendering (CPU path; optional OpenGL context flag).
4. Implement `WaveformDataBridge` + optional decimation helper (single-threaded first; add background worker later if needed).
5. Add `SignalProcessor` & correlation metrics.
6. Integrate `TimingEngine` (basic modes → advanced triggers later).
7. Introduce `OscilState` / `TrackState` & migration skeleton.
8. Theme system & expanded UI components.
9. Performance tuning & LOD (optimize paint & decimation strategies).
10. Extended layouts & multi-track scaling.

\n## 19. GPU FX & Shader Extensibility (Planned)

Objective: Provide optional GPU-accelerated visual enhancements and user-selectable shader effects without reintroducing a heavy multi-backend layer.

Planned Stages:

- Stage A (Task 1.6): Optional `juce::OpenGLContext` attachment (no visual changes; performance baseline).
- Stage B (Task 1.7): Lightweight `GpuRenderHook` interface (beginFrame / drawWaveforms / postFx / endFrame) — no-op when disabled.
- Stage C (Task 4.5): Built-in FX passes (persistence trail, glow) implemented as simple post fragment programs operating on an offscreen texture.
- Stage D (Task 4.6): Developer / advanced user custom fragment shader hot-reload (guarded by dev flag) with sandboxed uniform set.

Design Principles:

- Zero divergence: Core waveform generation logic unchanged; FX only post-process composited image.
- Graceful fallback: Disable FX automatically if context fails or shader compile errors persist.
- Safety: Validate shader length, forbid disallowed GLSL keywords (e.g., imageStore if not needed) to reduce crash risk.
- Performance Budget: Post FX pass ≤10% of total frame time at 16 tracks; GPU path should reduce CPU compositing cost measurably at high resolutions.

Data Flow (GPU FX Enabled):

```text
paint() -> (if GL active) invoke GpuRenderHook.beginFrame()
         -> draw waveforms (CPU -> JUCE Graphics) OR future direct GL lines
         -> GpuRenderHook.applyPostFx(renderTargetTexture)
         -> GpuRenderHook.endFrame()
```

User Shader Contract (proposed fragment uniforms):

- sampler2D uWaveformTex
- vec2 uResolution
- float uTime
- float uDecay / uGlowIntensity
- vec4 uColorParams[4] (spare params)

Failure Handling:

- On compile failure: retain last good program; surface log (dev build UI panel).
- On persistent failures (>N in window): disable custom shader feature for session.

Additional Risks (added to §16 conceptually):

- User shader instability → mitigate with sandbox + fallback
- Overdraw / excessive effects → cap passes (max 2) and texture sizes.

## 20. Development Workflow & Audio Input Configuration

### Development Strategy

The Oscil project follows a **standalone-first development workflow** to optimize development velocity and testing efficiency:

**Primary Development Cycle:**
1. **Feature Development:** Build and test new features primarily in the standalone application
2. **Iterative Testing:** Use standalone app with microphone/line input for rapid prototyping
3. **Milestone Validation:** At key milestones, build and test all plugin formats (VST3/AU) in real DAWs

**Benefits of Standalone-First Approach:**
- Faster build times (no plugin wrapper overhead)
- Direct microphone input for immediate audio visualization testing
- Simplified debugging without DAW host complexity
- Rapid iteration cycles for UI and rendering development

### Audio Input Configuration

The system implements **conditional audio input routing** based on the JUCE wrapper type:

| Format | Audio Input Source | Use Case |
|--------|-------------------|----------|
| **Standalone App** | Microphone/Line Input | Development & standalone testing |
| **VST3/AU Plugins** | Host/DAW Audio Input | Production use in DAWs |

**Implementation Details:**
```cpp
// In PluginProcessor::processBlock()
bool isStandalone = (wrapperType == wrapperType_Standalone);

if (!isStandalone) {
    // Plugin mode: use audio input from DAW host
    // Store in ring buffers for oscilloscope display
} else {
    // Standalone mode: use microphone/audio device input
    // Store in ring buffers for oscilloscope display
}
```

### macOS Microphone Permissions

**Required Configuration:**
- CMakeLists.txt: `MICROPHONE_PERMISSION_ENABLED TRUE` in `juce_add_plugin()`
- Generates `NSMicrophoneUsageDescription` in Info.plist
- Ad-hoc code signing for proper permission dialogs

**Permission Flow:**
1. First launch of standalone app prompts for microphone access
2. User grants permission via macOS dialog
3. App can access microphone input for real-time visualization
4. Permissions persist for subsequent launches

**Troubleshooting:**
- Check System Preferences > Privacy & Security > Microphone
- Ensure "Oscil" app is listed and enabled
- Try ad-hoc signing: `codesign --force --sign - Oscil.app`

### Development Milestones

**Phase 1 Milestones:** Core functionality in standalone
- Basic oscilloscope visualization
- Multi-track ring buffer management
- Real-time rendering performance

**Phase 2 Milestones:** Plugin validation
- VST3/AU build and installation
- DAW integration testing (Ableton Live, Bitwig Studio)
- Host automation and state persistence

**Phase 3 Milestones:** Advanced features
- Signal processing modes (mid/side, correlation)
- Timing and trigger systems
- Theme and layout management

## 19. Acceptance Traceability
This architecture explicitly supports PRD acceptance criteria AC001–AC008 via isolated modules with testable seams (e.g., decimation, correlation, timing). Performance headroom ensured by optional background decimation and lightweight paint paths.

---
Removed BGFX integration — simplification reduces maintenance, build time, and eliminates cross-backend complexity.

End of Architecture Document.
