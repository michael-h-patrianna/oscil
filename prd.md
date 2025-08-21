# Oscil Multi-Track Oscilloscope Plugin - Technical Implementation Specification

## Overview

**Product**: Oscil - Multi-Track Audio Oscilloscope Plugin
**Technical Target**: VST3/AU/AAX plugin supporting 64 simultaneous audio tracks
**Core Functionality**: Real-time waveform visualization with cross-DAW compatibility
**Implementation Framework**: JUCE 7.x, C++20, CMake build system

## Technical Requirements Summary

### Rendering Backend Decision
The product adopts bgfx as the rendering backend abstraction (Metal on macOS, Direct3D12 on Windows, Vulkan where available, OpenGL only as fallback).

Implications / Requirements:

- CMake integration of bgfx, bx, bimg via FetchContent or subtree.
- Shader build step invoking bgfx shaderc, producing per-backend binaries.
- `RendererBackend` interface plus `BgfxRendererBackend` implementation.
- Config flags: `OSCIL_BGFX_FORCE_BACKEND`, `OSCIL_BGFX_DEBUG`.
- Runtime fallback path when initialization fails (reduced performance acceptable but functional for validation).

### Performance Specifications
- **Maximum Tracks**: 64 simultaneous audio inputs
- **Refresh Rate**: 60fps minimum, 120fps target
- **CPU Usage**: ≤5% with 16 active tracks (Intel i7-9750H baseline)
- **Memory Usage**: ≤10MB per track, 640MB maximum total
- **Latency**: ≤10ms processing delay
- **Sample Rates**: 44.1kHz to 192kHz support
- **Buffer Sizes**: 32 to 2048 samples

### Platform Compatibility Matrix
| Platform | Architecture | Plugin Formats | Min OS Version |
|----------|-------------|----------------|----------------|
| macOS | Intel x64 | VST3, AU | 10.15+ |
| macOS | Apple Silicon | VST3, AU | 11.0+ |
| Windows | x64 | VST3, AAX | Windows 10+ |

### DAW Compatibility Requirements
- Ableton Live 11.x, 12.x
- Logic Pro 10.x, 11.x
- Cubase/Nuendo 12.x, 13.x, 14.x
- Pro Tools 2023.x, 2024.x
- Studio One 5.x, 6.x
- Bitwig Studio 4.x, 5.x
- Reaper 6.x, 7.x

## Implementation Specifications

### Core Features - Implementation Order

#### F001: Multi-Track Visualization Engine

**Implementation Priority**: CRITICAL - Core functionality
**Dependencies**: None
**Estimated Complexity**: High

**Functional Requirements**:

- Maximum concurrent tracks: 64
- Minimum refresh rate: 60fps
- Waveform resolution: 44.1kHz-192kHz sample rate support
- Buffer size compatibility: 32-2048 samples
- Memory footprint: <10MB per track

**User Interface Requirements**:

- Track selection dropdown with DAW track names
- Layout switcher: Overlay, Split, Grid (2x2, 3x3, 4x4, 6x6, 8x8), Tabbed
- Per-track color assignment and visibility toggles
- Track naming/labeling system with user-defined aliases
- Drag-and-drop track reordering in multi-pane layouts

**Required Architecture Components**:

- MultiTrackEngine class with track management
- TrackData structure for individual track state
- Layout management system with multiple display modes
- Color assignment and visibility control system
- Track naming and alias management

#### F002: Cross-DAW Plugin Compatibility

**Implementation Priority**: CRITICAL - Must work in all target DAWs
**Dependencies**: F001 (MultiTrackEngine)
**Estimated Complexity**: Medium

**Required DAW Support**:

- **Ableton Live**: 11.x, 12.x (VST3, AU on macOS)
- **Logic Pro**: 10.x, 11.x (AU primary, VST3 secondary)
- **Cubase/Nuendo**: 12.x, 13.x, 14.x (VST3, AAX)
- **Pro Tools**: 2023.x, 2024.x (AAX, VST3)
- **Studio One**: 5.x, 6.x (VST3)
- **Bitwig Studio**: 4.x, 5.x (VST3)
- **Reaper**: 6.x, 7.x (VST3, AU)

**Technical Implementation Requirements**:

- Plugin format compliance: VST3 SDK 3.7.7+, AU SDK current, AAX SDK 2.4.1+
- State serialization: XML-based with backward compatibility
- Parameter automation: All UI controls must be automatable
- Preset management: DAW-native preset system integration
- Multi-instance support: Inter-instance communication for track sharing

**Required Architecture Components**:

- OscilPluginProcessor inheriting from juce::AudioProcessor
- State persistence system with XML serialization
- Parameter automation framework
- Multi-instance communication manager
- DAW-specific compatibility layer

#### F003: Signal Processing Modes

**Implementation Priority**: HIGH - Essential for professional use
**Dependencies**: F001, F002
**Estimated Complexity**: Medium

