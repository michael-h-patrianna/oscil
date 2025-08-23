/**
 * @file AdvancedDecimationProcessor.h
 * @brief Multi-level decimation processor optimized for 64-track performance
 *
 * This file contains the AdvancedDecimationProcessor class that implements
 * progressive min/max pyramid decimation algorithms to achieve stable performance
 * with up to 64 simultaneous tracks. Uses memory pooling and adaptive quality
 * strategies to maintain target frame rates under high load.
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <atomic>
#include <array>
#include <cstddef>
#include <algorithm>
#include <chrono>
#include <mutex>

namespace oscil::render {

/**
 * @class AdvancedDecimationProcessor
 * @brief High-performance multi-level decimation for 64-track optimization
 *
 * Implements progressive min/max pyramid decimation with adaptive quality
 * control specifically designed to maintain stable performance with up to
 * 64 simultaneous audio tracks. Uses memory pooling and multiple LOD levels
 * to achieve target frame rates under extreme load conditions.
 *
 * Key Features:
 * - Multi-level min/max pyramid decimation
 * - Adaptive quality mode based on load
 * - Memory pooling for zero-allocation processing
 * - Track load balancing across frames
 * - Performance-guided quality adjustment
 * - OpenGL optimization recommendations
 *
 * Performance Characteristics:
 * - Target: 30fps minimum, 60fps preferred with 64 tracks
 * - Memory: <640MB total with full track load
 * - CPU: <16% usage on reference hardware
 * - Latency: <1ms per decimation operation
 *
 * Adaptive Quality System:
 * The processor automatically adjusts quality based on:
 * - Current frame rate performance
 * - Active track count
 * - Available processing time budget
 * - GPU acceleration availability
 *
 * Memory Pool Strategy:
 * - Pre-allocated buffers for all 64 tracks
 * - Reusable scratch buffers for intermediate calculations
 * - Lock-free allocation for real-time performance
 * - Automatic pool expansion under high load
 *
 * Example usage:
 * @code
 * AdvancedDecimationProcessor processor;
 * processor.prepareForTracks(64, 1920); // 64 tracks, 1920px wide display
 *
 * // Process multiple tracks efficiently
 * auto results = processor.processMultipleTracks(
 *     trackData, trackCount, targetPixels, frameTimeBudget);
 *
 * for (const auto& result : results) {
 *     renderTrack(result.samples, result.sampleCount, result.trackIndex);
 * }
 * @endcode
 *
 * @see DecimationProcessor
 * @see PerformanceMonitor
 * @note Optimized for 64-track scenarios with extreme performance requirements
 */
class AdvancedDecimationProcessor {
public:
    /**
     * @brief Maximum number of tracks supported
     */
    static constexpr size_t MAX_TRACKS = 64;

    /**
     * @brief Maximum decimation pyramid levels
     */
    static constexpr size_t MAX_PYRAMID_LEVELS = 8;

    /**
     * @brief Default frame time budget in milliseconds
     */
    static constexpr double DEFAULT_FRAME_BUDGET_MS = 16.67; // 60 FPS

    /**
     * @enum QualityMode
     * @brief Adaptive quality levels for performance optimization
     */
    enum class QualityMode {
        Highest,        ///< Maximum quality, no compromises
        High,           ///< High quality with minor optimizations
        Balanced,       ///< Balance between quality and performance
        Performance,    ///< Favor performance over quality
        Maximum         ///< Maximum performance, minimum quality
    };

    /**
     * @struct TrackDecimationInput
     * @brief Input data for a single track decimation operation
     */
    struct TrackDecimationInput {
        const float* samples = nullptr;    ///< Input sample data
        size_t sampleCount = 0;           ///< Number of input samples
        int trackIndex = 0;               ///< Track identifier for result
        bool isVisible = true;            ///< Whether track is currently visible
        float priority = 1.0f;            ///< Rendering priority (0.0-1.0)
    };

    /**
     * @struct TrackDecimationResult
     * @brief Result of a single track decimation operation
     */
    struct TrackDecimationResult {
        const float* samples = nullptr;    ///< Processed sample data
        size_t sampleCount = 0;           ///< Number of processed samples
        int trackIndex = 0;               ///< Original track identifier
        bool wasDecimated = false;        ///< Whether decimation was applied
        QualityMode appliedQuality = QualityMode::Highest; ///< Quality level used
        double processingTimeMs = 0.0;    ///< Time spent processing this track
    };

