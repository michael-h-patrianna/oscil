/**
 * @file TrackSelectorComponent.h
 * @brief Track selection and management UI component for multi-track oscilloscope
 *
 * This file implements the TrackSelectorComponent which provides comprehensive
 * track management functionality including track selection from DAW channels,
 * custom track naming, drag-and-drop reordering, bulk operations, and visual
 * configuration. The component is designed to handle up to 64 simultaneous
 * tracks efficiently while maintaining responsive UI performance.
 *
 * Key Features:
 * - DAW track selection via dropdown interface (up to 1000+ tracks)
 * - Custom track naming with aliasing support
 * - Drag-and-drop track reordering within oscilloscope view
 * - Bulk operations: show all/hide all tracks
 * - Per-track color assignment and visibility management
 * - Integration with existing MultiTrackEngine and state persistence
 * - Theme-aware visual styling with accessibility support
 *
 * Performance Targets:
 * - UI operations complete within 100ms for up to 64 tracks
 * - Memory efficient track list management
 * - Zero audio thread interruption during track operations
 * - Responsive interaction during heavy audio processing
 *
 * Integration Points:
 * - MultiTrackEngine for track data and management
 * - TrackState system for persistence and configuration
 * - ThemeManager for consistent visual styling
 * - OscilloscopeComponent for visibility coordination
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
#include <memory>

// Forward declarations
class OscilAudioProcessor;
namespace audio { class MultiTrackEngine; }
namespace oscil::theme { class ThemeManager; }
namespace oscil::state { class TrackState; }
namespace oscil::ui::layout { class LayoutManager; }

/**
 * @class TrackSelectorComponent
 * @brief Comprehensive track selection and management UI component
 *
 * Provides a complete track management interface for the multi-track oscilloscope
 * plugin. Handles track selection from DAW channels, custom naming, visibility
 * control, reordering via drag-and-drop, and bulk operations. The component
 * integrates seamlessly with the existing audio engine and state management
 * systems while maintaining optimal performance with up to 64 tracks.
 *
 * UI Layout:
 * ```
 * [Add Track Dropdown ▼] [Track Name Field] [Color] [👁] [≡]
 * [Add Track Dropdown ▼] [Track Name Field] [Color] [👁] [≡]
 * ...
 * [Show All] [Hide All] [Clear All]
 * ```
 *
 * Functionality:
 * - **Track Selection**: ComboBox showing available DAW input channels
 * - **Custom Naming**: Editable track names with persistence across sessions
 * - **Color Management**: Per-track color assignment from theme palette
 * - **Visibility Control**: Individual track show/hide with immediate effect
 * - **Drag-and-Drop**: Reorder tracks within the oscilloscope display
 * - **Bulk Operations**: Show all, hide all, clear all tracks
 *
 * State Management:
 * - Track configurations persist across plugin reload/DAW restart
 * - Custom names and colors stored in ValueTree system
 * - Visibility state coordinated with OscilloscopeComponent
 * - Track assignments integrated with LayoutManager regions
 *
 * Performance Characteristics:
 * - Sub-100ms response time for all track operations
 * - Memory efficient with pre-allocated UI elements
 * - Zero heap allocations during normal operation
 * - Scales linearly with track count up to 64 tracks
 *
 * @see audio::MultiTrackEngine
 * @see oscil::state::TrackState
 * @see oscil::theme::ThemeManager
 */
