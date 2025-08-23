/**
 * @file OscilloscopeComponent.h
 * @brief Main oscilloscope visualization component for real-time waveform display
 *
 * This file defines the OscilloscopeComponent class which provides the primary
 * visual interface for the Oscil plugin. It renders real-time audio waveforms
 * with performance optimizations including level-of-detail processing, GPU
 * acceleration support, and zero-allocation rendering paths.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/WaveformDataBridge.h"
#include "../util/PerformanceMonitor.h"
#include "../ui/layout/LayoutManager.h"
#include "DecimationProcessor.h"
#include "AdvancedDecimationProcessor.h"

// Forward declarations
class OscilAudioProcessor;
class OpenGLManager;
class GpuRenderHook;

namespace oscil::theme {
    class ThemeManager;
}

/**
 * @class OscilloscopeComponent
 * @brief Main oscilloscope visualization component for real-time waveform display
 *
 * The OscilloscopeComponent provides the primary visual interface for the Oscil
 * oscilloscope plugin. It renders real-time audio waveforms from multiple channels
 * with high performance and visual quality.
 *
 * Key Features:
 * - Real-time waveform visualization with minimal latency
 * - Multi-channel support with distinct color coding
 * - Level-of-detail (LOD) optimization for performance
 * - Optional GPU acceleration through OpenGL
 * - Zero-allocation rendering path for real-time performance
 * - Integrated performance monitoring and statistics
 * - Themeable color schemes and styling
 * - Adaptive quality based on available processing time
 *
 * Performance Characteristics:
 * - Renders at 60 FPS with <1ms frame time typical
 * - Supports up to 8 simultaneous audio channels
 * - Automatic decimation for high sample rates
 * - Memory-efficient caching strategies
 * - Lock-free data access from audio thread
 *
 * Architecture:
 * The component integrates with several key systems:
 * - WaveformDataBridge: Provides lock-free audio data access
 * - DecimationProcessor: Handles LOD optimization
 * - OpenGLManager: Optional GPU acceleration
 * - ThemeManager: Color scheme and styling
 * - PerformanceMonitor: Real-time performance tracking
 *
 * Rendering Pipeline:
 * 1. Acquire latest audio data snapshot (lock-free)
 * 2. Apply decimation based on zoom level and performance
 * 3. Update cached paths if geometry changed
 * 4. Render waveforms with theme-appropriate colors
 * 5. Record performance metrics
 *
 * Thread Safety:
 * - All rendering operations are performed on the message thread
 * - Audio data access is lock-free through atomic operations
 * - Component state is not shared between threads
 *
 * Example Usage:
 * @code
 * // Basic setup
 * OscilloscopeComponent oscilloscope(processor);
 * oscilloscope.setOpenGLManager(&glManager);
 * oscilloscope.setThemeManager(&themeManager);
 *
 * // Performance monitoring
 * auto stats = oscilloscope.getPerformanceStats();
 * DBG("Avg frame time: " << stats.averageMs << "ms");
 * @endcode
 *
 * @see WaveformDataBridge for audio data access
 * @see DecimationProcessor for LOD optimization
 * @see OpenGLManager for GPU acceleration
 * @see ThemeManager for styling
 * @see PerformanceMonitor for performance tracking
 */
class OscilloscopeComponent : public juce::Component {
public:
    /**
     * @brief Constructs the oscilloscope component with processor reference
     *
     * Initializes the component with a reference to the main audio processor.
     * Sets up internal state, allocates buffers, and prepares for rendering.
     *
     * @param processor Reference to the main audio processor that provides
     *                 audio data and plugin state
     *
     * @post Component is ready for display and audio data rendering
     * @post Performance monitoring is initialized and active
     * @post Decimation processor is configured with default settings
     *
     * @note This constructor does not throw exceptions
     * @note Component must be added to a parent before rendering begins
     */
    explicit OscilloscopeComponent(OscilAudioProcessor& processor);

    /**
     * @brief Renders the oscilloscope waveform display
     *
     * Core rendering method called by JUCE framework to draw the component.
     * Implements a high-performance rendering pipeline with zero-allocation
     * paths and adaptive quality based on available processing time.
     *
     * Rendering Pipeline:
     * 1. Acquire latest audio data snapshot (lock-free)
     * 2. Update cached bounds if component size changed
     * 3. Apply decimation based on zoom level and performance budget
     * 4. Render background grid and UI elements
     * 5. Draw waveforms for each active channel
     * 6. Record performance metrics for adaptive quality
     *
     * @param graphics The JUCE Graphics context for drawing operations
     *
     * @pre Component bounds must be valid (width > 0, height > 0)
     * @post Frame performance statistics are updated
     * @post Visual display reflects latest audio data
     *
     * @note This method is called on the message thread only
     * @note Average execution time: <1ms for typical configurations
     * @note Memory allocations are avoided in the rendering path
     */
    void paint(juce::Graphics& graphics) override;

