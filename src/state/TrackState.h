/**
 * @file TrackState.h
 * @brief Track state management and serialization for oscilloscope tracks
 *
 * This file contains the TrackState class that manages persistent state
 * for individual oscilloscope tracks including visual properties, settings,
 * and serialization capabilities using JUCE ValueTree.
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_core/juce_core.h>

namespace oscil::state {

/**
 * @class TrackState
 * @brief Persistent state management for individual oscilloscope tracks
 *
 * Represents and manages the complete persistent state for a single track
 * in the oscilloscope including visual properties, display settings, and
 * audio processing parameters. Uses JUCE ValueTree for efficient state
 * storage, serialization, and automatic change notifications.
 *
 * The class provides type-safe accessors for all track properties while
 * maintaining backward compatibility through version migration. State
 * changes are automatically persisted and can be serialized to XML for
 * project storage or preset management.
 *
 * Key features:
 * - Type-safe property access with validation
 * - Automatic serialization/deserialization via JUCE ValueTree
 * - Version migration for backward compatibility
 * - Thread-safe property access through ValueTree
 * - XML import/export for human-readable storage
 * - Change notification support via JUCE listeners
 *
 * Track properties managed:
 * - **Track ID**: Unique identifier for referencing the track
 * - **Display Name**: User-visible name for the track
 * - **Color Index**: Index into theme color palette (0-63)
 * - **Visibility**: Whether the track is currently displayed
 * - **Gain**: Amplitude scaling factor for display
 * - **Offset**: Vertical positioning offset for multi-track layouts
 *
 * State persistence:
 * - Properties are automatically persisted in ValueTree
 * - Supports XML serialization for project files
 * - Version migration ensures compatibility with older files
 * - Default values are applied for missing properties
 *
 * Thread safety:
 * - Property access is thread-safe through ValueTree
 * - State modifications are atomic
 * - Change notifications can be received on any thread
 *
 * Example usage:
 * @code
 * // Create new track state
 * TrackState track(0);
 * track.setTrackName("Left Channel");
 * track.setColorIndex(2);
 * track.setGain(1.5f);
 *
 * // Serialize to XML
 * juce::String xml = track.toXmlString();
 *
 * // Restore from XML
 * TrackState restored = TrackState::fromXmlString(xml);
 * @endcode
 *
 * @see oscil::theme::ThemeManager for color palette management
 * @see audio::MultiTrackEngine for track processing integration
 * @note State changes trigger ValueTree change notifications
 */
class TrackState {
public:
    // Property identifiers for ValueTree - using string literals directly to avoid static initialization
    inline static const char* TRACK_ID_PROPERTY = "trackId";
    inline static const char* TRACK_NAME_PROPERTY = "trackName";
    inline static const char* COLOR_INDEX_PROPERTY = "colorIndex";
    inline static const char* IS_VISIBLE_PROPERTY = "isVisible";
    inline static const char* GAIN_PROPERTY = "gain";
    inline static const char* OFFSET_PROPERTY = "offset";

    /**
     * Creates a new TrackState with default values.
     * @param trackId Unique identifier for the track
     */
    explicit TrackState(int trackId);

    /**
     * Creates a TrackState from an existing ValueTree.
     * @param tree The ValueTree containing track state data
     */
    explicit TrackState(const juce::ValueTree& tree);

    /**
     * Creates a copy of another TrackState.
     */
    TrackState(const TrackState& other);
    TrackState& operator=(const TrackState& other);

    // Default destructor
    ~TrackState() = default;

    // === Property Accessors ===

    /** Gets the track ID (unique identifier). */
    int getTrackId() const;

    /** Sets the track ID. */
    void setTrackId(int newId);

    /** Gets the track display name. */
    juce::String getTrackName() const;

    /** Sets the track display name. */
    void setTrackName(const juce::String& newName);

    /** Gets the color index (0-63 for up to 64 distinct colors). */
    int getColorIndex() const;

    /** Sets the color index. */
    void setColorIndex(int newColorIndex);

    /** Gets the visibility state. */
    bool isVisible() const;

    /** Sets the visibility state. */
    void setVisible(bool shouldBeVisible);

    /** Gets the gain multiplier (1.0 = normal). */
    float getGain() const;

    /** Sets the gain multiplier. */
    void setGain(float newGain);

    /** Gets the vertical offset (-1.0 to 1.0). */
    float getOffset() const;

    /** Sets the vertical offset. */
    void setOffset(float newOffset);

    // === State Management ===

    /** Gets the underlying ValueTree for serialization. */
    const juce::ValueTree& getState() const { return state; }

    /** Gets a copy of the underlying ValueTree. */
    juce::ValueTree getStateCopy() const { return state.createCopy(); }

    /**
     * Replaces the current state with a new ValueTree.
     * @param newState The new state to use
     */
    void replaceState(const juce::ValueTree& newState);

    /**
     * Checks if the state contains valid data.
     * @return true if the state is valid
     */
    bool isValid() const;

    /**
     * Resets all properties to their default values.
     */
    void resetToDefaults();

    // === Serialization Helpers ===

    /**
     * Serializes the state to an XML string.
     * @return XML representation of the state
     */
    juce::String toXmlString() const;

    /**
     * Creates a TrackState from an XML string.
     * @param xmlString The XML string to parse
     * @return A new TrackState, or invalid state if parsing fails
     */
    static TrackState fromXmlString(const juce::String& xmlString);

    // === Version Information ===

    /** Current version of the TrackState format. */
    static constexpr int CURRENT_VERSION = 1;

    /** Gets the version of this state data. */
    int getVersion() const;

private:
    juce::ValueTree state;

    /**
     * Initializes the ValueTree with default values.
     * @param trackId The track ID to use
     */
    void initializeDefaults(int trackId);

    /**
     * Validates and migrates the state if necessary.
     */
    void validateAndMigrate();
};

} // namespace oscil::state
