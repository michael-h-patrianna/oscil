/**
 * @file LayoutManager.h
 * @brief Layout management system for multi-track oscilloscope visualization
 *
 * This file defines the LayoutManager class which handles different display modes
 * for multi-track audio visualization including overlay, split, and grid layouts.
 * Provides smooth transitions between layouts and track assignment management.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2025
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <map>
#include <memory>

namespace oscil::ui::layout {

/**
 * @enum LayoutMode
 * @brief Available layout modes for multi-track visualization
 */
enum class LayoutMode {
    Overlay,    ///< All tracks overlaid in same region (default)
    Split2H,    ///< Two horizontal regions (top/bottom)
    Split2V,    ///< Two vertical regions (left/right)
    Split4,     ///< Four quadrant regions
    Grid2x2,    ///< 2x2 grid (4 regions)
    Grid3x3,    ///< 3x3 grid (9 regions)
    Grid4x4,    ///< 4x4 grid (16 regions)
    Grid6x6,    ///< 6x6 grid (36 regions)
    Grid8x8     ///< 8x8 grid (64 regions)
};

/**
 * @struct LayoutRegion
 * @brief Defines a single region within a layout
 */
struct LayoutRegion {
    juce::Rectangle<float> bounds;                   ///< Region bounds in component coordinates
    std::vector<int> assignedTracks;                ///< Track indices assigned to this region
    bool isActive = true;                           ///< Whether region should be rendered
    juce::Colour backgroundColor = juce::Colours::transparentBlack;  ///< Optional background color

    LayoutRegion() = default;
    LayoutRegion(const juce::Rectangle<float>& regionBounds) : bounds(regionBounds) {}

    /**
     * @brief Adds a track to this region
     * @param trackIndex Zero-based track index to assign
     */
    void addTrack(int trackIndex);

    /**
     * @brief Removes a track from this region
     * @param trackIndex Track index to remove
     * @return true if track was found and removed
     */
    bool removeTrack(int trackIndex);

    /**
     * @brief Checks if a track is assigned to this region
     * @param trackIndex Track index to check
     * @return true if track is assigned to this region
     */
    bool hasTrack(int trackIndex) const;

    /**
     * @brief Gets the number of tracks assigned to this region
     * @return Number of assigned tracks
     */
    size_t getNumTracks() const { return assignedTracks.size(); }

    /**
     * @brief Clears all track assignments from this region
     */
    void clearTracks() { assignedTracks.clear(); }
};

/**
 * @struct LayoutConfiguration
 * @brief Complete layout configuration including regions and assignments
 */
struct LayoutConfiguration {
    LayoutMode mode = LayoutMode::Overlay;          ///< Current layout mode
    std::vector<LayoutRegion> regions;              ///< All regions in the layout
    float regionSpacing = 2.0f;                     ///< Spacing between regions in pixels
    bool showRegionBorders = false;                 ///< Whether to draw region borders
    juce::Colour borderColor = juce::Colours::white.withAlpha(0.2f);  ///< Border color

    /**
     * @brief Gets the number of regions in this layout
     * @return Number of layout regions
     */
    size_t getNumRegions() const { return regions.size(); }

    /**
     * @brief Finds which region contains a specific track
     * @param trackIndex Track index to search for
     * @return Region index, or -1 if track not found
     */
    int findRegionForTrack(int trackIndex) const;

    /**
     * @brief Gets all tracks assigned to a specific region
     * @param regionIndex Zero-based region index
     * @return Vector of track indices (empty if invalid region)
     */
    std::vector<int> getTracksForRegion(int regionIndex) const;
};

/**
 * @class LayoutManager
 * @brief Manages layout modes and track assignments for multi-track visualization
 *
 * The LayoutManager handles all aspects of layout management including:
 * - Layout mode switching with smooth transitions
 * - Track assignment to layout regions
 * - Layout bounds calculation for different modes
 * - Animation and transition management
 * - State persistence for layout preferences
 *
 * Key Features:
 * - 9 different layout modes (overlay, split, grid)
 * - Smooth animated transitions between layouts
 * - Flexible track assignment to regions
 * - Performance-optimized layout calculations
 * - Thread-safe operation for UI updates
 * - State persistence across sessions
 *
 * Performance Characteristics:
 * - Layout calculations: O(regions) complexity
 * - Transition animations: 100ms target duration
 * - Memory usage: <1MB for maximum layout complexity
 * - Track assignment updates: <1ms latency
 *
 * Example Usage:
 * @code
 * LayoutManager layoutManager;
 * layoutManager.setComponentBounds({0, 0, 800, 600});
 * layoutManager.setLayoutMode(LayoutMode::Grid4x4);
 *
 * // Assign tracks to specific regions
 * layoutManager.assignTrackToRegion(0, 0);  // Track 0 to region 0
 * layoutManager.assignTrackToRegion(1, 1);  // Track 1 to region 1
 *
 * // Get layout for rendering
 * auto config = layoutManager.getCurrentLayout();
 * @endcode
 */