    /**
     * @brief Handles component size changes and layout updates
     *
     * Called by JUCE framework when component bounds change. Updates
     * internal layout calculations, cached bounds, and prepares rendering
     * buffers for the new dimensions.
     *
     * @post Cached bounds are invalidated and will be recalculated
     * @post Pre-allocated rendering paths are resized as needed
     * @post Component is ready for rendering at new dimensions
     *
     * @note This method is called on the message thread only
     * @note Triggers recalculation of channel layout and spacing
     */
    void resized() override;

    /**
     * @brief Sets the OpenGL manager for GPU render hook integration
     *
     * Configures optional GPU acceleration through OpenGL rendering.
     * When enabled, certain rendering operations can be offloaded to
     * the GPU for improved performance with complex waveforms.
     *
     * @param manager Pointer to OpenGL manager instance, or nullptr to
     *               disable GPU acceleration
     *
     * @post GPU rendering is enabled/disabled based on manager validity
     * @post Component will use CPU or GPU rendering path appropriately
     *
     * @note GPU acceleration is optional and gracefully degrades
     * @note Can be called at any time to change rendering mode
     */
    void setOpenGLManager(OpenGLManager* manager);

    /**
     * @brief Sets the theme manager for color theming and styling
     *
     * Configures the component to use a specific theme manager for
     * color schemes, styling, and visual appearance. Enables dynamic
     * theme switching and consistent visual styling across the plugin.
     *
     * @param manager Pointer to theme manager instance, or nullptr to
     *               use default colors
     *
     * @post Component will use theme colors for waveforms and UI
     * @post Visual appearance reflects current theme settings
     * @post Color changes are applied immediately
     *
     * @note Theme changes take effect on next paint() call
     * @note Can be called at any time to change themes
     */
    void setThemeManager(oscil::theme::ThemeManager* manager);

    /**
     * @brief Sets per-track visibility state
     *
     * Controls whether individual tracks are rendered. Allows selective
     * display of tracks without affecting audio processing. Changes take
     * effect immediately on next paint() call.
     *
     * @param trackIndex Zero-based track index (0-63)
     * @param visible Whether the track should be visible
     *
     * @note Track indices beyond actual channel count are ignored
     * @note Default visibility is true for all tracks
     */
    void setTrackVisible(int trackIndex, bool visible);

    /**
     * @brief Gets per-track visibility state
     *
     * Returns whether a specific track is currently set to visible.
     * Tracks beyond the current channel count return false.
     *
     * @param trackIndex Zero-based track index (0-63)
     * @return true if track is visible, false otherwise
     */
    bool isTrackVisible(int trackIndex) const;

    /**
     * @brief Sets visibility for all tracks
     *
     * Batch operation to show or hide all tracks simultaneously.
     * More efficient than calling setTrackVisible() for each track.
     *
     * @param visible Whether all tracks should be visible
     */
    void setAllTracksVisible(bool visible);

    /**
     * @brief Gets the number of visible tracks
     *
     * Returns count of tracks currently set to visible, regardless
     * of whether they have active audio data.
     *
     * @return Number of visible tracks (0-64)
     */
    int getNumVisibleTracks() const;

    //==============================================================================
    // Layout Management

    /**
     * @brief Sets the layout manager for multi-layout rendering
     *
     * Configures the component to use a specific layout manager for
     * organizing tracks across different layout modes (overlay, split, grid).
     *
     * @param manager Pointer to layout manager instance, or nullptr to
     *               use single overlay mode
     *
     * @post Component will render tracks according to layout configuration
     * @post Layout changes take effect on next paint() call
     */
    void setLayoutManager(oscil::ui::layout::LayoutManager* manager);

    /**
     * @brief Gets the current layout manager
     *
     * @return Pointer to layout manager, or nullptr if not set
     */
    oscil::ui::layout::LayoutManager* getLayoutManager() const { return layoutManager; }

    /**
     * @brief Assigns a track to a specific layout region
     *
     * Routes a track to render in the specified layout region. Only
     * effective when a layout manager is set and in non-overlay mode.
     *
     * @param trackIndex Zero-based track index (0-63)
     * @param regionIndex Zero-based region index
     * @return true if assignment was successful
     */
    bool assignTrackToRegion(int trackIndex, int regionIndex);

