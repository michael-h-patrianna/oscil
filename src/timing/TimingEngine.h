/**
 * @file TimingEngine.h
 * @brief Timing and synchronization engine for Oscil multi-track oscilloscope plugin
 *
 * This file contains the TimingEngine class that provides precise timing control
 * for oscilloscope capture with multiple synchronization modes. The engine supports
 * sample-accurate DAW transport synchronization, trigger detection, and musical timing.
 *
 * Key Features:
 * - Five timing modes: Free Running, Host Sync, Time-Based, Musical, Trigger
 * - Sample-accurate timing synchronization (±1 sample precision)
 * - Real-time mode switching without audio interruption
 * - DAW transport synchronization via JUCE AudioPlayHead
 * - Trigger detection: level, edge, and slope algorithms
 * - Musical timing with BPM tracking and tempo changes
 * - Performance monitoring and statistics tracking
 *
 * Performance Requirements:
 * - Processing latency ≤1ms for all timing modes
 * - CPU overhead <0.5% additional load
 * - Sample-accurate timing within ±1 sample tolerance
 * - Zero audio dropouts during mode switching
 * - Memory usage <100KB for timing engine
 *
 * Thread Safety:
 * - Audio thread: Lock-free operations for timing calculations
 * - UI thread: Thread-safe mode switching and configuration
 * - Atomic operations for mode changes and parameter updates
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 *
 * @see audio::MultiTrackEngine
 * @see juce::AudioPlayHead
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <array>

namespace oscil {
namespace timing {

/**
 * @brief Timing synchronization modes supported by the timing engine
 *
 * These modes define different approaches to timing control for oscilloscope
 * capture, each optimized for specific use cases and analysis requirements.
 */
enum class TimingMode : int {
    /**
     * @brief Free Running - Continuous capture without synchronization
     *
     * Captures audio continuously without any external timing reference.
     * Provides the lowest latency and most responsive real-time display.
     * Ideal for general-purpose audio analysis and monitoring.
     */
    FreeRunning = 0,

    /**
     * @brief Host Sync - Synchronize with DAW transport
     *
     * Synchronizes capture timing with the host DAW's transport position
     * and playback state. Provides sample-accurate alignment with musical
     * content and automation. Ideal for analyzing specific sections of a mix.
     */
    HostSync = 1,

    /**
     * @brief Time-Based - Absolute time intervals
     *
     * Captures at precise time intervals specified in milliseconds or seconds.
     * Provides consistent timing independent of musical context. Ideal for
     * technical analysis and measurement applications.
     */
    TimeBased = 2,

    /**
     * @brief Musical - BPM-based musical timing
     *
     * Synchronizes capture with musical timing based on current BPM and
     * time signature. Captures at musically relevant intervals (beats, bars).
     * Ideal for rhythmic analysis and tempo-synchronized visualization.
     */
    Musical = 3,

    /**
     * @brief Trigger - Signal-based triggering
     *
     * Captures when audio signal meets specific trigger conditions (level,
     * edge, slope). Provides event-driven capture for analyzing transients,
     * peaks, and signal characteristics. Ideal for drum analysis and
     * event detection.
     */
    Trigger = 4
};

/**
 * @brief Trigger detection algorithms for signal-based timing
 */
enum class TriggerType : int {
    Level = 0,      ///< Simple threshold comparison with signal amplitude
    Edge = 1,       ///< Rising/falling edge detection using derivative
    Slope = 2       ///< Multi-sample slope analysis for accurate timing
};

/**
 * @brief Trigger edge direction for edge-based detection
 */
enum class TriggerEdge : int {
    Rising = 0,     ///< Trigger on signal rising above threshold
    Falling = 1,    ///< Trigger on signal falling below threshold
    Both = 2        ///< Trigger on both rising and falling edges
};

/**
 * @brief Configuration parameters for trigger-based timing
 */