class LayoutManager {
public:
    /**
     * @brief Constructs layout manager with default configuration
     */
    LayoutManager();

    /**
     * @brief Destructor
     */
    ~LayoutManager() = default;

    //==============================================================================
    // Layout Mode Management

    /**
     * @brief Sets the current layout mode
     *
     * Changes the layout mode and triggers recalculation of all regions.
     * If transitions are enabled, starts smooth animation to new layout.
     *
     * @param mode New layout mode to apply
     * @param animated Whether to animate the transition (default: true)
     *
     * @post Layout regions are recalculated for new mode
     * @post Track assignments are preserved where possible
     * @post Animation starts if enabled and mode changed
     */
    void setLayoutMode(LayoutMode mode, bool animated = true);

    /**
     * @brief Gets the current layout mode
     * @return Current layout mode
     */
    LayoutMode getLayoutMode() const { return currentConfig.mode; }

    /**
     * @brief Checks if a layout transition is currently in progress
     * @return true if transition animation is active
     */
    bool isTransitioning() const { return isAnimating; }

    //==============================================================================
    // Component Integration

    /**
     * @brief Sets the component bounds for layout calculations
     *
     * Updates the available space for layout regions. Triggers recalculation
     * of all region bounds based on current layout mode.
     *
     * @param bounds Available component bounds for layouts
     *
     * @post All layout regions are recalculated
     * @post Cached layout data is invalidated
     */
    void setComponentBounds(const juce::Rectangle<float>& bounds);

    /**
     * @brief Gets the current component bounds
     * @return Component bounds used for layout calculations
     */
    juce::Rectangle<float> getComponentBounds() const { return componentBounds; }

    //==============================================================================
    // Track Assignment Management

    /**
     * @brief Assigns a track to a specific layout region
     *
     * Moves a track to the specified region, removing it from any other
     * region. If region index is invalid, track assignment is removed.
     *
     * @param trackIndex Zero-based track index (0-63)
     * @param regionIndex Zero-based region index
     * @return true if assignment was successful
     *
     * @note Track is automatically removed from previous region
     * @note Invalid region index removes track from all regions
     */
    bool assignTrackToRegion(int trackIndex, int regionIndex);

    /**
     * @brief Removes a track from all regions
     *
     * @param trackIndex Track index to remove
     * @return true if track was found and removed
     */
    bool removeTrackFromAllRegions(int trackIndex);

    /**
     * @brief Finds which region contains a specific track
     *
     * @param trackIndex Track index to search for
     * @return Region index, or -1 if track not assigned
     */
    int findRegionForTrack(int trackIndex) const;

    /**
     * @brief Gets all tracks assigned to a specific region
     *
     * @param regionIndex Zero-based region index
     * @return Vector of track indices (empty if invalid region)
     */
    std::vector<int> getTracksForRegion(int regionIndex) const;

    /**
     * @brief Automatically distributes tracks across available regions
     *
     * Distributes the specified number of tracks evenly across all
     * available regions in the current layout mode.
     *
     * @param numTracks Number of tracks to distribute (1-64)
     *
     * @post Tracks are evenly distributed across regions
     * @post Previous track assignments are cleared
     */
    void autoDistributeTracks(int numTracks);

    //==============================================================================
    // Layout Access

    /**
     * @brief Gets the current layout configuration
     *
     * Returns the complete layout configuration including all regions,
     * track assignments, and layout parameters.
     *
     * @return Current layout configuration
     *
     * @note Configuration reflects current animation state during transitions
     */
    const LayoutConfiguration& getCurrentLayout() const { return currentConfig; }

    /**
     * @brief Gets a specific layout region by index
     *
     * @param regionIndex Zero-based region index
     * @return Pointer to region, or nullptr if invalid index
     */
    const LayoutRegion* getRegion(int regionIndex) const;

