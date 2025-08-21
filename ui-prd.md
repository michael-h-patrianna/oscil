# UI Component Library Plan for Oscil Multi-Track Oscilloscope Plugin

## Overview
Build a high-performance, modern UI component library for the Oscil multi-track oscilloscope plugin using JUCE 7.x with C++20, optimized for 64 simultaneous tracks at 60fps minimum performance. The architecture prioritizes real-time waveform rendering, efficient state management, and cross-DAW compatibility.

## Critical Performance Requirements

### Real-Time Constraints
- **Frame Rate**: 60fps minimum, 120fps target with 64 tracks
- **CPU Usage**: ≤5% with 16 tracks, ≤16% with 64 tracks
- **Memory**: ≤10MB per track, 640MB maximum total
- **Latency**: ≤10ms visual update latency
- **Refresh Rate**: Real-time waveform updates at audio buffer rate

### Multi-Track Display Requirements
- **Maximum Tracks**: 64 simultaneous audio visualization
- **Layout Modes**: Overlay, Split, Grid (2x2 to 8x8), Tabbed
- **Sample Rates**: 44.1kHz to 192kHz support
- **Buffer Sizes**: 32 to 2048 samples
- **Color Management**: 64 distinct waveform colors per theme

## Technology Stack

### Core Framework

- **JUCE 7.x**: Latest stable for plugin + windowing integration
- **C++20**: Concepts, ranges, coroutines for performance
- **bgfx**: Cross‑API GPU abstraction (Metal on macOS, D3D12 on Windows, Vulkan where available, OpenGL fallback)
- **SIMD**: Vectorized audio processing where available
- **Lock-free**: Audio-thread safe data structures

### Performance-Critical Components

- **bgfx context**: Hardware acceleration for waveform display (auto-select backend)
- **juce::AudioBuffer**: Efficient audio data handling
- **std::atomic**: Thread-safe state for real-time updates
- **juce::CriticalSection**: Minimal locking for UI updates
- **juce::HighResolutionTimer**: Precise timing for animations

## Architecture Overview

### 1. Component Hierarchy

```
OscilPluginEditor (Root)
├── HeaderBarComponent
│   ├── PresetSelectorComponent
│   ├── ThemeToggleComponent
│   └── SettingsMenuComponent
├── MainDisplayComponent
│   ├── MultiTrackOscilloscopeComponent (CRITICAL PERFORMANCE)
│   │   ├── TrackLayoutManagerComponent
│   │   ├── WaveformCanvasComponent (bgfx)
│   │   │   ├── WaveformRenderer (per track, up to 64)
│   │   │   ├── GridOverlayComponent
│   │   │   ├── CursorOverlayComponent
│   │   │   └── MeasurementOverlayComponent
│   │   └── TrackSelectorComponent
│   └── ControlPanelComponent
│       ├── TimingControlsComponent
│       │   ├── TimeBaseControlComponent
│       │   ├── TriggerControlComponent
│       │   └── WindowSizeControlComponent
│       ├── SignalProcessingComponent
│       │   ├── ProcessingModeSelector
│       │   └── CorrelationMeterComponent
│       └── DisplayControlsComponent
│           ├── ZoomControlComponent
│           ├── ColorPickerComponent
│           └── VisibilityToggleComponent
└── StatusBarComponent
    ├── PerformanceMonitorComponent
    ├── SampleRateDisplayComponent
    └── TrackCountIndicatorComponent
```

### 2. Critical Performance Components

#### MultiTrackOscilloscopeComponent
**Performance Requirements**: 60fps with 64 tracks, <16% CPU usage
**Responsibilities**:
- Coordinate rendering of up to 64 simultaneous waveforms
- Manage layout switching (Overlay, Split, Grid modes)
- Handle real-time audio data updates
- Optimize bgfx rendering pipeline (instancing, transient buffers)

#### WaveformCanvasComponent
**Performance Requirements**: Hardware-accelerated rendering, minimal CPU usage
**Responsibilities**:

- bgfx-based waveform rendering (Metal/D3D12/Vulkan auto backend)
- Vertex/instance buffer management for smooth waveforms
- Level-of-detail (LOD) optimization based on zoom
- Batch rendering for multiple tracks (instancing / multi-submit reduction)

#### WaveformRenderer (Per Track)
**Performance Requirements**: <0.25% CPU per track, <10MB memory per track
**Responsibilities**:
- Individual track waveform rendering
- Color and style management
- Visibility state handling
- Audio buffer to vertex data conversion

### 3. Real-Time Data Flow Architecture

#### Audio Thread Communication Requirements
- Lock-free data transfer from audio processing to UI rendering
- Circular buffer system with atomic operations for thread safety
- Maximum 64 simultaneous audio streams with independent buffering
- Buffer overflow protection with graceful degradation
- Sample-accurate timing preservation across thread boundaries

#### UI Update Optimization Strategy
- Target 120fps update rate with adaptive frame dropping to maintain 60fps minimum
- Level-of-detail (LOD) system reducing vertex count when zoomed out
- Batch rendering for multiple waveforms in single GPU call
- Dirty region tracking to update only changed screen areas
- Background thread preparation of vertex data

### 4. State Management Architecture

#### Multi-Track State Requirements
- ValueTree-based hierarchical state for 64 tracks maximum
- Cached value system for performance-critical UI bindings
- Atomic state updates to prevent UI flickering
- Preset system supporting partial and complete state snapshots
- Cross-DAW state compatibility with version migration

#### Per-Track State Components
- Track visibility and color assignment
- User-defined track names and aliases
- Individual gain and offset controls
- Line width and rendering style preferences
- Signal processing mode per track (if different from global)

#### Global State Components
- Layout mode selection (Overlay, Split, Grid configurations)
- Timing and synchronization settings
- Theme selection and custom theme data
- Performance monitoring and optimization settings
- Window and measurement tool configurations

### 5. Component Library Requirements

#### Base Interactive Components

**ModernButton Component**
- **Performance**: Sub-5ms click response, smooth hover animations
- **Usability**: Multiple visual styles (Primary, Secondary, Ghost, Icon), clear visual feedback for all states (normal, hover, pressed, disabled)
- **Accessibility**: Keyboard navigation support, screen reader compatibility
- **Variations**: Small/Medium/Large sizes, optional icon support, customizable colors per theme

**ModernSlider Component**
- **Performance**: Real-time value updates without frame drops, smooth dragging feel
- **Usability**: Linear and rotary styles, optional value display, unit labeling, snap-to-step functionality
- **Accessibility**: Arrow key increment/decrement, accessible value announcement
- **Precision**: Double-precision values with configurable ranges and steps

**ModernComboBox Component**
- **Performance**: Fast dropdown rendering with large item lists (up to 1000+ DAW tracks)
- **Usability**: Search/filter capability, keyboard navigation, recent selections
- **Scalability**: Virtualized rendering for large lists, lazy loading of items
- **Integration**: DAW track name synchronization, real-time track list updates

#### Layout Management Components

**FlexContainer Component**
- **Performance**: Efficient layout calculations, minimal reflow operations
- **Flexibility**: Row/Column direction, justify content options, alignment control
- **Responsiveness**: Auto-wrapping, gap control, stretch behaviors
- **Use Cases**: Control panels, button groups, responsive layouts

**GridContainer Component**
- **Performance**: Optimized for 8x8 grid layouts (64-track maximum)
- **Precision**: Exact track sizing, configurable gaps, overflow handling
- **Adaptability**: Dynamic column/row adjustments, aspect ratio preservation
- **Use Cases**: Multi-track waveform display grids, control matrices

#### Performance-Critical Display Components

**WaveformRenderer Component (Per Track)**

