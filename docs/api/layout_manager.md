# Layout Manager API Documentation

## Overview

The Layout Manager system provides flexible multi-track visualization layouts for the Oscil oscilloscope plugin. It supports overlay, split, and grid layout modes with configurable track assignments and smooth transitions.

## Key Classes

### LayoutManager

The central manager class responsible for coordinate layout calculations, track assignments, and state persistence.

#### Layout Modes

- **Overlay**: All tracks rendered in a single viewport (1 region)
- **Split2H**: Horizontal split creating 2 stacked regions
- **Split2V**: Vertical split creating 2 side-by-side regions
- **Split4**: Quadrant split creating 4 equal regions
- **Grid2x2**: 2×2 grid (4 regions)
- **Grid3x3**: 3×3 grid (9 regions)
- **Grid4x4**: 4×4 grid (16 regions)
- **Grid6x6**: 6×6 grid (36 regions)
- **Grid8x8**: 8×8 grid (64 regions)

#### Key Methods

```cpp
// Layout mode management
void setLayoutMode(LayoutMode mode, bool animate = true);
LayoutMode getLayoutMode() const;
int getNumRegions() const;

// Track assignment
bool assignTrackToRegion(int trackIndex, int regionIndex);
int findRegionForTrack(int trackIndex) const;
std::vector<int> getTracksForRegion(int regionIndex) const;
void autoDistributeTracks(int numTracks);

// Region access
const LayoutRegion* getRegion(int index) const;
const LayoutConfiguration& getCurrentLayout() const;

// State persistence
juce::ValueTree saveToState() const;
bool loadFromState(const juce::ValueTree& state);

// Transitions
void enableTransitions(bool enable);
bool areTransitionsEnabled() const;
void setTransitionDuration(int milliseconds);
```

### LayoutRegion

Represents a single layout region with bounds, track assignments, and visual properties.

```cpp
struct LayoutRegion {
    juce::Rectangle<float> bounds;          // Region coordinates
    std::vector<int> assignedTracks;        // Track indices in this region
    bool isActive = true;                   // Whether region is enabled
    juce::Colour backgroundColor;           // Optional background color
    int index = 0;                          // Region index
};
```

### LayoutConfiguration

Container for complete layout state including all regions and visual settings.

```cpp
struct LayoutConfiguration {
    LayoutMode mode;                        // Current layout type
    std::vector<LayoutRegion> regions;      // All layout regions
    bool showRegionBorders = false;         // Region border visibility
    juce::Colour borderColor;               // Border color
    float regionSpacing = 2.0f;             // Space between regions

    int getNumRegions() const;
    void clear();
};
```

## Integration with OscilloscopeComponent

The layout manager integrates with the main rendering component through:

```cpp
class OscilloscopeComponent {
    // Layout integration
    void setLayoutManager(std::unique_ptr<LayoutManager> manager);

    // Layout-aware rendering
    void renderMultiRegionLayout(juce::Graphics& graphics, int numChannels);
    void renderLayoutRegion(juce::Graphics& graphics,
                           const LayoutRegion& region,
                           int regionIndex);
    void renderChannelInRegion(juce::Graphics& graphics,
                              int channelIndex,
                              int regionChannelIndex,
                              const float* samples,
                              size_t sampleCount);
};
```

## Performance Characteristics

- **Layout switching**: < 100ms transitions for all modes
- **Track assignment**: < 1ms for 64 track operations
- **State persistence**: < 1ms save/load operations
- **Memory usage**: Minimal overhead with region pooling

## Usage Examples

### Basic Layout Setup

```cpp
auto layoutManager = std::make_unique<LayoutManager>();
layoutManager->setComponentBounds({0, 0, 800, 600});
layoutManager->setLayoutMode(LayoutMode::Grid2x2);

// Assign tracks to regions
layoutManager->assignTrackToRegion(0, 0);  // Track 0 to top-left
layoutManager->assignTrackToRegion(1, 1);  // Track 1 to top-right
layoutManager->assignTrackToRegion(2, 2);  // Track 2 to bottom-left
layoutManager->assignTrackToRegion(3, 3);  // Track 3 to bottom-right

oscilloscopeComponent.setLayoutManager(std::move(layoutManager));
```

### Auto-Distribution

```cpp
// Automatically distribute 8 tracks across current layout
layoutManager->autoDistributeTracks(8);
```

### State Persistence

```cpp
// Save layout state
auto state = layoutManager->saveToState();
applicationState.addChild(state, -1, nullptr);

// Restore layout state
auto savedState = applicationState.getChildWithName("Layout");
layoutManager->loadFromState(savedState);
```

### Custom Layout Properties

```cpp
auto& layout = layoutManager->getCurrentLayout();
layout.showRegionBorders = true;
layout.borderColor = juce::Colours::grey;
layout.regionSpacing = 4.0f;
```

## Thread Safety

The Layout Manager is designed for single-threaded use on the message thread. All layout operations should be performed from the GUI thread.

## Error Handling

- Invalid track indices (< 0 or >= 64) are ignored
- Invalid region indices return nullptr/false
- Malformed state data fails gracefully
- Component bounds validation prevents crashes

## See Also

- [Architecture Overview](../design/architecture.md)
- [Multi-Track Engine](multitrack_engine.md)
- [Performance Monitoring](performance_monitor.md)