struct TriggerConfig {
    TriggerType type = TriggerType::Level;      ///< Detection algorithm type
    TriggerEdge edge = TriggerEdge::Rising;     ///< Edge direction for edge triggers
    float threshold = 0.5f;                    ///< Trigger threshold level (-1.0 to 1.0)
    float hysteresis = 0.1f;                   ///< Hysteresis amount to prevent false triggers
    int holdOffSamples = 512;                  ///< Minimum samples between triggers
    int slopeWindowSamples = 8;                ///< Window size for slope analysis
    bool enabled = true;                       ///< Whether trigger detection is active

    /**
     * @brief Validates trigger configuration parameters
     * @return true if configuration is valid, false otherwise
     */
    bool isValid() const {
        return threshold >= -1.0f && threshold <= 1.0f &&
               hysteresis >= 0.0f && hysteresis <= 1.0f &&
               holdOffSamples > 0 && holdOffSamples <= 48000 &&
               slopeWindowSamples > 0 && slopeWindowSamples <= 256;
    }
};

/**
 * @brief Configuration parameters for musical timing
 */
struct MusicalConfig {
    int beatDivision = 4;           ///< Beat division (1=whole note, 4=quarter note, 8=eighth note)
    int barLength = 4;              ///< Number of beats per bar
    bool snapToBeats = true;        ///< Whether to snap timing to exact beat positions
    bool followTempoChanges = true; ///< Whether to adapt to DAW tempo changes
    double customBPM = 120.0;       ///< Custom BPM when not following DAW tempo

    /**
     * @brief Validates musical configuration parameters
     * @return true if configuration is valid, false otherwise
     */
    bool isValid() const {
        return beatDivision > 0 && beatDivision <= 64 &&
               barLength > 0 && barLength <= 32 &&
               customBPM >= 60.0 && customBPM <= 300.0;
    }
};

/**
 * @brief Configuration parameters for time-based timing
 */
struct TimeBasedConfig {
    double intervalMs = 100.0;      ///< Time interval in milliseconds
    bool driftCompensation = true;  ///< Whether to compensate for timing drift
    bool adaptToSampleRate = true;  ///< Whether to adapt intervals to sample rate changes

    /**
     * @brief Validates time-based configuration parameters
     * @return true if configuration is valid, false otherwise
     */
    bool isValid() const {
        return intervalMs >= 1.0 && intervalMs <= 10000.0;
    }
};

/**
 * @brief Current timing state information
 */
struct TimingState {
    TimingMode currentMode = TimingMode::FreeRunning;   ///< Active timing mode
    bool isActive = false;                              ///< Whether timing is currently active
    uint64_t samplesProcessed = 0;                      ///< Total samples processed
    uint64_t captureEvents = 0;                         ///< Number of capture events triggered
    uint64_t missedTriggers = 0;                        ///< Number of missed trigger events
    double currentBPM = 120.0;                          ///< Current BPM from DAW or custom
    double sampleRate = 44100.0;                        ///< Current sample rate

    // Timing accuracy statistics
    double averageTimingError = 0.0;                    ///< Average timing error in samples
    double maxTimingError = 0.0;                        ///< Maximum timing error in samples
    uint64_t accuracyMeasurements = 0;                  ///< Number of accuracy measurements
};

/**
 * @brief Performance statistics for timing engine monitoring
 */
struct TimingPerformanceStats {
    std::atomic<uint64_t> processBlockCalls{0};        ///< Number of processBlock calls
    std::atomic<uint64_t> timingCalculations{0};       ///< Number of timing calculations
    std::atomic<uint64_t> triggerDetections{0};        ///< Number of trigger detections
    std::atomic<uint64_t> modeChanges{0};              ///< Number of mode changes
    std::atomic<double> averageProcessingTime{0.0};    ///< Average processing time in ms
    std::atomic<double> maxProcessingTime{0.0};        ///< Maximum processing time in ms

    // Copy constructor and assignment operator for thread safety
    TimingPerformanceStats() = default;