    /**
     * @struct MultiTrackDecimationResult
     * @brief Combined result of multi-track decimation operation
     */
    struct MultiTrackDecimationResult {
        std::vector<TrackDecimationResult> trackResults; ///< Per-track results
        size_t visibleTrackCount = 0;                   ///< Number of visible tracks processed
        double totalProcessingTimeMs = 0.0;             ///< Total processing time
        QualityMode overallQuality = QualityMode::Highest; ///< Overall quality applied
        bool shouldEnableOpenGL = false;                ///< Recommendation to enable GPU acceleration
        size_t memoryUsageBytes = 0;                    ///< Current memory usage
    };

    /**
     * @struct PerformanceMetrics
     * @brief Real-time performance tracking data
     */
    struct PerformanceMetrics {
        double averageFrameTimeMs = 0.0;     ///< Average frame processing time
        double peakFrameTimeMs = 0.0;        ///< Peak frame processing time
        double frameRate = 0.0;              ///< Current frame rate
        size_t framesProcessed = 0;          ///< Total frames processed
        size_t tracksProcessed = 0;          ///< Total tracks processed
        double cpuUsagePercent = 0.0;        ///< Estimated CPU usage
        size_t memoryPoolUsageBytes = 0;     ///< Memory pool usage
        QualityMode currentQuality = QualityMode::Highest; ///< Current quality mode
    };

    /**
     * @brief Default constructor
     *
     * Initializes the processor with default settings optimized for
     * typical use cases. Memory pools and pyramid structures are
     * allocated lazily on first use.
     */
    AdvancedDecimationProcessor();

    /**
     * @brief Destructor
     *
     * Releases all allocated memory pools and cleans up internal state.
     */
    ~AdvancedDecimationProcessor();

    /**
     * @brief Prepares the processor for multi-track operation
     *
     * Pre-allocates memory pools and optimization structures for the
     * specified number of tracks and display resolution. Should be called
     * once during initialization or when track count/resolution changes significantly.
     *
     * @param numTracks Expected number of simultaneous tracks (up to MAX_TRACKS)
     * @param displayWidth Width of display area in pixels
     * @param sampleRate Audio sample rate for buffer size calculations
     *
     * @pre numTracks <= MAX_TRACKS
     * @pre displayWidth > 0
     * @pre sampleRate > 0
     *
     * @post Memory pools are allocated for specified track count
     * @post Pyramid structures are pre-calculated
     * @post Ready for high-performance processing
     */
    void prepareForTracks(size_t numTracks, int displayWidth, double sampleRate = 44100.0);

    /**
     * @brief Processes multiple tracks with adaptive quality control
     *
     * Core multi-track decimation method that processes all provided tracks
     * with automatic quality adjustment based on performance constraints.
     * Uses adaptive algorithms to maintain target frame rate.
     *
     * @param inputs Array of track input data
     * @param inputCount Number of tracks to process
     * @param targetPixels Target display width in pixels
     * @param frameTimeBudgetMs Maximum time budget for this frame (default: 16.67ms for 60fps)
     *
     * @return Complete multi-track decimation results
     *
     * @pre inputs points to valid array of inputCount elements
     * @pre targetPixels > 0
     * @pre frameTimeBudgetMs > 0
     *
     * @post All visible tracks are processed within time budget
     * @post Quality is automatically adjusted if needed
     * @post Performance metrics are updated
     *
     * @note Invisible tracks are skipped for performance
     * @note Quality automatically degrades if time budget exceeded
     */
    MultiTrackDecimationResult processMultipleTracks(
        const TrackDecimationInput* inputs,
        size_t inputCount,
        int targetPixels,
        double frameTimeBudgetMs = DEFAULT_FRAME_BUDGET_MS);

    /**
     * @brief Sets the quality mode manually
     *
     * Overrides automatic quality control and forces a specific quality
     * level. Use QualityMode::Highest to re-enable automatic control.
     *
     * @param mode Quality mode to apply
     *
     * @post Quality mode is set as specified
     * @post Automatic quality control disabled (unless mode == Highest)
     */
    void setQualityMode(QualityMode mode);

    /**
     * @brief Gets current quality mode
     *
     * @return Currently active quality mode
     */
    QualityMode getQualityMode() const { return currentQuality.load(); }

    /**
     * @brief Enables or disables OpenGL acceleration hints
     *
     * When enabled, the processor will analyze performance and suggest
     * enabling OpenGL acceleration when it would provide benefits.
     *
     * @param enable Whether to enable OpenGL recommendations
     *
     * @post OpenGL suggestions enabled/disabled as specified
     */
    void setOpenGLHintsEnabled(bool enable) { openGLHintsEnabled.store(enable); }