**Required Processing Modes**:

1. **Full Stereo**: Left and right channels displayed separately with phase correlation
2. **Mono Sum**: L+R summed signal analysis
3. **Mid/Side**: M/S matrix decoding with separate mid and side visualization
4. **Left Channel Only**: Isolated left channel analysis
5. **Right Channel Only**: Isolated right channel analysis
6. **Difference (L-R)**: Phase difference analysis for stereo width assessment

**Processing Requirements**:

- Real-time audio block processing
- Mathematical precision for M/S conversion: M = (L+R)/2, S = (L-R)/2
- Correlation coefficient calculation between channels
- Stereo width measurement and display
- Processing mode switching without audio interruption

**Required Architecture Components**:

- SignalProcessor class with mode enumeration
- ProcessedOutput structure with correlation metrics
- Channel processing algorithms for each mode
- Real-time switching capability between modes

#### F004: Timing and Synchronization System

**Implementation Priority**: HIGH - Required for professional workflow
**Dependencies**: F002 (plugin host communication)
**Estimated Complexity**: High

**Required Time Base Options**:

- **Free Running**: Continuous scrolling independent of DAW timing
- **Host Sync**: Locked to DAW transport and tempo
- **Time-Based Windows**: 1ms - 60s configurable windows
- **Musical Windows**: 1/32 note to 16 bars based on host BPM
- **Trigger Modes**: Level, edge, slope, and manual triggering

**Synchronization Requirements**:

- Sample-accurate timing with DAW transport
- BPM change response within one audio buffer period
- Trigger threshold accuracy ±1 sample
- Musical timing calculations for beat-synchronized windows
- Manual trigger capability with immediate response

**Required Architecture Components**:

- TimingEngine class with multiple time base modes
- TimingConfig structure for mode-specific settings
- Host BPM tracking and synchronization
- Trigger detection algorithms
- Display buffer size calculation

#### F005: Modern Theming System

**Implementation Priority**: MEDIUM - Important for user experience
**Dependencies**: F001 (UI components)
**Estimated Complexity**: Medium

**Required Built-in Themes**:

1. **Dark Professional**: Charcoal background, bright waveforms, minimal chrome
2. **Dark Blue**: Navy-tinted dark theme with blue accent colors
3. **Pure Black**: OLED-optimized pure black background
4. **Light Modern**: Clean white background with subtle shadows
5. **Light Warm**: Cream-colored background with warm accent tones
6. **Classic Green**: Hardware oscilloscope emulation with green phosphor
7. **Classic Amber**: Retro amber monochrome display simulation

**Theme System Requirements**:

- Instant theme switching without visual artifacts
- Support for 64 distinct waveform colors per theme
- Custom theme creation and editing capability
- Theme import/export in JSON format
- Per-project and global theme preferences

**Required Architecture Components**:

- ColorTheme structure with comprehensive color definitions
- ThemeManager class for loading and applying themes
- Theme persistence system
- JSON import/export functionality
- Theme application to UI components

#### F006: Project Integration & State Management

**Implementation Priority**: HIGH - Required for DAW integration
**Dependencies**: F002, F005
**Estimated Complexity**: Medium

**Required State Management Features**:

- Per-project plugin state persistence
- Global user preference storage
- Preset system with categorization
- Import/export of complete configurations
- Version migration for backward compatibility

**State Persistence Requirements**:

- Exact restoration of all plugin settings
- Cross-DAW compatibility for state files
- Backward compatibility with previous versions
- Atomic save/load operations to prevent corruption
- Error recovery for corrupted state files

**Required Architecture Components**:

- OscilState class with nested state structures
- ProjectState for session-specific settings
- GlobalPreferences for user-wide settings
- Version migration system
- State validation and error recovery

## Acceptance Criteria & Testing Requirements

### Test Environment Specifications

**Reference Hardware**:

- macOS: 2019 MacBook Pro (Intel i7-9750H, 16GB RAM)
- Windows: Surface Pro 9 (Intel i5-1235U, 16GB RAM)
- DAW Versions: Latest stable releases of all supported DAWs

**Test Audio Material**:

- 1kHz sine wave at -12dBFS (mono and stereo)
- White noise at -20dBFS
- Stereo test file with L channel 1kHz, R channel 440Hz
- Mid/Side test file with identical mono content
- Complex music content (reference tracks)

### AC001: Multi-Track Engine Functionality

**Requirement**: Track management system with precise behavior limits

**Test Scenario 1.1: Basic Track Operations**

GIVEN: Fresh MultiTrackEngine instance
WHEN: Adding tracks sequentially up to maximum limit
THEN:
- Must accept exactly 64 tracks maximum
- Track 65 addition must fail gracefully
- Each track must maintain independent state
- Active track count must be accurate at all times
- Track removal must decrement count correctly

**Test Scenario 1.2: Performance Under Load**

