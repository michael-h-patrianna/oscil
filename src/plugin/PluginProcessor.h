/**
 * @file PluginProcessor.h
 * @brief Core audio processor for the Oscil multi-track oscilloscope plugin
 *
 * This file contains the main AudioProcessor-based class that handles audio I/O,
 * multi-track management, and integration with the host DAW. The implementation
 * has been modernized to remove backward compatibility features and focus on
 * the unified MultiTrackEngine architecture.
 *
 * Key Features:
 * - Multi-track audio capture and processing (up to 64 tracks)
 * - Lock-free communication with UI thread via WaveformDataBridge
 * - Theme management and state persistence
 * - Cross-DAW compatibility (VST3, AU, AAX)
 * - Real-time performance optimized for minimal CPU usage
 * - OpenGL acceleration support (optional)
 *
 * Architecture Notes:
 * - Legacy ring buffer access API has been removed (as of v0.2)
 * - All audio data access now goes through MultiTrackEngine
 * - Single optimized processing path eliminates dual compatibility modes
 * - Thread-safe design with lock-free audio thread operations
 *
 * Performance Characteristics:
 * - Audio thread: <1% CPU usage for single track processing
 * - Memory usage: Linear scaling with track count (<10MB per track)
 * - Latency: ≤10ms visual update from audio capture to UI display
 * - Thread safety: Complete lock-free audio processing with UI bridge
 *
 * Migration from Legacy API:
 * @code
 * // OLD (removed in v0.2):
 * auto& ringBuffer = processor.getRingBuffer(channelIndex);
 * int numBuffers = processor.getNumRingBuffers();
 *
 * // NEW (current):
 * auto& bridge = processor.getMultiTrackEngine().getWaveformDataBridge();
 * audio::AudioDataSnapshot snapshot;
 * if (bridge.pullLatestData(snapshot)) {
 *     // Use snapshot.samples[channel][sample] for audio data
 * }
 * @endcode
 *
 * @author Oscil Development Team
 * @version 0.2
 * @date 2024
 *
 * @see audio::MultiTrackEngine
 * @see audio::WaveformDataBridge
 * @see oscil::theme::ThemeManager
 * @see oscil::state::TrackState
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../audio/WaveformDataBridge.h"
#include "../audio/MultiTrackEngine.h"
#include "../audio/MeasurementData.h"
#include "../audio/SignalProcessor.h"
#include "../state/TrackState.h"
#include "../theme/ThemeManager.h"
#include "../timing/TimingEngine.h"

/**
 * @class OscilAudioProcessor
 * @brief Main audio processor for the Oscil multi-track oscilloscope plugin
 *
 * This class serves as the primary entry point for audio processing in the Oscil plugin.
 * It inherits from JUCE's AudioProcessor and provides the following key functionality:
 *
 * - Real-time audio capture and buffering for up to 64 simultaneous tracks
 * - Thread-safe communication between audio and UI threads
 * - State management and persistence for plugin settings
 * - Theme management for visual customization
 * - Cross-DAW compatibility for VST3/AU/AAX formats
 *
 * The processor maintains separate engines for different aspects:
 * - MultiTrackEngine: Handles multiple audio track capture and management
 * - WaveformDataBridge: Provides lock-free audio-to-UI data transfer
 * - ThemeManager: Manages visual themes and color schemes
 * - TrackState: Manages individual track configuration and settings
 *
 * Performance characteristics:
 * - Supports up to 64 simultaneous audio tracks
 * - Real-time safe audio processing with no blocking operations
 * - Memory efficient with linear scaling per track (<10MB per track)
 * - Low latency operation (≤10ms processing delay)
 *
 * @see audio::MultiTrackEngine
 * @see audio::WaveformDataBridge
 * @see oscil::theme::ThemeManager
 * @see oscil::state::TrackState
 */
class OscilAudioProcessor : public juce::AudioProcessor {
   public:
    /**
     * @brief Constructs the Oscil audio processor
     *
     * Initializes all audio processing components including:
     * - Multi-track engine for up to 64 simultaneous tracks
     * - Ring buffers for audio data storage
     * - Waveform data bridge for thread-safe UI communication
     * - Theme manager for visual customization
     * - Track state management system
     */
    OscilAudioProcessor();