    TimingPerformanceStats(const TimingPerformanceStats& other)
        : processBlockCalls{other.processBlockCalls.load()}
        , timingCalculations{other.timingCalculations.load()}
        , triggerDetections{other.triggerDetections.load()}
        , modeChanges{other.modeChanges.load()}
        , averageProcessingTime{other.averageProcessingTime.load()}
        , maxProcessingTime{other.maxProcessingTime.load()}
    {}

    TimingPerformanceStats& operator=(const TimingPerformanceStats& other) {
        if (this != &other) {
            processBlockCalls.store(other.processBlockCalls.load());
            timingCalculations.store(other.timingCalculations.load());
            triggerDetections.store(other.triggerDetections.load());
            modeChanges.store(other.modeChanges.load());
            averageProcessingTime.store(other.averageProcessingTime.load());
            maxProcessingTime.store(other.maxProcessingTime.load());
        }
        return *this;
    }

    void reset() {
        processBlockCalls.store(0);
        timingCalculations.store(0);
        triggerDetections.store(0);
        modeChanges.store(0);
        averageProcessingTime.store(0.0);
        maxProcessingTime.store(0.0);
    }
};

/**
 * @brief Timing and synchronization engine for multi-track oscilloscope
 *
 * The TimingEngine provides precise timing control for audio capture across
 * multiple synchronization modes. It integrates with DAW transport, supports
 * trigger detection, and maintains sample-accurate timing requirements.
 *
 * Key Features:
 * - Five timing modes with real-time switching
 * - Sample-accurate DAW synchronization via AudioPlayHead
 * - Configurable trigger detection algorithms
 * - Musical timing with BPM tracking
 * - Performance monitoring and statistics
 * - Thread-safe operation for audio and UI threads
 *
 * Usage Example:
 * @code
 * // Initialize timing engine
 * TimingEngine timingEngine;
 * timingEngine.prepareToPlay(44100.0, 512);
 *
 * // Set musical timing mode
 * MusicalConfig config;
 * config.beatDivision = 4;  // Quarter note timing
 * timingEngine.setMusicalConfig(config);
 * timingEngine.setTimingMode(TimingMode::Musical);
 *
 * // In audio callback
 * auto playHead = getPlayHead();
 * bool shouldCapture = timingEngine.shouldCaptureAtCurrentTime(playHead, numSamples);
 * if (shouldCapture) {
 *     // Trigger capture event
 * }
 * @endcode
 *
 * Performance Requirements:
 * - Processing latency ≤1ms
 * - Sample-accurate timing (±1 sample)
 * - Zero audio thread blocking
 * - Memory usage <100KB
 *
 * @see TimingMode
 * @see TriggerConfig
 * @see MusicalConfig
 * @see TimeBasedConfig
 */
class TimingEngine {
public:
    /**
     * @brief Constructs a new timing engine with default configuration
     */
    TimingEngine();

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~TimingEngine();

    // === Core Lifecycle ===

    /**
     * @brief Prepares the timing engine for audio processing
     *
     * Initializes internal state and allocates resources needed for timing
     * calculations. Must be called before any audio processing.
     *
     * @param sampleRate The audio sample rate in Hz
     * @param samplesPerBlock Maximum samples per audio block
     *
     * @pre sampleRate > 0 and samplesPerBlock > 0
     * @post Engine is prepared for audio processing
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    /**
     * @brief Releases timing engine resources
     *
     * Cleans up all allocated resources and resets internal state.
     * Should be called when audio processing stops.
     *
     * @post All resources are released and engine is in idle state
     */
    void releaseResources();

    // === Timing Mode Management ===

    /**
     * @brief Sets the current timing mode
     *
     * Changes the active timing mode. Mode switching is atomic and takes
     * effect immediately without blocking the audio thread.
     *
     * @param mode The new timing mode to activate
     *
     * @note Thread-safe - can be called from UI thread
     */
    void setTimingMode(TimingMode mode);

    /**
     * @brief Gets the current timing mode
     *
     * @return The currently active timing mode
     *
     * @note Thread-safe
     */
    TimingMode getTimingMode() const;