    /**
     * @brief Auto-distributes tracks across layout regions
     *
     * Automatically distributes visible tracks evenly across all
     * available regions in the current layout mode.
     *
     * @param numTracks Number of tracks to distribute
     */
    void autoDistributeTracks(int numTracks);

    /**
     * @brief Retrieves current performance statistics
     *
     * Returns comprehensive performance metrics for the rendering
     * pipeline including frame times, frame rate, and statistical
     * analysis of rendering performance.
     *
     * @return FrameStats structure containing:
     *         - Average frame time in milliseconds
     *         - Minimum and maximum frame times
     *         - Current frame rate (FPS)
     *         - Standard deviation of frame times
     *         - Total number of frames rendered
     *
     * @note Statistics are calculated from recent rendering history
     * @note Thread-safe operation using atomic operations
     * @note Useful for performance monitoring and optimization
     */
    oscil::util::PerformanceMonitor::FrameStats getPerformanceStats() const;

private:
    //==============================================================================
    // Core Dependencies

    /**
     * @brief Reference to the main audio processor
     *
     * Provides access to audio data, plugin state, and processing parameters.
     * Used to acquire waveform data and coordinate with the audio pipeline.
     */
    OscilAudioProcessor& processor;

    /**
     * @brief Optional OpenGL manager for GPU acceleration
     *
     * When not nullptr, enables GPU-accelerated rendering for improved
     * performance with complex waveforms. Can be changed at runtime.
     */
    OpenGLManager* openGLManager = nullptr;

    /**
     * @brief Optional theme manager for color schemes
     *
     * Provides color schemes and styling information. When nullptr,
     * component uses default colors. Can be changed at runtime.
     */
    oscil::theme::ThemeManager* themeManager = nullptr;

    /**
     * @brief Optional layout manager for multi-layout rendering
     *
     * Handles layout modes and track assignment to regions. When nullptr,
     * component uses default overlay layout. Can be changed at runtime.
     */
    oscil::ui::layout::LayoutManager* layoutManager = nullptr;

    //==============================================================================
    // Performance and Processing

    /**
     * @brief Real-time performance monitoring system
     *
     * Tracks rendering performance including frame times, frame rate,
     * and statistical analysis. Used for adaptive quality and debugging.
     */
    oscil::util::PerformanceMonitor performanceMonitor;

    /**
     * @brief Level-of-detail processor for waveform optimization
     *
     * Handles decimation and LOD processing to maintain performance
     * with high sample rates and complex waveforms. Automatically
     * adjusts quality based on available processing time.
     */
    oscil::render::DecimationProcessor decimationProcessor;

    /**
     * @brief Advanced decimation processor for 64-track optimization
     *
     * High-performance multi-level decimation processor designed specifically
     * for 64-track scenarios. Provides adaptive quality control, memory pooling,
     * and OpenGL acceleration recommendations to maintain target frame rates
     * under extreme load conditions.
     */
    oscil::render::AdvancedDecimationProcessor advancedDecimationProcessor;

    //==============================================================================
    // Rendering State and Caches

    /**
     * @brief Frame counter for time-based effects and debugging
     *
     * Incremented each frame to support time-based visual effects,
     * animations, and debugging. Provides temporal context for rendering.
     */
    uint64_t frameCounter = 0;

    /**
     * @brief Thread-safe snapshot of current audio data
     *
     * Contains the latest audio waveform data acquired from the audio
     * thread through lock-free mechanisms. Updated each frame with
     * fresh audio data.
     */
    audio::AudioDataSnapshot currentSnapshot;

    /**
     * @brief Flag indicating new audio data is available
     *
     * Set when fresh audio data has been acquired and needs to be
     * processed. Used to avoid unnecessary rendering when data
     * hasn't changed.
     */
    bool hasNewData = false;

    /**
     * @brief Pre-allocated path objects to avoid runtime allocations
     *
     * Vector of JUCE Path objects pre-allocated to avoid memory
     * allocations during rendering. Sized to accommodate maximum
     * expected number of channels and waveform complexity.
     */
    std::vector<juce::Path> cachedPaths;

    /**
     * @brief Per-track visibility state for multi-track rendering
     *
     * Boolean array indicating which tracks should be rendered.
     * Size matches MAX_CHANNELS from AudioDataSnapshot for consistency.
     * Default state is all tracks visible.
     */
    std::array<bool, audio::AudioDataSnapshot::MAX_CHANNELS> trackVisibility;