    /**
     * @brief Gets current performance metrics
     *
     * Returns comprehensive performance data including frame rates,
     * processing times, memory usage, and quality settings.
     *
     * @return Current performance metrics structure
     *
     * @note Thread-safe operation
     * @note Metrics are updated after each processMultipleTracks call
     */
    PerformanceMetrics getPerformanceMetrics() const;

    /**
     * @brief Resets all performance metrics and memory pools
     *
     * Clears performance history and reallocates memory pools.
     * Useful when starting fresh measurement periods or after
     * significant configuration changes.
     *
     * @post All performance metrics reset to zero
     * @post Memory pools are cleared and ready for reallocation
     * @post Quality mode reset to Highest (automatic)
     */
    void reset();

    /**
     * @brief Validates performance against 64-track requirements
     *
     * Evaluates current performance metrics against the specific
     * requirements for 64-track operation defined in the PRD.
     *
     * @return true if all 64-track performance criteria are met
     * @retval false if any performance threshold is exceeded
     *
     * Performance criteria checked:
     * - Frame rate ≥30fps minimum
     * - CPU usage <16% estimated
     * - Memory usage <640MB total
     * - No frame drops over measurement period
     */
    bool meetsPerformanceTargets() const;

private:
    //==============================================================================
    // Private Member Variables

    /**
     * @brief Memory pool for decimated sample storage
     *
     * Pre-allocated memory pools organized by track for zero-allocation
     * processing during real-time operation.
     */
    struct MemoryPool {
        std::array<std::vector<float>, MAX_TRACKS> trackBuffers;
        std::vector<float> scratchBuffer;
        std::atomic<size_t> totalAllocatedBytes{0};

        void allocateForTracks(size_t numTracks, size_t bufferSize);
        void clear();
    };

    /**
     * @brief Pyramid decimation level data
     */
    struct PyramidLevel {
        std::vector<float> data;
        size_t sampleCount = 0;
        float compressionRatio = 1.0f;
    };

    /**
     * @brief Per-track pyramid cache
     */
    struct TrackPyramid {
        std::array<PyramidLevel, MAX_PYRAMID_LEVELS> levels;
        size_t validLevels = 0;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    //==============================================================================
    // State Variables

    mutable std::mutex stateMutex;              ///< Protects non-atomic state
    MemoryPool memoryPool;                      ///< Pre-allocated memory pools
    std::array<TrackPyramid, MAX_TRACKS> trackPyramids; ///< Per-track pyramid cache

    // Configuration
    std::atomic<size_t> preparedTrackCount{0};
    std::atomic<int> preparedDisplayWidth{1920};
    std::atomic<double> preparedSampleRate{44100.0};

    // Quality control
    std::atomic<QualityMode> currentQuality{QualityMode::Highest};
    std::atomic<bool> autoQualityEnabled{true};
    std::atomic<bool> openGLHintsEnabled{true};

    // Performance tracking
    mutable std::atomic<double> averageFrameTime{0.0};
    mutable std::atomic<double> peakFrameTime{0.0};
    mutable std::atomic<size_t> totalFramesProcessed{0};
    mutable std::atomic<size_t> totalTracksProcessed{0};

    // Frame rate calculation
    mutable std::chrono::steady_clock::time_point lastFrameTime;
    mutable std::atomic<double> currentFrameRate{60.0};

    //==============================================================================
    // Private Methods

    /**
     * @brief Processes a single track with specified quality level
     */
    TrackDecimationResult processSingleTrack(
        const TrackDecimationInput& input,
        int targetPixels,
        QualityMode quality,
        double timeBudgetMs);

    /**
     * @brief Builds or updates pyramid cache for a track
     */
    void updateTrackPyramid(size_t trackIndex, const float* samples, size_t sampleCount);

    /**
     * @brief Selects optimal pyramid level for given quality and pixel count
     */
    size_t selectPyramidLevel(const TrackPyramid& pyramid, int targetPixels, QualityMode quality);

    /**
     * @brief Performs progressive min/max decimation
     */
    size_t performProgressiveDecimation(
        const float* input, size_t inputCount,
        float* output, int targetPixels,
        QualityMode quality);

    /**
     * @brief Updates performance metrics after frame processing
     */
    void updatePerformanceMetrics(const MultiTrackDecimationResult& result) const;

    /**
     * @brief Determines if OpenGL should be recommended
     */
    bool shouldRecommendOpenGL(const PerformanceMetrics& metrics) const;

    /**
     * @brief Adjusts quality based on performance feedback
     */
    QualityMode calculateAdaptiveQuality(double frameTimeMs, size_t visibleTracks) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedDecimationProcessor)
};

} // namespace oscil::render