    /**
     * @brief Destructor
     *
     * Safely releases all audio resources and cleans up processing threads.
     * Ensures proper shutdown of the multi-track engine and data bridges.
     */
    ~OscilAudioProcessor() override;

    //==============================================================================
    // Audio Processing Methods

    /**
     * @brief Prepares the processor for audio playback
     *
     * Called by the host before audio processing begins. Initializes all audio
     * processing components with the specified parameters.
     *
     * @param sampleRate The sample rate that will be used for audio processing
     * @param samplesPerBlock The maximum number of samples that will be processed in each block
     *
     * @pre sampleRate > 0 and samplesPerBlock > 0
     * @post All audio components are initialized and ready for processing
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    /**
     * @brief Releases all audio processing resources
     *
     * Called by the host when audio processing stops. Safely cleans up all
     * audio buffers, threads, and processing components.
     *
     * @post All audio resources are released and processor is in idle state
     */
    void releaseResources() override;

    /**
     * @brief Determines if the given bus layout is supported
     *
     * Validates whether the plugin can work with the proposed input/output
     * configuration. The Oscil plugin supports multiple input channels for
     * multi-track visualization.
     *
     * @param layouts The proposed bus layout configuration
     * @return true if the layout is supported, false otherwise
     *
     * @note Supports up to 64 input channels for multi-track operation
     */
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    /**
     * @brief Processes a block of audio samples
     *
     * This is the main audio processing callback where incoming audio data
     * is captured and stored for oscilloscope visualization. Operates in
     * real-time with strict performance requirements.
     *
     * @param buffer Audio buffer containing input samples to process
     * @param midiMessages MIDI messages (unused in Oscil)
     *
     * @pre prepareToPlay() has been called
     * @post Audio samples are captured in ring buffers for visualization
     *
     * @note This method must be real-time safe with no blocking operations
     * @note Supports up to 64 input channels simultaneously
     */
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    // Editor and Plugin Information

    /**
     * @brief Creates the plugin's graphical user interface
     *
     * Instantiates and returns the main editor component for the Oscil plugin.
     * The editor provides real-time oscilloscope visualization and control interface.
     *
     * @return Pointer to the newly created editor component
     * @retval nullptr if editor creation fails
     *
     * @note The returned pointer is managed by the host
     */
    juce::AudioProcessorEditor* createEditor() override;

    /**
     * @brief Indicates whether this plugin has a graphical editor
     *
     * @return true (Oscil always provides a graphical interface)
     */
    bool hasEditor() const override {
        return true;
    }

    //==============================================================================
    // Plugin Properties

    /**
     * @brief Returns the name of this plugin
     *
     * @return "Oscil" - the plugin name displayed to users
     */
    const juce::String getName() const override {
        return "Oscil";
    }

    /**
     * @brief Indicates whether this plugin accepts MIDI input
     *
     * @return false (Oscil is an audio analysis tool that doesn't process MIDI)
     */
    bool acceptsMidi() const override {
        return false;
    }

    /**
     * @brief Indicates whether this plugin produces MIDI output
     *
     * @return false (Oscil only analyzes audio, doesn't generate MIDI)
     */
    bool producesMidi() const override {
        return false;
    }

    /**
     * @brief Indicates whether this plugin is a MIDI effect
     *
     * @return false (Oscil processes audio, not MIDI)
     */
    bool isMidiEffect() const override {
        return false;
    }

    /**
     * @brief Returns the tail length in seconds
     *
     * The tail length indicates how long the plugin continues to produce
     * meaningful output after input stops.
     *
     * @return 0.0 (Oscil has no tail - output stops immediately when input stops)
     */
    double getTailLengthSeconds() const override {
        return 0.0;
    }

    //==============================================================================
    // Program/Preset Management (Not used in Oscil)

    /**
     * @brief Returns the number of presets/programs available
     *
     * @return 1 (Oscil uses state-based configuration rather than traditional presets)
     */
    int getNumPrograms() override {
        return 1;
    }

