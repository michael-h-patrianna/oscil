/**
 * @file MeasurementData.h
 * @brief Data structures for audio measurement information
 *
 * This file defines data structures for transferring measurement information
 * from audio processing to UI components including correlation metrics,
 * stereo width measurements, and other audio analysis data.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include "ProcessingModes.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>

namespace audio {

/**
 * @struct MeasurementData
 * @brief Container for all measurement data transferred to UI
 *
 * Fixed-size structure containing measurement data calculated during
 * audio processing. Designed for lock-free transfer to UI thread.
 */
struct MeasurementData {
    // Correlation and stereo imaging
    CorrelationMetrics correlationMetrics;
    bool correlationValid = false;

    // Stereo width measurement (alternative calculation)
    float stereoWidth = 1.0f;
    bool stereoWidthValid = false;

    // Peak levels for each channel
    static constexpr size_t MAX_CHANNELS = 64;
    std::array<float, MAX_CHANNELS> peakLevels{};
    std::array<float, MAX_CHANNELS> rmsLevels{};
    bool levelsValid = false;

    // Processing mode when measurements were taken
    SignalProcessingMode processingMode = SignalProcessingMode::FullStereo;

    // Timing information
    uint64_t measurementTimestamp = 0;
    uint32_t measurementId = 0;  // Incrementing ID for freshness tracking

    /**
     * @brief Default constructor initializes to safe values
     */
    MeasurementData() {
        clear();
    }

    /**
     * @brief Clears all measurement data to safe defaults
     */
    void clear() {
        correlationMetrics.reset();
        correlationValid = false;
        stereoWidth = 1.0f;
        stereoWidthValid = false;
        peakLevels.fill(0.0f);
        rmsLevels.fill(0.0f);
        levelsValid = false;
        processingMode = SignalProcessingMode::FullStereo;
        measurementTimestamp = 0;
        measurementId = 0;
    }

    /**
     * @brief Updates correlation metrics
     *
     * @param metrics New correlation metrics
     * @param timestamp Measurement timestamp
     */
    void updateCorrelation(const CorrelationMetrics& metrics, uint64_t timestamp) {
        correlationMetrics = metrics;
        correlationValid = true;
        measurementTimestamp = timestamp;
        ++measurementId;
    }

    /**
     * @brief Updates stereo width measurement
     *
     * @param width Stereo width value (0.0 to 2.0)
     * @param timestamp Measurement timestamp
     */
    void updateStereoWidth(float width, uint64_t timestamp) {
        stereoWidth = width;
        stereoWidthValid = true;
        measurementTimestamp = timestamp;
        ++measurementId;
    }

    /**
     * @brief Updates channel level measurements
     *
     * @param peaks Peak levels for each channel
     * @param rms RMS levels for each channel
     * @param numChannels Number of valid channels
     * @param timestamp Measurement timestamp
     */
    void updateLevels(const float* peaks, const float* rms, size_t numChannels, uint64_t timestamp) {
        const size_t channelCount = std::min(numChannels, MAX_CHANNELS);

        for (size_t ch = 0; ch < channelCount; ++ch) {
            peakLevels[ch] = peaks ? peaks[ch] : 0.0f;
            rmsLevels[ch] = rms ? rms[ch] : 0.0f;
        }

        // Clear unused channels
        for (size_t ch = channelCount; ch < MAX_CHANNELS; ++ch) {
            peakLevels[ch] = 0.0f;
            rmsLevels[ch] = 0.0f;
        }

        levelsValid = true;
        measurementTimestamp = timestamp;
        ++measurementId;
    }

    /**
     * @brief Sets the processing mode for these measurements
     *
     * @param mode Processing mode when measurements were taken
     */
    void setProcessingMode(SignalProcessingMode mode) {
        processingMode = mode;
    }

    /**
     * @brief Checks if measurement data is relevant for given processing mode
     *
     * @param mode Processing mode to check against
     * @return true if measurements are relevant for the mode
     */
    bool isRelevantForMode(SignalProcessingMode mode) const {
        switch (mode) {
            case SignalProcessingMode::FullStereo:
            case SignalProcessingMode::MidSide:
            case SignalProcessingMode::Difference:
                return correlationValid || stereoWidthValid;

            case SignalProcessingMode::MonoSum:
            case SignalProcessingMode::LeftOnly:
            case SignalProcessingMode::RightOnly:
                return levelsValid; // Only level measurements relevant
        }
        return false;
    }

    /**
     * @brief Gets age of measurements in milliseconds
     *
     * @param currentTimestamp Current timestamp for comparison
     * @return Age in milliseconds
     */
    uint64_t getAgeMs(uint64_t currentTimestamp) const {
        return (currentTimestamp > measurementTimestamp) ?
               (currentTimestamp - measurementTimestamp) : 0;
    }

    /**
     * @brief Checks if measurements are fresh (< 100ms old)
     *
     * @param currentTimestamp Current timestamp for comparison
     * @return true if measurements are considered fresh
     */
    bool isFresh(uint64_t currentTimestamp) const {
        constexpr uint64_t MAX_AGE_MS = 100;
        return getAgeMs(currentTimestamp) < MAX_AGE_MS;
    }
};

/**
 * @class MeasurementDataBridge
 * @brief Lock-free bridge for transferring measurement data to UI
 *
 * Provides thread-safe transfer of measurement data from audio processing
 * thread to UI thread using atomic operations and double buffering.
 */
class MeasurementDataBridge {
public:
    /**
     * @brief Constructs measurement data bridge
     */
    MeasurementDataBridge();

    /**
     * @brief Destructor
     */
    ~MeasurementDataBridge() = default;

    /**
     * @brief Pushes new measurement data from audio thread
     *
     * Non-blocking operation that updates measurement data for UI consumption.
     * Safe to call from real-time audio processing thread.
     *
     * @param data Measurement data to push
     *
     * @note Thread-safe: Can be called from audio thread
     * @note Non-blocking: Never allocates memory or waits
     */
    void pushMeasurementData(const MeasurementData& data);

    /**
     * @brief Pulls latest measurement data for UI thread
     *
     * Gets the most recent measurement data available. Returns true if
     * new data is available since the last call.
     *
     * @param outData Reference to receive measurement data
     * @return true if new data was available
     *
     * @note Thread-safe: Safe to call from UI thread
     * @note Non-blocking: Always returns immediately
     */
    bool pullLatestMeasurements(MeasurementData& outData);

    /**
     * @brief Gets performance statistics
     *
     * @return Number of measurement updates pushed
     */
    uint64_t getTotalMeasurementsPushed() const {
        return measurementsPushed.load(std::memory_order_acquire);
    }

    /**
     * @brief Gets pull statistics
     *
     * @return Number of measurement pulls performed
     */
    uint64_t getTotalMeasurementsPulled() const {
        return measurementsPulled.load(std::memory_order_acquire);
    }

    /**
     * @brief Resets performance statistics
     */
    void resetStats();

private:
    // Double-buffered measurement data for lock-free communication
    std::unique_ptr<MeasurementData> writeData;
    std::unique_ptr<MeasurementData> readData;

    // Atomic coordination for buffer swapping
    std::atomic<bool> dataReady{false};

    // Performance counters
    std::atomic<uint64_t> measurementsPushed{0};
    std::atomic<uint64_t> measurementsPulled{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementDataBridge)
};

} // namespace audio