- **Performance**: <0.25% CPU per track, GPU rendering via bgfx
- **Memory**: <10MB per track, efficient transient / dynamic buffer management
- **Visual Quality**: Anti-aliased (geometry/thick line shader) lines, configurable line width (1-5px), smooth curves
- **Optimization**: Level-of-detail based on zoom level, instanced / batched submissions
- **Color Management**: Per-track color assignment, theme integration, alpha blending

**WaveformCanvasComponent (Container)**
- **Performance**: Coordinate up to 64 WaveformRenderer instances, maintain 60fps minimum
- **Layout**: Dynamic layout switching (Overlay, Split, Grid), smooth transitions
- **Interaction**: Zoom controls, pan operations, selection tools
- **Synchronization**: Real-time audio data updates, sample-accurate timing
- **Memory Management**: Efficient buffer pooling, automatic cleanup

**GridOverlayComponent**
- **Performance**: Minimal rendering overhead, cached grid lines
- **Customization**: Configurable divisions, color and alpha control
- **Integration**: Time/amplitude scale markers, musical timing alignment
- **Visibility**: Toggle on/off, fade animations, responsive to theme changes

**CursorOverlayComponent**
- **Performance**: Real-time cursor tracking, minimal CPU usage
- **Precision**: Sample-accurate positioning, snap-to-grid functionality
- **Visual Feedback**: Clear cursor lines, measurement display, delta calculations
- **Interaction**: Mouse and touch input, keyboard fine-tuning

#### Control Panel Components

**TimingControlsComponent**
- **TimeBaseControlComponent**: Free-running, Host-sync, Time-based, Musical, Trigger modes
- **TriggerControlComponent**: Level/edge/slope triggering, threshold control, manual trigger
- **WindowSizeControlComponent**: 1ms to 60s range, musical divisions (1/32 to 16 bars)
- **Performance**: Immediate mode switching, no audio interruption
- **Synchronization**: Sample-accurate DAW transport locking, BPM change response

**SignalProcessingComponent**
- **ProcessingModeSelector**: Full Stereo, Mono Sum, Mid/Side, L/R Only, Difference modes
- **CorrelationMeterComponent**: Real-time stereo correlation display, -1.0 to +1.0 range
- **Performance**: Real-time processing mode switching, mathematical precision
- **Visual Feedback**: Mode-specific UI adaptations, correlation visualization

**DisplayControlsComponent**
- **ZoomControlComponent**: Horizontal/vertical zoom, fit-to-window, zoom presets
- **ColorPickerComponent**: Per-track color selection, theme color palette
- **VisibilityToggleComponent**: Individual track show/hide, all/none toggles
- **Layout**: Efficient space usage, contextual control grouping

#### Information Display Components

**StatusBarComponent**
- **PerformanceMonitorComponent**: Real-time CPU/memory usage, frame rate display
- **SampleRateDisplayComponent**: Current sample rate, buffer size information
- **TrackCountIndicatorComponent**: Active tracks display, memory usage per track
- **Layout**: Compact information display, color-coded status indicators

#### Theme and Visual System

**ThemeManagementComponent**
- **Built-in Themes**: 7 predefined themes (Dark Professional, Dark Blue, Pure Black, Light Modern, Light Warm, Classic Green, Classic Amber)
- **Custom Themes**: Theme creation/editing interface, color picker integration
- **Performance**: Instant theme switching <50ms, no visual artifacts
- **Persistence**: Per-project and global theme preferences, import/export capability

**ColorSystemComponent**
- **Waveform Colors**: 64 distinct colors per theme, automatic contrast optimization
- **UI Colors**: Comprehensive color system (background, surface, text, accent, error)
- **Accessibility**: Color blindness considerations, high contrast mode support
- **Consistency**: Unified color application across all components

### 6. Animation and Interaction System