    /**
     * @brief Returns the currently selected program index
     *
     * @return 0 (single program model)
     */
    int getCurrentProgram() override {
        return 0;
    }

    /**
     * @brief Sets the current program (no-op in Oscil)
     *
     * @param index Program index (ignored)
     *
     * @note Oscil uses ValueTree-based state management instead of programs
     */
    void setCurrentProgram(int) override {}

    /**
     * @brief Returns the name of a program
     *
     * @param index Program index (ignored)
     * @return Empty string (programs not used in Oscil)
     */
    const juce::String getProgramName(int) override {
        return {};
    }

    /**
     * @brief Changes the name of a program (no-op in Oscil)
     *
     * @param index Program index (ignored)
     * @param newName New program name (ignored)
     */
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    // State Persistence

    /**
     * @brief Saves the current plugin state to memory
     *
     * Serializes all plugin settings, track configurations, and theme preferences
     * into a binary block for host storage. This enables state restoration
     * when the project is reloaded.
     *
     * @param destData Memory block to store the serialized state
     *
     * @note Called by the host during project save operations
     * @see setStateInformation()
     */
    void getStateInformation(juce::MemoryBlock& destData) override;

    /**
     * @brief Restores plugin state from memory
     *
     * Deserializes plugin state from a previously saved binary block and
     * applies all settings to restore the exact plugin configuration.
     *
     * @param data Pointer to serialized state data
     * @param sizeInBytes Size of the state data in bytes
     *
     * @pre data points to valid serialized state data
     * @post Plugin state is fully restored to saved configuration
     *
     * @note Called by the host during project load operations
     * @see getStateInformation()
     */
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Legacy Ring Buffer API removed - use getMultiTrackEngine().getWaveformDataBridge() instead

    //==============================================================================
    // Modern Data Access Interface

    /**
     * @brief Gets the thread-safe waveform data bridge
     *
     * Provides access to the modern lock-free communication system between
     * the audio processing thread and UI thread. Recommended for all new code.
     *
     * @return Reference to the waveform data bridge
     *
     * @note Thread-safe for concurrent access from audio and UI threads
     * @see audio::WaveformDataBridge
     */
    audio::WaveformDataBridge& getWaveformDataBridge() {
        return multiTrackEngine.getWaveformDataBridge();
    }

    // === Multi-Track Engine Access ===

    /**
     * @brief Gets the multi-track engine for advanced track management
     *
     * Provides access to the core multi-track audio processing engine that
     * handles up to 64 simultaneous audio track capture and management.
     *
     * @return Reference to the multi-track engine
     *
     * @note Thread-safe for configuration changes from UI thread
     * @see audio::MultiTrackEngine
     */
    audio::MultiTrackEngine& getMultiTrackEngine() { return multiTrackEngine; }

    /**
     * @brief Gets read-only access to the multi-track engine
     *
     * @return Const reference to the multi-track engine
     */
    const audio::MultiTrackEngine& getMultiTrackEngine() const { return multiTrackEngine; }

    /**
     * @brief Gets reference to the measurement data bridge
     *
     * Provides access to measurement data (correlation, stereo width, levels)
     * for UI components. Use this to retrieve measurement data calculated
     * during audio processing.
     *
     * @return Reference to measurement data bridge
     * @see audio::MeasurementDataBridge
     */
    audio::MeasurementDataBridge& getMeasurementDataBridge() { return measurementDataBridge; }

    /**
     * @brief Gets const reference to the measurement data bridge
     * @see getMeasurementDataBridge()
     */
    const audio::MeasurementDataBridge& getMeasurementDataBridge() const { return measurementDataBridge; }

    // === Theme Management ===

    /**
     * @brief Gets the theme manager for visual customization
     *
     * Provides access to the theme management system that controls colors,
     * visual styling, and accessibility features throughout the plugin UI.
     *
     * @return Reference to the theme manager
     *
     * @see oscil::theme::ThemeManager
     */
    oscil::theme::ThemeManager& getThemeManager() { return themeManager; }

    /**
     * @brief Gets read-only access to the theme manager
     *
     * @return Const reference to the theme manager
     */
    const oscil::theme::ThemeManager& getThemeManager() const { return themeManager; }

