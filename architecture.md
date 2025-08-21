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

## 2. High-Level Layered Model

```text
+-----------------------------------------------------------+
|            Host / DAW Integration Layer (JUCE)            |
+-------------------------+---------------------------------+
|   Plugin Processor      |       Editor / UI Root          |
| (Audio Thread, RT Core) |  (Message Thread + bgfx submit) |
+-------------------------+---------------------------------+
|   Real-Time Engine Subsystems (Lock-Free Data Flow)       |
|   - MultiTrackEngine / TrackCapture                       |
|   - SignalProcessor (modes)                               |
|   - TimingEngine (sync, triggers)                         |
+-----------------------------------------------------------+
|              Rendering & Visualization Layer              |
|   - WaveformDataBridge (intermediate snapshot)           |
|   - WaveformGeometryBuilder (background prep)            |
|   - RendererBackend (WaveformCanvas + bgfx buffers)      |
+-----------------------------------------------------------+
|            State, Theme, Layout & Interaction Layer       |
|   - OscilState (ValueTree)                                |
|   - TrackState, LayoutState, ThemeState                   |
|   - ThemeManager, LayoutManager                          |
|   - Command / Action system (optional future)             |
+-----------------------------------------------------------+
|                Utilities & Infrastructure                 |
|   - Lock-free containers, ring buffers, profiling         |
|   - Build system (CMake), Tests (Catch2)                  |
+-----------------------------------------------------------+
```

## 3. Core Modules & Responsibilities

| Module | Purpose | Thread Context | Key Constraints |
|--------|---------|---------------|-----------------|
| AudioProcessor (PluginProcessor) | Entry point, audio I/O, invokes RT subsystems | Audio | No blocking, no heap per block |
| MultiTrackEngine | Manages N track capture buffers | Audio | Lock-free writes |
| TrackCapture / RingBuffer | Per-track audio sample storage (~200 ms) | Audio | Wait-free overwrite-oldest |
| SignalProcessor | Stereo modes, correlation, mid/side, difference | Audio | Branch-light, SIMD-ready |
| TimingEngine | Transport sync, window sizing, trigger detection | Audio | Sample-accurate decisions |
| WaveformDataBridge | Copies reduced data to UI-safe structure | Audio→Bridge | Single-producer / single-consumer |
| GeometryBuilder (background) | Decimates & converts samples to vertices | Background Worker | Avoids UI stalls |
| RendererBackend (WaveformCanvas + Renderers) | Draws batched track waveforms via bgfx | Message (bgfx submit) | Minimize state changes / submit calls |
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
  Timer / Async trigger -> Drain new packets -> aggregate per track -> schedule background build

[Background Geometry Thread]
  Build/decimate sample arrays -> produce vertex buffers (reuse pooled VBO IDs)
  -> Post completion (message thread) -> mark track dirty

[GPU Render Pass]
  On render callback: for dirty tracks update transient/dynamic vertex buffers & issue minimal bgfx::submit calls (instancing / batching)