    // === Configuration Management ===

    /**
     * @brief Sets trigger detection configuration
     *
     * Updates trigger parameters for trigger-based timing mode.
     * Changes take effect on the next audio block.
     *
     * @param config Trigger configuration parameters
     * @return true if configuration is valid and applied, false otherwise
     *
     * @note Thread-safe - can be called from UI thread
     */
    bool setTriggerConfig(const TriggerConfig& config);

    /**
     * @brief Gets current trigger configuration
     *
     * @return Current trigger configuration
     */
    TriggerConfig getTriggerConfig() const;

    /**
     * @brief Sets musical timing configuration
     *
     * Updates musical timing parameters for musical timing mode.
     * Changes take effect on the next audio block.
     *
     * @param config Musical timing configuration
     * @return true if configuration is valid and applied, false otherwise
     *
     * @note Thread-safe - can be called from UI thread
     */
    bool setMusicalConfig(const MusicalConfig& config);

    /**
     * @brief Gets current musical timing configuration
     *
     * @return Current musical timing configuration
     */
    MusicalConfig getMusicalConfig() const;

    /**
     * @brief Sets time-based timing configuration
     *
     * Updates time-based timing parameters for time-based timing mode.
     * Changes take effect on the next audio block.
     *
     * @param config Time-based timing configuration
     * @return true if configuration is valid and applied, false otherwise
     *
     * @note Thread-safe - can be called from UI thread
     */
    bool setTimeBasedConfig(const TimeBasedConfig& config);

    /**
     * @brief Gets current time-based timing configuration
     *
     * @return Current time-based timing configuration
     */
    TimeBasedConfig getTimeBasedConfig() const;

    // === Core Timing Operations ===

    /**
     * @brief Determines if capture should occur at the current time
     *
     * This is the main timing decision function called from the audio thread.
     * It analyzes the current timing mode, DAW state, and audio signal to
     * determine if a capture event should be triggered.
     *
     * @param playHead AudioPlayHead for DAW synchronization (may be nullptr)
     * @param audioData Audio samples for trigger analysis (may be nullptr)
     * @param numSamples Number of samples in current audio block
     * @return true if capture should be triggered, false otherwise
     *
     * @note Real-time safe - must complete within <1ms
     * @note Called from audio thread
     */
    bool shouldCaptureAtCurrentTime(juce::AudioPlayHead* playHead,
                                    const float* const* audioData,
                                    int numSamples);

    /**
     * @brief Updates timing calculations for the current audio block
     *
     * Processes timing calculations and updates internal state based on
     * the current audio block. Should be called once per audio block.
     *
     * @param playHead AudioPlayHead for DAW synchronization
     * @param numSamples Number of samples in current audio block
     *
     * @note Real-time safe
     * @note Called from audio thread
     */
    void processTimingBlock(juce::AudioPlayHead* playHead, int numSamples);

    /**
     * @brief Forces an immediate trigger event
     *
     * Manually triggers a capture event regardless of current timing mode.
     * Useful for user-initiated capture or testing purposes.
     *
     * @note Thread-safe
     */
    void forceTrigger();

    // === State and Statistics ===

    /**
     * @brief Gets current timing state information
     *
     * @return Current timing state including mode, statistics, and measurements
     *
     * @note Thread-safe
     */
    TimingState getTimingState() const;

    /**
     * @brief Gets performance statistics
     *
     * @return Performance statistics for monitoring and optimization
     *
     * @note Thread-safe
     */
    TimingPerformanceStats getPerformanceStats() const;

    /**
     * @brief Resets all statistics and counters
     *
     * Clears performance statistics and timing measurements.
     *
     * @note Thread-safe
     */
    void resetStatistics();

    // === Utility Functions ===

    /**
     * @brief Converts BPM to samples per beat
     *
     * @param bpm Beats per minute
     * @param sampleRate Audio sample rate
     * @return Number of samples per beat
     */
    static double bpmToSamplesPerBeat(double bpm, double sampleRate);