    // === Timing Engine Access ===

    /**
     * @brief Gets the timing engine for timing and synchronization control
     *
     * Provides access to the timing engine that controls capture timing across
     * multiple synchronization modes including DAW sync, trigger detection, and
     * musical timing.
     *
     * @return Reference to the timing engine
     *
     * @see oscil::timing::TimingEngine
     */
    oscil::timing::TimingEngine& getTimingEngine() { return timingEngine; }

    /**
     * @brief Gets read-only access to the timing engine
     *
     * @return Const reference to the timing engine
     */
    const oscil::timing::TimingEngine& getTimingEngine() const { return timingEngine; }

    // === Track State Management ===

    /**
     * @brief Gets the track state for the primary track
     *
     * Provides access to track-specific configuration including visibility,
     * color settings, gain, offset, and other display parameters.
     *
     * @return Reference to the track state
     *
     * @note For single-track implementation, track 0 is used as default
     * @see oscil::state::TrackState
     */
    oscil::state::TrackState& getTrackState() { return trackState; }

    /**
     * @brief Gets read-only access to the track state
     *
     * @return Const reference to the track state
     */
    const oscil::state::TrackState& getTrackState() const { return trackState; }

    /**
     * @brief Applies track state changes to the current configuration
     *
     * Updates the processor's internal state to reflect any changes made
     * to the track state. Should be called after modifying track settings
     * to ensure UI consistency.
     *
     * @post All track state changes are applied to internal configuration
     * @note Thread-safe for UI thread access
     */
    void applyTrackStateChanges();

   private:
    //==============================================================================
    // Private Member Variables

    /**
     * @brief Legacy ring buffers removed - use MultiTrackEngine instead
     *
     * Legacy ring buffer system has been removed. All audio data access
     * now goes through the MultiTrackEngine's WaveformDataBridge.
     *
     * Migration guide:
     * - Replace getRingBuffer(ch) with getMultiTrackEngine().getWaveformDataBridge()
     * - Use WaveformDataBridge for thread-safe audio data access
     */

    /**
     * @brief Multi-track audio capture and processing engine
     *
     * Core component that handles up to 64 simultaneous audio tracks,
     * provides thread-safe track management, and coordinates with the UI
     * through lock-free data structures.
     *
     * @see audio::MultiTrackEngine
     */
    audio::MultiTrackEngine multiTrackEngine;

    /**
     * @brief Track state management for the primary track
     *
     * Manages configuration, display settings, and preferences for
     * the main audio track including visibility, colors, gain, and offset.
     *
     * @see oscil::state::TrackState
     */
    /**
     * @brief Track state management for the primary track
     *
     * Manages configuration, display settings, and preferences for
     * the main audio track including visibility, colors, gain, and offset.
     *
     * @see oscil::state::TrackState
     */
    oscil::state::TrackState trackState;

    /**
     * @brief Theme management system
     *
     * Handles visual themes, color schemes, and accessibility settings
     * for the plugin user interface. Supports built-in themes and
     * custom theme creation/import.
     *
     * @see oscil::theme::ThemeManager
     */
    oscil::theme::ThemeManager themeManager;

    /**
     * @brief Timing and synchronization engine
     *
     * Provides precise timing control for audio capture with multiple
     * synchronization modes including DAW sync, trigger detection,
     * and musical timing.
     *
     * @see oscil::timing::TimingEngine
     */
    oscil::timing::TimingEngine timingEngine;

    /**
     * @brief Signal processor for measurement calculations
     *
     * Performs correlation analysis, stereo width calculation, and other
     * signal processing measurements used by the measurement display system.
     *
     * @see audio::SignalProcessor
     */
    audio::SignalProcessor signalProcessor;

    /**
     * @brief Measurement data bridge for UI communication
     *
     * Provides lock-free transfer of measurement data (correlation,
     * stereo width, peak levels) from audio processing to UI components.
     *
     * @see audio::MeasurementDataBridge
     */
    audio::MeasurementDataBridge measurementDataBridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilAudioProcessor)
};