```

### Concurrency Primitives

- Single-producer/single-consumer ring for audio→UI (atomic head/tail)
- Atomic flags for track dirty state
- Pooled buffers (fixed-capacity freelists) to avoid new/delete churn
- JUCE MessageThread enqueues minimal work; heavy lifting offloaded to geometry thread

## 5. Performance Strategy

| Concern | Strategy |
|---------|----------|
| CPU Scaling (64 tracks) | Per-track decimation & LOD; dynamic sample density based on zoom |
| Memory Cap | Preallocate ring buffers sized by max window; reuse geometry & VBO pools |
| Draw Calls | Batch by line style/color using interleaved vertex format + glMultiDrawArrays |
| Latency | Push-based bridge; UI wakes ≤ one frame interval after audio write |
| GC / Allocations | Pre-size ValueTree nodes; custom small object pools (optional phase 2) |
| Cache Locality | SoA for amplitude arrays; contiguous per-track metadata block |
| SIMD | Optional vectorized min/max & decimation (AVX2 / NEON) |

## 6. Directory & File Structure (Proposed)

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
      WaveformCanvasComponent.h
      WaveformCanvasComponent.cpp
      WaveformRenderer.h                # per-track logic & VBO mgmt
      WaveformRenderer.cpp
      GeometryBuilder.h
      GeometryBuilder.cpp
  RendererBackend.h                 # interface
  BgfxRendererBackend.cpp           # implementation
  BgfxShaders.h                     # embedded / loaded shader handles
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
    shaders/                            # if not embedding

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

## 8. Rendering Pipeline Detail

1. Audio writes raw (and optionally mode-processed) samples to per-track ring.
2. Periodic UI tick requests snapshot window (duration derived from TimingEngine config).
3. Decimation & min/max pair extraction (LODs) done in GeometryBuilder thread:

  - Input: contiguous float span (interleaved or per channel)
  - Outputs: Vertex buffer (line strip or line list with degenerates for breaks)

4. RendererBackend (bgfx) batches contiguous tracks:

  - Shared shader program (uniform block for global transforms)
  - Per-track color in uniform array or per-vertex attribute (compact RGBA8)
  - LOD selection selects stride (e.g., 1x, 2x, 4x samples) based on pixel width / sample count ratio.

5. Overlays (grid, cursor, measurement) drawn second pass (or same pass with layered z or alpha) to minimize state changes.

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
- JUCE modules via FetchContent or subdirectory (already bootstrapped)
- Integrate bgfx (and dependencies bx, bimg) via FetchContent or external project.
- Configuration flags:
  - `OSCIL_ENABLE_PROFILING`
  - `OSCIL_ENABLE_ASSERTS`
  - `OSCIL_BGFX_FORCE_BACKEND` (metal|vulkan|d3d12|opengl|auto)
  - `OSCIL_BGFX_DEBUG`
  - `OSCIL_MAX_TRACKS` (default 64)
- Platform abstraction in `util/Platform.h` (OS & compiler macros)
- Shader build: invoke bgfx shaderc at configure/build; outputs platform-specific binaries in `resources/shaders/bin/`.
- Shader loading: runtime load & create bgfx program handles cached in `BgfxShaders.h/cpp` or embed compressed blobs.

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
| Cross-backend feature gaps (bgfx vs native APIs) | Visual fidelity regression | Keep abstraction thin; allow optional native path later |
| Excessive CPU at 64 tracks | Miss perf targets | Early perf harness, LOD + batching, profiling gates in CI |
| State schema churn | Migration complexity | Central `Versioning` unit tests + semantic version gating |
| Thread contention on UI updates | Frame drops | SPSC queue + background geometry thread |
| Correlation calc overhead | Real-time cost | Incremental sums + optional per-frame throttle |

## 17. Mapping to Task Phases

| Phase Task | Architecture Element |
|------------|----------------------|
| 1.4 Rendering foundation (bgfx) | `render/` (WaveformCanvas, basic renderer backend) |
| 2.1 Audio↔UI comms | `AtomicRingQueue`, `WaveformDataBridge` |
| 2.4 Perf optimization | GeometryBuilder, LOD, batching |
| 3.1 Multi-Track Engine | `MultiTrackEngine`, TrackCapture |
| 3.2 Multi-Track Rendering | Multi-draw batching in renderer |
| 3.3 Layout System | `LayoutManager`, grid containers |
| 4.x Signal/Timing | `SignalProcessor`, `TimingEngine` |
| 5.x Themes | `ThemeManager`, `ColorTheme` |
| 6.x State/Persistence | `OscilState`, `Serialization` |

## 18. Incremental Implementation Path (Recommended Ordering)

1. Restructure directories; move existing code into `plugin/`, `render/`, `audio/` segments.
2. Introduce `MultiTrackEngine` (initially single track facade wrapping existing ring buffer).
3. Add bgfx pipeline (RendererBackend abstraction + basic line rendering; retain software fallback path behind flag if needed).
4. Implement `WaveformDataBridge` + simple geometry builder (single-threaded first; add background thread later).
5. Add `SignalProcessor` & correlation metrics.
6. Integrate `TimingEngine` (basic modes → advanced triggers later).
7. Introduce `OscilState` / `TrackState` & migration skeleton.
8. Theme system & expanded UI components.
9. Performance tuning & LOD.
10. Extended layouts & multi-track scaling.

\n## 19. Acceptance Traceability
This architecture explicitly supports PRD acceptance criteria AC001–AC008 via isolated modules with testable seams (e.g., decimation, correlation, timing). Performance headroom ensured by early introduction of background geometry processing and batching strategies.

---
End of Architecture Document.