    /**
     * @brief Gets the number of regions in current layout
     * @return Number of layout regions
     */
    size_t getNumRegions() const { return currentConfig.getNumRegions(); }

    //==============================================================================
    // Animation and Transitions

    /**
     * @brief Sets transition animation duration
     *
     * @param durationMs Transition duration in milliseconds (default: 100ms)
     */
    void setTransitionDuration(int durationMs) { transitionDurationMs = durationMs; }

    /**
     * @brief Gets current transition duration
     * @return Transition duration in milliseconds
     */
    int getTransitionDuration() const { return transitionDurationMs; }

    /**
     * @brief Enables or disables transition animations
     *
     * @param enabled Whether transitions should be animated
     */
    void setTransitionsEnabled(bool enabled) { transitionsEnabled = enabled; }

    /**
     * @brief Checks if transitions are enabled
     * @return true if transition animations are enabled
     */
    bool areTransitionsEnabled() const { return transitionsEnabled; }

    //==============================================================================
    // State Management

    /**
     * @brief Loads layout configuration from ValueTree
     *
     * Restores layout mode, track assignments, and layout parameters
     * from previously saved state.
     *
     * @param layoutState ValueTree containing layout configuration
     * @return true if state was loaded successfully
     */
    bool loadFromState(const juce::ValueTree& layoutState);

    /**
     * @brief Saves layout configuration to ValueTree
     *
     * Stores current layout mode, track assignments, and parameters
     * for persistence across sessions.
     *
     * @return ValueTree containing complete layout state
     */
    juce::ValueTree saveToState() const;

    //==============================================================================
    // Static Utility Methods

    /**
     * @brief Gets the number of regions for a specific layout mode
     *
     * @param mode Layout mode to query
     * @return Number of regions in the specified mode
     */
    static int getNumRegionsForMode(LayoutMode mode);

    /**
     * @brief Converts layout mode to string representation
     *
     * @param mode Layout mode to convert
     * @return String representation of layout mode
     */
    static juce::String layoutModeToString(LayoutMode mode);

    /**
     * @brief Converts string to layout mode
     *
     * @param modeString String representation of layout mode
     * @return Layout mode, or Overlay if string invalid
     */
    static LayoutMode stringToLayoutMode(const juce::String& modeString);

private:
    //==============================================================================
    // Internal State

    LayoutConfiguration currentConfig;              ///< Current layout configuration
    juce::Rectangle<float> componentBounds;         ///< Available component bounds

    // Animation state
    bool isAnimating = false;                       ///< Whether transition is in progress
    bool transitionsEnabled = true;                ///< Whether transitions are enabled
    int transitionDurationMs = 100;                ///< Transition duration in milliseconds

    // Cached calculations
    bool layoutNeedsRecalculation = true;           ///< Whether layout needs recalculation

    //==============================================================================
    // Internal Methods

    /**
     * @brief Recalculates layout regions for current mode and bounds
     *
     * Updates all region bounds based on current layout mode and component
     * bounds. Called automatically when mode or bounds change.
     *
     * @post All regions have updated bounds
     * @post Layout cache is marked as valid
     */
    void recalculateLayout();

    /**
     * @brief Calculates regions for overlay mode
     *
     * @param bounds Available component bounds
     * @return Vector containing single region with full bounds
     */
    std::vector<LayoutRegion> calculateOverlayLayout(const juce::Rectangle<float>& bounds) const;

    /**
     * @brief Calculates regions for split modes
     *
     * @param bounds Available component bounds
     * @param mode Split mode (Split2H, Split2V, Split4)
     * @return Vector of regions for split layout
     */
    std::vector<LayoutRegion> calculateSplitLayout(const juce::Rectangle<float>& bounds, LayoutMode mode) const;

    /**
     * @brief Calculates regions for grid modes
     *
     * @param bounds Available component bounds
     * @param mode Grid mode (Grid2x2 through Grid8x8)
     * @return Vector of regions for grid layout
     */
    std::vector<LayoutRegion> calculateGridLayout(const juce::Rectangle<float>& bounds, LayoutMode mode) const;

    /**
     * @brief Preserves track assignments when changing layouts
     *
     * Attempts to maintain track assignments when switching between layouts
     * with different numbers of regions. Excess tracks are distributed.
     *
     * @param newRegions New layout regions to populate with track assignments
     */
    void preserveTrackAssignments(std::vector<LayoutRegion>& newRegions) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayoutManager)
};

} // namespace oscil::ui::layout