#### Animation Requirements
- **Performance**: Hardware-accelerated animations, 60fps minimum during transitions
- **Easing Functions**: Linear, EaseIn/Out, Bounce, Elastic for different interaction contexts
- **Property Animation**: Smooth transitions for colors, positions, scales, opacity
- **Layout Transitions**: Seamless switching between display modes (Overlay ↔ Grid ↔ Split)
- **Timing**: Theme transitions <50ms, layout changes <100ms, micro-animations <200ms

#### Interaction Design Principles
- **Immediate Feedback**: Visual response to all user interactions within 16ms (1 frame)
- **Progressive Disclosure**: Complex controls hidden behind contextual menus
- **Spatial Consistency**: Consistent spacing, alignment, and sizing across all components
- **Touch Support**: Finger-friendly touch targets (minimum 44px), gesture recognition
- **Keyboard Navigation**: Complete keyboard accessibility, logical tab order

### 7. Development Workflow and Structure

#### Component Development Phases
**Phase 1: Foundation Components (Weeks 1-2)**
- Reactive component base system with state binding
- Core layout components (FlexContainer, GridContainer)
- Basic theme system with 7 built-in themes
- Essential input components (ModernButton, ModernSlider, ModernComboBox)

**Phase 2: Performance-Critical Components (Weeks 3-4)**

- WaveformRenderer with bgfx acceleration
- WaveformCanvasComponent with 64-track support
- Real-time data flow architecture
- Lock-free audio-to-UI communication system

**Phase 3: Specialized Display Components (Weeks 5-6)**
- GridOverlayComponent with customizable divisions
- CursorOverlayComponent with measurement tools
- Multi-track layout management system
- Theme application and switching system

**Phase 4: Control Panel Components (Weeks 7-8)**
- TimingControlsComponent with all synchronization modes
- SignalProcessingComponent with real-time mode switching
- DisplayControlsComponent with zoom and color management
- StatusBarComponent with performance monitoring

**Phase 5: Integration and Polish (Weeks 9-10)**
- Animation system implementation
- Accessibility features and keyboard navigation
- Cross-platform testing and optimization
- Performance profiling and bottleneck elimination

#### File Organization Strategy
**Component Categories**:
- Base: Fundamental reactive components and layout managers
- Input: Interactive controls (buttons, sliders, combo boxes)
- Display: Visual components (waveforms, grids, overlays)
- Control: Specialized control panels and parameter management
- Theme: Visual styling, color management, animation systems
- State: State management, data binding, persistence systems

**Testing Strategy**:
- Unit tests for individual component behavior and performance
- Integration tests for multi-component interactions
- Performance tests for 64-track scenarios and memory usage
- Accessibility tests for keyboard navigation and screen readers
- Cross-platform tests for macOS (Intel/Apple Silicon) and Windows

#### Component Documentation Requirements
- Performance specifications for each component
- Usability guidelines and interaction patterns
- Integration examples with state management
- Accessibility implementation notes
- Cross-platform compatibility considerations

## Success Criteria and Validation

### Performance Validation
- **Frame Rate**: Sustained 60fps with 64 active tracks in complex layouts
- **CPU Usage**: <16% total with maximum track load on reference hardware
- **Memory Usage**: <640MB with 64 tracks, no memory leaks over extended sessions
- **Responsiveness**: <10ms UI response time for all interactive elements
- **Startup Time**: <2 seconds from plugin load to ready state

### Usability Validation
- **Learning Curve**: New users productive within 15 minutes
- **Efficiency**: Expert users can perform common tasks quickly
- **Error Prevention**: Clear feedback prevents configuration mistakes
- **Accessibility**: Full functionality via keyboard and screen readers
- **Cross-Platform**: Identical behavior and performance across supported platforms

### Technical Validation
- **State Management**: Perfect state persistence across DAW sessions
- **Theme System**: Instant theme switching without visual artifacts
- **Multi-Track**: Reliable operation with 64 simultaneous tracks
- **Synchronization**: Sample-accurate timing with DAW transport
- **Memory Safety**: No crashes or data corruption under stress conditions