GIVEN: 16 active tracks with different audio signals
WHEN: Processing continuous updates for 1000 frames
THEN:
- Average frame time must be <16.67ms (60fps minimum)
- CPU usage must remain <5% on reference hardware
- Memory usage must stay <160MB total (10MB per track)
- No memory leaks detected over test duration
- Frame rate must remain stable throughout test

### AC002: DAW Integration Validation

**Requirement**: Perfect state persistence across all supported DAWs

**Test Scenario 2.1: State Persistence Accuracy**

GIVEN: Plugin with configured state (tracks, colors, theme, timing)
WHEN: State is saved and restored in new instance
THEN:
- All track configurations must be identical
- Theme settings must be preserved exactly
- Timing configuration must match original
- No data loss or corruption allowed
- State restoration must complete within 100ms

**Test Scenario 2.2: Parameter Automation Precision**

GIVEN: Plugin with automatable parameters
WHEN: Parameters are changed via DAW automation
THEN:
- All UI controls must respond to automation
- Parameter changes must be sample-accurate
- No audio artifacts during parameter changes
- Automation must work identically across all DAW formats
- Parameter ranges must be consistent across DAWs

### AC003: Signal Processing Accuracy

**Requirement**: Mathematically perfect audio processing with no artifacts

**Test Scenario 3.1: Mid/Side Conversion Precision**

GIVEN: Test signals with known characteristics
WHEN: Processing in Mid/Side mode
THEN:
- Mono content: Mid channel = full signal, Side channel = silence
- Stereo content: M = (L+R)/2, S = (L-R)/2 exactly
- Conversion accuracy within 0.001 tolerance
- No processing artifacts introduced
- Processing latency ≤2ms

**Test Scenario 3.2: Correlation Coefficient Accuracy**

GIVEN: Test signals with known correlation relationships
WHEN: Calculating stereo correlation
THEN:
- Identical signals must show correlation = 1.0 (±0.01)
- Inverted signals must show correlation = -1.0 (±0.01)
- Uncorrelated signals must show correlation ≈ 0.0 (±0.1)
- Calculation must be real-time capable
- Results must be stable over time

### AC004: Timing System Precision

**Requirement**: Sample-accurate synchronization with host DAW

**Test Scenario 4.1: Host Sync Accuracy**

GIVEN: DAW playing at variable BPM with musical timing windows
WHEN: BPM changes occur during playback
THEN:
- Window size must adjust within one audio buffer period
- Beat alignment must remain sample-accurate
- No visual artifacts during tempo changes
- Synchronization accuracy within ±1ms
- Musical timing calculations must be exact

**Test Scenario 4.2: Trigger Mode Stability**

GIVEN: Audio signal with periodic impulses above threshold
WHEN: Using triggered timing mode
THEN:
- Trigger detection accuracy within ±1 sample
- No false triggers from noise below threshold
- Consistent triggering on repeated signals
- Manual trigger must work immediately
- Trigger timing jitter <1ms

### AC005: Theme System Performance

**Requirement**: Instant visual updates without rendering interruption

**Test Scenario 5.1: Theme Application Speed**

GIVEN: Plugin actively displaying multiple waveforms
WHEN: Switching between built-in themes
THEN:
- Theme change must complete within 50ms
- No visual artifacts or flicker during transition
- All UI elements must update simultaneously
- Waveform rendering must continue uninterrupted
- Theme persistence must work across sessions

### AC006: Maximum Load Performance

**Requirement**: Stable operation at absolute maximum capacity

**Test Scenario 6.1: 64-Track Stress Test**

GIVEN: System loaded with 64 tracks of complex audio
WHEN: Running continuous operation for extended period
THEN:
- Frame rate must maintain ≥30fps minimum (target 60fps)
- CPU usage must stay <16% (64 tracks × 0.25% each)
- Memory usage must not exceed 640MB total
- No dropped frames allowed
- No memory leaks over 60-minute test period
- System must remain responsive for other operations

### AC007: Cross-Platform Consistency

**Requirement**: Identical behavior across all supported platforms

**Test Scenario 7.1: Platform Behavioral Parity**

GIVEN: Identical project setup on macOS and Windows
WHEN: Processing same audio content
THEN:
- Visual output must be pixel-perfect identical
- Performance characteristics within 10% between platforms
- All measurements must produce identical results
- File format compatibility must be 100%
- UI behavior must be equivalent across platforms

### AC008: Build System Requirements

**Requirement**: Automated build system supporting all target platforms

**Build Requirements**:

- CMake 3.20+ configuration for cross-platform builds
- JUCE 7.x framework integration
- VST3, AU, AAX plugin format support
- C++20 standard compliance
- Automated testing integration
- Release and debug build configurations

**Platform-Specific Requirements**:

- macOS: Universal binary (Intel + Apple Silicon)
- Windows: x64 architecture only
- Plugin validation: All formats must pass official validation tools
- Code signing: Plugins must be properly signed for distribution
