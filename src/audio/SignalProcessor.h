#pragma once

#include "ProcessingModes.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>

namespace audio {

/**
 * @brief Real-time signal processor for multi-channel audio analysis
 *
 * Provides mathematical signal processing modes for professional audio
 * visualization including stereo analysis, correlation metrics, and
 * mid/side processing.
 *
 * Performance characteristics:
 * - Real-time safe audio processing
 * - ≤2ms processing latency for all modes
 * - Mathematical precision for M/S conversion (±0.001 tolerance)
 * - Correlation accuracy ±0.01 for known signals
 * - Atomic mode switching without audio interruption
 *
 * @see SignalProcessingMode
 * @see ProcessingConfig
 * @see CorrelationMetrics
 */
class SignalProcessor {
public:
    static constexpr size_t MAX_CHANNELS = 64;
    static constexpr size_t MAX_BLOCK_SIZE = 2048;

    /**
     * @brief Processed audio output with correlation metrics
     */
    struct ProcessedOutput {
        float outputChannels[2][MAX_BLOCK_SIZE]; // Max 2 output channels
        size_t numOutputChannels = 0;
        size_t numSamples = 0;
        CorrelationMetrics metrics;
        bool metricsValid = false;

        ProcessedOutput() {
            // Initialize arrays to zero
            for (int ch = 0; ch < 2; ++ch) {
                std::fill(outputChannels[ch], outputChannels[ch] + MAX_BLOCK_SIZE, 0.0f);
            }
        }
    };

    /**
     * @brief Constructs processor with default configuration
     */
    SignalProcessor();

    /**
     * @brief Constructs processor with specific configuration
     */
    explicit SignalProcessor(const ProcessingConfig& config);

    /**
     * @brief Destructor
     */
    ~SignalProcessor() = default;

    // === Configuration ===

    /**
     * @brief Sets the processing configuration
     * Thread-safe, takes effect on next processBlock call
     */
    void setConfig(const ProcessingConfig& newConfig);

    /**
     * @brief Gets current processing configuration
     */
    ProcessingConfig getConfig() const;

    /**
     * @brief Sets processing mode only
     * Convenience method for mode-only changes
     */
    void setProcessingMode(SignalProcessingMode mode);

    /**
     * @brief Gets current processing mode
     */
    SignalProcessingMode getProcessingMode() const;

    // === Audio Processing ===

    /**
     * @brief Processes stereo audio block
     *
     * @param leftChannel Left channel input samples
     * @param rightChannel Right channel input samples
     * @param numSamples Number of samples to process
     * @param output Processed output with correlation metrics
     *
     * @note Real-time safe, no allocations or blocking operations
     */
    void processBlock(const float* leftChannel,
                     const float* rightChannel,
                     size_t numSamples,
                     ProcessedOutput& output);

    // === Statistics ===

    /**
     * @brief Performance statistics for monitoring
     */
    struct PerformanceStats {
        std::atomic<uint64_t> blocksProcessed{0};
        std::atomic<uint64_t> totalSamplesProcessed{0};
        std::atomic<uint64_t> modeChanges{0};
        std::atomic<float> averageProcessingTimeMs{0.0f};
    };

    /**
     * @brief Gets performance statistics
     */
    const PerformanceStats& getPerformanceStats() const { return stats; }

    /**
     * @brief Resets performance statistics
     */
    void resetStats();

private:
    // === Internal State ===

    // Thread-safe configuration
    mutable std::mutex configMutex;
    ProcessingConfig currentConfig;

    // Correlation state management
    CorrelationMetrics correlationState;
    size_t correlationSampleCount = 0;

    // Performance monitoring
    mutable PerformanceStats stats;

    // === Processing Implementation ===

    /**
     * @brief Process Full Stereo mode (L, R)
     */
    void processFullStereo(const float* left, const float* right,
                          size_t numSamples, ProcessedOutput& output);

    /**
     * @brief Process Mono Sum mode ((L+R)/2)
     */
    void processMonoSum(const float* left, const float* right,
                       size_t numSamples, ProcessedOutput& output);

    /**
     * @brief Process Mid/Side mode (M=(L+R)/2, S=(L-R)/2)
     */
    void processMidSide(const float* left, const float* right,
                       size_t numSamples, ProcessedOutput& output);

    /**
     * @brief Process Left Only mode
     */
    void processLeftOnly(const float* left, const float* right,
                        size_t numSamples, ProcessedOutput& output);

    /**
     * @brief Process Right Only mode
     */
    void processRightOnly(const float* left, const float* right,
                         size_t numSamples, ProcessedOutput& output);

    /**
     * @brief Process Difference mode (L-R)
     */
    void processDifference(const float* left, const float* right,
                          size_t numSamples, ProcessedOutput& output);

    /**
     * @brief Update correlation metrics incrementally
     */
    void updateCorrelation(const float* left, const float* right,
                          size_t numSamples, CorrelationMetrics& metrics);

    /**
     * @brief Check if correlation should be updated this block
     */
    bool shouldUpdateCorrelation() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalProcessor)
};

} // namespace audio