    /**
     * @brief Converts time interval to samples
     *
     * @param timeMs Time interval in milliseconds
     * @param sampleRate Audio sample rate
     * @return Number of samples for the time interval
     */
    static int timeToSamples(double timeMs, double sampleRate);

    /**
     * @brief Validates timing mode value
     *
     * @param mode Timing mode value to validate
     * @return true if mode is valid, false otherwise
     */
    static bool isValidTimingMode(int mode);

private:
    // === Internal State ===

    // Core timing state
    std::atomic<TimingMode> currentMode{TimingMode::FreeRunning};
    std::atomic<bool> isActive{false};

    // Audio processing parameters
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    bool isPrepared = false;

    // Timing configurations (protected by mutex for thread safety)
    mutable std::mutex configMutex;
    TriggerConfig triggerConfig;
    MusicalConfig musicalConfig;
    TimeBasedConfig timeBasedConfig;

    // Internal timing state
    uint64_t samplesProcessed = 0;
    uint64_t captureEvents = 0;
    uint64_t missedTriggers = 0;

    // Trigger detection state
    std::array<float, 256> triggerHistory{};    ///< Sample history for trigger analysis
    int triggerHistoryIndex = 0;
    uint64_t lastTriggerSample = 0;
    float lastSampleValue = 0.0f;

    // Musical timing state
    double lastBPM = 120.0;
    double samplesPerBeat = 22050.0;  // Default for 120 BPM at 44.1kHz
    uint64_t lastBeatSample = 0;

    // Time-based timing state
    uint64_t lastTimeBasedCapture = 0;
    double timeBasedInterval = 0.0;  // In samples

    // Performance monitoring
    mutable TimingPerformanceStats performanceStats;
    juce::Time processingStartTime;

    // === Internal Methods ===

    /**
     * @brief Processes free running timing mode
     * @return true if capture should be triggered
     */
    bool processFreeRunningMode();

    /**
     * @brief Processes host sync timing mode
     * @param playHead AudioPlayHead for DAW synchronization
     * @param numSamples Number of samples in current block
     * @return true if capture should be triggered
     */
    bool processHostSyncMode(juce::AudioPlayHead* playHead, int numSamples);

    /**
     * @brief Processes time-based timing mode
     * @param numSamples Number of samples in current block
     * @return true if capture should be triggered
     */
    bool processTimeBasedMode(int numSamples);

    /**
     * @brief Processes musical timing mode
     * @param playHead AudioPlayHead for DAW synchronization
     * @param numSamples Number of samples in current block
     * @return true if capture should be triggered
     */
    bool processMusicalMode(juce::AudioPlayHead* playHead, int numSamples);

    /**
     * @brief Processes trigger timing mode
     * @param audioData Audio samples for analysis
     * @param numSamples Number of samples in current block
     * @return true if capture should be triggered
     */
    bool processTriggerMode(const float* const* audioData, int numSamples);

    /**
     * @brief Detects level-based triggers
     * @param sample Current audio sample
     * @return true if trigger detected
     */
    bool detectLevelTrigger(float sample);

    /**
     * @brief Detects edge-based triggers
     * @param sample Current audio sample
     * @return true if trigger detected
     */
    bool detectEdgeTrigger(float sample);

    /**
     * @brief Detects slope-based triggers
     * @param audioData Audio samples for analysis
     * @param numSamples Number of samples
     * @return true if trigger detected
     */
    bool detectSlopeTrigger(const float* audioData, int numSamples);

    /**
     * @brief Updates BPM from AudioPlayHead
     * @param playHead AudioPlayHead for BPM information
     */
    void updateBPMFromPlayHead(juce::AudioPlayHead* playHead);

    /**
     * @brief Updates performance statistics
     * @param processingTimeMs Processing time for current block
     */
    void updatePerformanceStats(double processingTimeMs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimingEngine)
};

} // namespace timing
} // namespace oscil