class TrackSelectorComponent : public juce::Component,
                              public juce::DragAndDropContainer,
                              public juce::DragAndDropTarget {
public:
    //==============================================================================
    // Construction and Lifecycle

    /**
     * @brief Constructs the track selector component
     *
     * Initializes all UI elements, connects to the audio processor systems,
     * and sets up the complete track management interface. Prepares for
     * handling up to 64 simultaneous tracks with optimal performance.
     *
     * @param processor Reference to the audio processor for system integration
     *
     * @post Component is fully initialized and ready for display
     * @post All UI elements are created and properly configured
     * @post Integration with MultiTrackEngine and state systems complete
     */
    explicit TrackSelectorComponent(OscilAudioProcessor& processor);

    /**
     * @brief Destructor
     *
     * Safely releases all resources and disconnects from processor systems.
     *
     * @post All UI resources properly released
     * @post No dangling references to processor systems
     */
    ~TrackSelectorComponent() override = default;

    //==============================================================================
    // JUCE Component Overrides

    /**
     * @brief Renders the component interface
     *
     * Draws the track selector UI including track list, control buttons,
     * and any visual feedback for drag-and-drop operations.
     *
     * @param g Graphics context for drawing operations
     */
    void paint(juce::Graphics& g) override;

    /**
     * @brief Handles component resize events
     *
     * Repositions and resizes all child components to fit the new bounds.
     * Implements responsive layout that adapts to different container sizes.
     *
     * @post All child components properly positioned and sized
     * @post Layout optimized for current component dimensions
     */
    void resized() override;

    //==============================================================================
    // Track Management Interface

    /**
     * @brief Refreshes the track list from current engine state
     *
     * Updates the track selector UI to reflect the current state of the
     * MultiTrackEngine, including any newly added or removed tracks.
     * Should be called after external track modifications.
     *
     * @post UI reflects current MultiTrackEngine state
     * @post Track list is synchronized with audio system
     */
    void refreshTrackList();

    /**
     * @brief Gets the number of managed tracks
     *
     * Returns the count of tracks currently being managed by this component.
     *
     * @return Number of active tracks (0-64)
     */
    [[nodiscard]] int getNumTracks() const;

    /**
     * @brief Shows all tracks simultaneously
     *
     * Bulk operation to make all managed tracks visible in the oscilloscope
     * display. More efficient than calling individual track visibility methods.
     *
     * @post All tracks are set to visible state
     * @post Oscilloscope display updated to show all tracks
     */
    void showAllTracks();

    /**
     * @brief Hides all tracks simultaneously
     *
     * Bulk operation to hide all managed tracks from the oscilloscope display.
     * Audio capture continues but tracks are not visually rendered.
     *
     * @post All tracks are set to hidden state
     * @post Oscilloscope display updated to hide all tracks
     */
    void hideAllTracks();

    /**
     * @brief Removes all tracks from management
     *
     * Clears all managed tracks, stopping audio capture and removing from
     * the oscilloscope display. Does not affect DAW routing.
     *
     * @post All tracks removed from MultiTrackEngine
     * @post Track selector UI reset to empty state
     * @post Memory usage reduced to minimum
     */
    void clearAllTracks();

    //==============================================================================
    // DAW Integration

    /**
     * @brief Gets available DAW input channel names
     *
     * Retrieves the list of available input channels from the DAW for
     * track selection. Channel names are obtained from the audio processor.
     *
     * @return Vector of channel names available for track creation
     *
     * @note Return value may change based on DAW routing configuration
     * @note Maximum supported channels depends on DAW capabilities
     */
    [[nodiscard]] std::vector<juce::String> getAvailableChannelNames() const;

    /**
     * @brief Sets the maximum number of input channels to display
     *
     * Configures the maximum number of DAW input channels that will be
     * shown in the track selection dropdown. Useful for managing large
     * channel counts in professional DAW environments.
     *
     * @param maxChannels Maximum channels to display (1-1000)
     *
     * @pre maxChannels must be positive
     * @post Channel dropdown limited to specified count
     */
    void setMaxDisplayChannels(int maxChannels);

    //==============================================================================
    // Theme Integration

    /**
     * @brief Sets the theme manager for visual styling
     *
     * Connects the component to the theme management system for consistent
     * visual styling and color management. Must be called before displaying.
     *
     * @param manager Pointer to the theme manager (non-null)
     *
     * @pre manager must be valid and initialized
     * @post Component uses theme colors and styling
     * @post Track colors synchronized with theme palette
     */
    void setThemeManager(oscil::theme::ThemeManager* manager);

    /**
     * @brief Sets the layout manager for track region assignment
     *
     * Connects the component to the layout management system for coordinating
     * track assignments to display regions in different layout modes.
     *
     * @param manager Pointer to the layout manager (can be null)
     *
     * @post Track region assignments coordinated with layout system
     * @post Drag-and-drop operations respect layout regions
     */
    void setLayoutManager(oscil::ui::layout::LayoutManager* manager);

    //==============================================================================
    // Drag and Drop Support

    /**
     * @brief Checks if items can be dropped on this component
     *
     * DragAndDropTarget interface implementation for accepting track
     * reordering operations via drag-and-drop.
     *
     * @param sourceDetails Information about the dragged item
     * @return true if the item can be accepted for dropping
     */
    bool isInterestedInDragSource(const SourceDetails& sourceDetails) override;

    /**
     * @brief Handles item drop operations
     *
     * Processes completed drag-and-drop operations to reorder tracks
     * within the selector and update the oscilloscope display accordingly.
     *
     * @param sourceDetails Information about the dropped item
     */
    void itemDropped(const SourceDetails& sourceDetails) override;

    /**
     * @brief Provides visual feedback during drag operations
     *
     * Updates the component appearance to show where items can be dropped
     * and provides visual feedback for the drag-and-drop operation.
     *
     * @param sourceDetails Information about the dragged item
     * @param position Current position of the drag operation
     */
    void itemDragEnter(const SourceDetails& sourceDetails) override;

    /**
     * @brief Cleans up drag operation visual feedback
     *
     * Removes visual indicators when drag operations exit the component
     * area or are completed.
     *
     * @param sourceDetails Information about the dragged item
     */
    void itemDragExit(const SourceDetails& sourceDetails) override;

private:
    //==============================================================================
    // Internal Track Management

    /**
     * @struct TrackEntry
     * @brief Internal representation of a managed track
     */
    struct TrackEntry {
        juce::String trackId;                    ///< Unique track identifier
        juce::String displayName;               ///< User-configurable track name
        juce::String dawChannelName;            ///< Original DAW channel name
        int channelIndex = -1;                  ///< Input channel index
        int colorIndex = 0;                     ///< Color palette index
        bool isVisible = true;                  ///< Visibility state

        // UI Components for this track
        std::unique_ptr<juce::ComboBox> channelSelector;
        std::unique_ptr<juce::TextEditor> nameEditor;
        std::unique_ptr<juce::TextButton> colorButton;
        std::unique_ptr<juce::TextButton> visibilityButton;
        std::unique_ptr<juce::TextButton> dragHandle;

        TrackEntry() = default;
        ~TrackEntry() = default;

        // Non-copyable but movable
        TrackEntry(const TrackEntry&) = delete;
        TrackEntry& operator=(const TrackEntry&) = delete;
        TrackEntry(TrackEntry&&) = default;
        TrackEntry& operator=(TrackEntry&&) = default;
    };

    /**
     * @brief Creates a new track entry with UI components
     *
     * @param channelIndex DAW channel index for this track
     * @param name Initial track name
     * @return Unique pointer to the created track entry
     */
    std::unique_ptr<TrackEntry> createTrackEntry(int channelIndex, const juce::String& name);

    /**
     * @brief Updates track entry UI from current state
     *
     * @param entry Track entry to update
     */
    void updateTrackEntryUI(TrackEntry& entry);

    /**
     * @brief Removes a track entry and updates UI
     *
     * @param trackIndex Index of track to remove
     */
    void removeTrackEntry(int trackIndex);

    /**
     * @brief Reorders track entries via drag-and-drop
     *
     * @param fromIndex Source track index
     * @param toIndex Destination track index
     */
    void reorderTracks(int fromIndex, int toIndex);

    /**
     * @brief Applies theme styling to all UI elements
     */
    void updateThemeColors();

    /**
     * @brief Lays out all track entry components
     */
    void layoutTrackEntries();

    /**
     * @brief Lays out bulk operation buttons
     */
    void layoutBulkOperationButtons(juce::Rectangle<int> area);

    /**
     * @brief Lays out header labels
     */
    void layoutHeaderLabels(juce::Rectangle<int> area);

    //==============================================================================
    // Member Variables

    /// Reference to the audio processor for system integration
    OscilAudioProcessor& processor;

    /// Theme manager for visual styling (can be null)
    oscil::theme::ThemeManager* themeManager = nullptr;

    /// Layout manager for region coordination (can be null)
    oscil::ui::layout::LayoutManager* layoutManager = nullptr;

    /// List of managed track entries
    std::vector<std::unique_ptr<TrackEntry>> trackEntries;

    /// Maximum channels to show in DAW selection
    int maxDisplayChannels = 64;

    /// Drag and drop state
    int dragSourceIndex = -1;
    int dragTargetIndex = -1;
    bool isDragActive = false;

    //==============================================================================
    // Bulk Operation Controls

    juce::TextButton showAllButton;
    juce::TextButton hideAllButton;
    juce::TextButton clearAllButton;
    juce::TextButton addTrackButton;

    /// Header labels
    juce::Label channelHeaderLabel;
    juce::Label nameHeaderLabel;
    juce::Label colorHeaderLabel;
    juce::Label visibilityHeaderLabel;

    /// Viewport for scrolling track list
    juce::Viewport trackListViewport;
    juce::Component trackListContainer;

    //==============================================================================
    // Layout Constants

    static constexpr int HEADER_HEIGHT = 25;
    static constexpr int TRACK_ROW_HEIGHT = 30;
    static constexpr int BUTTON_ROW_HEIGHT = 35;
    static constexpr int COMPONENT_MARGIN = 5;
    static constexpr int COLUMN_SPACING = 10;

    // Column widths
    static constexpr int CHANNEL_COLUMN_WIDTH = 120;
    static constexpr int NAME_COLUMN_WIDTH = 150;
    static constexpr int COLOR_COLUMN_WIDTH = 40;
    static constexpr int VISIBILITY_COLUMN_WIDTH = 40;
    static constexpr int DRAG_HANDLE_WIDTH = 20;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackSelectorComponent)
};
