/**
 * @file PluginEditor.h
 * @brief Main editor/UI component for the Oscil oscilloscope plugin
 *
 * This file contains the primary user interface implementation for the Oscil
 * multi-track oscilloscope plugin. The editor provides real-time waveform
 * visualization, control panels, and theme management.
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Forward declarations
class OscilAudioProcessor;
class OscilloscopeComponent;
class OpenGLManager;

/**
 * @class OscilAudioProcessorEditor
 * @brief Main editor interface for the Oscil oscilloscope plugin
 *
 * This class provides the complete user interface for the Oscil plugin,
 * including real-time waveform visualization, control panels, and theme
 * management. It inherits from JUCE's AudioProcessorEditor and implements
 * a Timer for periodic UI updates.
 *
 * Key features:
 * - Real-time oscilloscope display with up to 64 simultaneous tracks
 * - Optional OpenGL acceleration for improved performance
 * - Theme selection and visual customization
 * - Debug UI for development and troubleshooting
 * - Responsive layout that adapts to different window sizes
 *
 * The editor maintains a connection to the audio processor and coordinates
 * between the audio processing thread and the UI thread using thread-safe
 * communication mechanisms.
 *
 * Performance characteristics:
 * - Updates at 60fps with minimal CPU overhead
 * - Scales efficiently with multiple tracks
 * - Optional GPU acceleration via OpenGL
 * - Memory-efficient rendering pipeline
 *
 * @see OscilAudioProcessor
 * @see OscilloscopeComponent
 * @see OpenGLManager
 */
class OscilAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
   public:
    /**
     * @brief Constructs the plugin editor
     *
     * Initializes the complete user interface including the oscilloscope
     * component, control panels, and optional OpenGL acceleration.
     * Sets up the layout and establishes communication with the audio processor.
     *
     * @param processor Reference to the audio processor that owns this editor
     *
     * @post Editor is fully initialized and ready for display
     * @post Timer is started for periodic UI updates
     * @post Oscilloscope component is created and configured
     */
    explicit OscilAudioProcessorEditor(OscilAudioProcessor& processor);

    /**
     * @brief Destructor
     *
     * Safely cleans up all UI components, stops the update timer,
     * and releases OpenGL resources if enabled.
     *
     * @post All UI resources are properly released
     * @post OpenGL context is safely destroyed if active
     */
    ~OscilAudioProcessorEditor() override;

    //==============================================================================
    // JUCE Component Overrides

    /**
     * @brief Renders the editor's user interface
     *
     * Called by JUCE when the editor needs to redraw itself.
     * Handles the main UI rendering and coordinates with child components.
     *
     * @param g Graphics context for drawing operations
     *
     * @note This method should be lightweight as it's called frequently
     * @note Actual waveform rendering is delegated to OscilloscopeComponent
     */
    void paint(juce::Graphics& g) override;

    /**
     * @brief Handles editor resize events
     *
     * Repositions and resizes all child components when the editor
     * window size changes. Implements responsive layout that adapts
     * to different window sizes and aspect ratios.
     *
     * @post All child components are properly positioned and sized
     * @post Layout is optimized for current window dimensions
     */
    void resized() override;

    //==============================================================================
    // OpenGL Management

    /**
     * @brief Enables OpenGL acceleration for improved performance
     *
     * Activates the OpenGL rendering context to provide GPU-accelerated
     * waveform rendering and visual effects. Gracefully handles cases
     * where OpenGL is unavailable.
     *
     * @post OpenGL context is active if hardware supports it
     * @post Falls back to CPU rendering if OpenGL initialization fails
     *
     * @note OpenGL provides better performance with multiple tracks
     * @see disableOpenGL()
     */
    void enableOpenGL();

    /**
     * @brief Disables OpenGL acceleration
     *
     * Deactivates the OpenGL rendering context and switches back to
     * CPU-based rendering. Useful for troubleshooting or compatibility.
     *
     * @post OpenGL context is safely destroyed
     * @post Rendering switches to CPU-based JUCE Graphics
     *
     * @see enableOpenGL()
     */
    void disableOpenGL();

    /**
     * @brief Checks if OpenGL acceleration is currently enabled
     *
     * @return true if OpenGL context is active and functional
     * @retval false if using CPU-based rendering
     *
     * @note Return value reflects actual OpenGL state, not just the request
     */
    [[nodiscard]] bool isOpenGLEnabled() const;

   private:
    //==============================================================================
    // Timer Implementation

    /**
     * @brief Timer callback for periodic UI updates
     *
     * Called at regular intervals to update the UI with fresh audio data
     * from the processor. Maintains 60fps update rate while minimizing
     * CPU usage through efficient data transfer.
     *
     * @note Runs on the message thread, safe for UI operations
     * @note Frequency is automatically adjusted based on system performance
     */
    void timerCallback() override;

    /**
     * @brief Sets up debug UI components
     *
     * Initializes theme selection controls and other debug/development
     * interface elements. Only active in debug builds or when explicitly
     * enabled for testing.
     *
     * @post Debug UI components are created and positioned
     * @post Theme selector is populated with available themes
     */
    void setupDebugUI();

    //==============================================================================
    // Member Variables

    /**
     * @brief Reference to the audio processor
     *
     * Maintains connection to the audio processing engine for data access
     * and state management. Used to retrieve waveform data and configuration.
     */
    OscilAudioProcessor& ap;

    /**
     * @brief Main oscilloscope visualization component
     *
     * Handles all real-time waveform rendering, multi-track display,
     * and user interaction with the oscilloscope view. This is the
     * primary visual component of the plugin.
     *
     * @see OscilloscopeComponent
     */
    std::unique_ptr<OscilloscopeComponent> oscilloscope;

    /**
     * @brief OpenGL context management
     *
     * Manages the OpenGL rendering context for GPU-accelerated
     * visualization. Handles context creation, destruction, and
     * integration with JUCE's rendering system.
     *
     * @note nullptr when OpenGL is disabled
     * @see OpenGLManager
     */
    std::unique_ptr<OpenGLManager> openGLManager;

    //==============================================================================
    // Debug UI Components

    /**
     * @brief Height reserved for debug UI elements
     *
     * Constant defining the vertical space allocated for debug controls
     * such as theme selection and performance monitoring.
     */
    static constexpr int DEBUG_UI_HEIGHT = 30;

    /**
     * @brief Theme selection dropdown for debug purposes
     *
     * Allows developers and testers to quickly switch between different
     * visual themes to test appearance and accessibility features.
     */
    juce::ComboBox themeSelector;

    /**
     * @brief Label for the theme selector
     *
     * Provides descriptive text for the theme selection control
     * in the debug interface.
     */
    juce::Label themeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilAudioProcessorEditor)
};