    /**
     * @brief Cached layout calculations for performance optimization
     *
     * Stores pre-calculated layout information to avoid recalculating
     * channel positions and dimensions every frame. Invalidated when
     * component size or channel count changes.
     */
    struct CachedBounds {
        juce::Rectangle<float> bounds;        ///< Overall component bounds
        float channelHeight = 0.0f;          ///< Height of each channel display
        float channelSpacing = 0.0f;         ///< Vertical spacing between channels
        int lastChannelCount = 0;            ///< Channel count when cache was built
        bool isValid = false;                ///< Whether cached data is current
    } cachedBounds;

    //==============================================================================
    // Private Methods

    /**
     * @brief Gets appropriate waveform color for a specific channel
     *
     * Retrieves the color to use for rendering a specific audio channel's
     * waveform. Uses theme manager if available, otherwise falls back to
     * default color scheme. Ensures each channel has a distinct color.
     *
     * @param channelIndex Zero-based index of the audio channel
     * @return JUCE Colour object for rendering the channel's waveform
     *
     * @note Colors cycle through available palette for channel counts > 8
     * @note Thread-safe operation
     */
    juce::Colour getChannelColor(int channelIndex) const;

    /**
     * @brief Updates cached bounds if component layout has changed
     *
     * Recalculates channel layout, spacing, and dimensions when the
     * component size or channel count changes. Invalidates cache when
     * necessary and rebuilds layout calculations.
     *
     * @param channelCount Current number of active audio channels
     *
     * @post cachedBounds contains current layout information
     * @post isValid flag reflects whether cache is current
     *
     * @note Only recalculates when dimensions or channel count change
     */
    void updateCachedBounds(int channelCount);

    /**
     * @brief Renders a single audio channel with performance optimizations
     *
     * Handles rendering of one audio channel's waveform with optimizations
     * including decimation, path caching, and adaptive quality. Core
     * rendering routine called for each active channel.
     *
     * @param graphics JUCE Graphics context for drawing operations
     * @param channelIndex Zero-based index of the channel being rendered
     * @param samples Pointer to audio sample data for the channel
     * @param sampleCount Number of samples in the audio data
     *
     * @pre samples points to valid audio data
     * @pre sampleCount > 0
     * @pre channelIndex is within valid range
     * @post Channel waveform is rendered to graphics context
     *
     * @note Uses cached paths to avoid path generation overhead
     * @note Applies decimation for performance with large sample counts
     */
    void renderChannel(juce::Graphics& graphics, int channelIndex,
                      const float* samples, size_t sampleCount);

    /**
     * @brief Renders tracks using overlay layout (all tracks in same region)
     *
     * Renders all visible tracks overlaid in the same region. This is the
     * default behavior and highest performance mode.
     *
     * @param graphics JUCE Graphics context for drawing operations
     * @param numChannels Number of audio channels to render
     */
    void renderOverlayLayout(juce::Graphics& graphics, int numChannels);

    /**
     * @brief Renders tracks using multi-region layout (tracks in separate regions)
     *
     * Renders tracks distributed across multiple layout regions according to
     * the current layout configuration. Supports split and grid modes.
     *
     * @param graphics JUCE Graphics context for drawing operations
     * @param numChannels Number of audio channels to render
     */
    void renderMultiRegionLayout(juce::Graphics& graphics, int numChannels);

    /**
     * @brief Renders a single region within a multi-region layout
     *
     * Renders all tracks assigned to a specific layout region with proper
     * clipping and coordinate transformation.
     *
     * @param graphics JUCE Graphics context for drawing operations
     * @param region Layout region configuration and bounds
     * @param regionIndex Zero-based region index for debugging
     */
    void renderLayoutRegion(juce::Graphics& graphics,
                           const oscil::ui::layout::LayoutRegion& region,
                           int regionIndex);

    /**
     * @brief Renders a single channel within a specific layout region
     *
     * Renders one audio channel's waveform within the bounds and coordinate
     * system of a specific layout region. Handles region-local positioning.
     *
     * @param graphics JUCE Graphics context for drawing operations
     * @param channelIndex Zero-based global channel index
     * @param regionChannelIndex Zero-based index within the region
     * @param samples Pointer to audio sample data
     * @param sampleCount Number of samples in the data
     */
    void renderChannelInRegion(juce::Graphics& graphics, int channelIndex, int regionChannelIndex,
                              const float* samples, size_t sampleCount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};
