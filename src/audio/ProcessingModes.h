#pragma once

#include <cmath>
#include <cstddef>
#include <algorithm>

namespace audio {

/**
 * @brief Available signal processing modes for audio analysis
 *
 * Each mode transforms stereo input into different visualization formats
 * for specialized audio analysis workflows.
 */
enum class SignalProcessingMode {
    /**
     * Full Stereo: Display left and right channels separately
     * Outputs: L, R (2 channels)
     */
    FullStereo,

    /**
     * Mono Sum: Sum left and right channels
     * Formula: (L + R) / 2
     * Outputs: Sum (1 channel)
     */
    MonoSum,

    /**
     * Mid/Side: M/S matrix decoding for stereo width analysis
     * Formulas: M = (L + R) / 2, S = (L - R) / 2
     * Outputs: Mid, Side (2 channels)
     */
    MidSide,

    /**
     * Left Only: Display only left channel
     * Outputs: L (1 channel)
     */
    LeftOnly,

    /**
     * Right Only: Display only right channel
     * Outputs: R (1 channel)
     */
    RightOnly,

    /**
     * Difference: Phase difference analysis
     * Formula: L - R
     * Outputs: Difference (1 channel)
     */
    Difference
};

/**
 * @brief Gets the number of output channels for a processing mode
 *
 * @param mode The signal processing mode
 * @return Number of output channels (1 or 2)
 */
constexpr int getOutputChannelCount(SignalProcessingMode mode) {
    switch (mode) {
        case SignalProcessingMode::FullStereo:
        case SignalProcessingMode::MidSide:
            return 2;
        case SignalProcessingMode::MonoSum:
        case SignalProcessingMode::LeftOnly:
        case SignalProcessingMode::RightOnly:
        case SignalProcessingMode::Difference:
            return 1;
    }
    return 1;
}

/**
 * @brief Gets human-readable name for a processing mode
 *
 * @param mode The signal processing mode
 * @return String representation of the mode
 */
inline const char* getProcessingModeName(SignalProcessingMode mode) {
    switch (mode) {
        case SignalProcessingMode::FullStereo: return "Full Stereo";
        case SignalProcessingMode::MonoSum:    return "Mono Sum";
        case SignalProcessingMode::MidSide:    return "Mid/Side";
        case SignalProcessingMode::LeftOnly:   return "Left Only";
        case SignalProcessingMode::RightOnly:  return "Right Only";
        case SignalProcessingMode::Difference: return "Difference";
    }
    return "Unknown";
}

/**
 * @brief Configuration for signal processing
 *
 * Contains all parameters needed for signal processing modes including
 * correlation analysis settings.
 */
struct ProcessingConfig {
    SignalProcessingMode mode = SignalProcessingMode::FullStereo;

    // Correlation analysis settings
    bool enableCorrelation = true;
    size_t correlationWindowSize = 1024;  // Samples for correlation calculation

    // Processing precision
    bool useDoublePrec = true;  // Use double precision for M/S calculations

    // Performance settings
    float correlationUpdateRate = 30.0f;  // Hz - how often to update correlation

    ProcessingConfig() = default;
    ProcessingConfig(SignalProcessingMode processingMode) : mode(processingMode) {}
};

/**
 * @brief Results of correlation analysis between stereo channels
 *
 * Provides comprehensive stereo imaging information for professional
 * audio analysis workflows.
 */
struct CorrelationMetrics {
    float correlation = 0.0f;     // Pearson correlation coefficient [-1.0, 1.0]
    float phase = 0.0f;          // Phase difference in radians [-π, π]
    float stereoWidth = 0.0f;    // Stereo width metric [0.0, 2.0]

    // Internal state for incremental calculation
    double sumL = 0.0;
    double sumR = 0.0;
    double sumLL = 0.0;
    double sumRR = 0.0;
    double sumLR = 0.0;
    size_t sampleCount = 0;

    /**
     * @brief Reset correlation state for new calculation window
     */
    void reset() {
        correlation = 0.0f;
        phase = 0.0f;
        stereoWidth = 0.0f;
        sumL = sumR = sumLL = sumRR = sumLR = 0.0;
        sampleCount = 0;
    }

    /**
     * @brief Calculate final correlation coefficient from accumulated sums
     */
    void calculateFinalMetrics() {
        if (sampleCount < 2) {
            correlation = 0.0f;
            return;
        }

        // Pearson correlation coefficient
        double meanL = sumL / sampleCount;
        double meanR = sumR / sampleCount;

        double numerator = sumLR - sampleCount * meanL * meanR;
        double denomL = sumLL - sampleCount * meanL * meanL;
        double denomR = sumRR - sampleCount * meanR * meanR;
        double denominator = std::sqrt(denomL * denomR);

        if (denominator > 1e-10) {
            correlation = static_cast<float>(numerator / denominator);
            // Clamp to valid range to handle numerical precision issues
            correlation = std::max(-1.0f, std::min(1.0f, correlation));
        } else {
            correlation = 0.0f;
        }

        // Stereo width derived from correlation
        // Width = 2 * sqrt(1 - |correlation|)
        stereoWidth = 2.0f * std::sqrt(1.0f - std::abs(correlation));
    }
};

} // namespace audio
