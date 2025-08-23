#include "SignalProcessor.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace audio {

SignalProcessor::SignalProcessor()
    : currentConfig(ProcessingConfig{}) {
}

SignalProcessor::SignalProcessor(const ProcessingConfig& config)
    : currentConfig(config) {
}

void SignalProcessor::setConfig(const ProcessingConfig& newConfig) {
    std::lock_guard<std::mutex> lock(configMutex);
    currentConfig = newConfig;
    stats.modeChanges.fetch_add(1);
}

ProcessingConfig SignalProcessor::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return currentConfig;
}

void SignalProcessor::setProcessingMode(SignalProcessingMode mode) {
    std::lock_guard<std::mutex> lock(configMutex);
    currentConfig.mode = mode;
    stats.modeChanges.fetch_add(1);
}

SignalProcessingMode SignalProcessor::getProcessingMode() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return currentConfig.mode;
}

void SignalProcessor::processBlock(const float* leftChannel,
                                  const float* rightChannel,
                                  size_t numSamples,
                                  ProcessedOutput& output) {
    if (numSamples == 0 || numSamples > MAX_BLOCK_SIZE) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Get current config atomically
    ProcessingConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = currentConfig;
    }

    // Set output parameters
    output.numSamples = numSamples;
    output.numOutputChannels = static_cast<size_t>(getOutputChannelCount(config.mode));
    output.metricsValid = false;

    // Process according to current mode
    switch (config.mode) {
        case SignalProcessingMode::FullStereo:
            processFullStereo(leftChannel, rightChannel, numSamples, output);
            break;
        case SignalProcessingMode::MonoSum:
            processMonoSum(leftChannel, rightChannel, numSamples, output);
            break;
        case SignalProcessingMode::MidSide:
            processMidSide(leftChannel, rightChannel, numSamples, output);
            break;
        case SignalProcessingMode::LeftOnly:
            processLeftOnly(leftChannel, rightChannel, numSamples, output);
            break;
        case SignalProcessingMode::RightOnly:
            processRightOnly(leftChannel, rightChannel, numSamples, output);
            break;
        case SignalProcessingMode::Difference:
            processDifference(leftChannel, rightChannel, numSamples, output);
            break;
    }

    // Update correlation if enabled
    if (config.enableCorrelation && shouldUpdateCorrelation()) {
        updateCorrelation(leftChannel, rightChannel, numSamples, output.metrics);
        output.metricsValid = true;
    }

    // Update performance stats
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    float processingTimeMs = duration.count() / 1000.0f;

    stats.blocksProcessed.fetch_add(1);
    stats.totalSamplesProcessed.fetch_add(numSamples);

    // Simple moving average for processing time
    float currentAvg = stats.averageProcessingTimeMs.load();
    float newAvg = (currentAvg * 0.9f) + (processingTimeMs * 0.1f);
    stats.averageProcessingTimeMs.store(newAvg);
}

void SignalProcessor::resetStats() {
    stats.blocksProcessed.store(0);
    stats.totalSamplesProcessed.store(0);
    stats.modeChanges.store(0);
    stats.averageProcessingTimeMs.store(0.0f);
}

void SignalProcessor::processFullStereo(const float* left, const float* right,
                                       size_t numSamples, ProcessedOutput& output) {
    // Copy left channel to output channel 0, right to output channel 1
    std::copy(left, left + numSamples, output.outputChannels[0]);
    std::copy(right, right + numSamples, output.outputChannels[1]);
}

void SignalProcessor::processMonoSum(const float* left, const float* right,
                                    size_t numSamples, ProcessedOutput& output) {
    // Mono sum: (L + R) / 2
    for (size_t i = 0; i < numSamples; ++i) {
        output.outputChannels[0][i] = (left[i] + right[i]) * 0.5f;
    }
}

void SignalProcessor::processMidSide(const float* left, const float* right,
                                    size_t numSamples, ProcessedOutput& output) {
    // Get precision setting
    ProcessingConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = currentConfig;
    }

    if (config.useDoublePrec) {
        // Double precision M/S conversion for accuracy
        for (size_t i = 0; i < numSamples; ++i) {
            double L = static_cast<double>(left[i]);
            double R = static_cast<double>(right[i]);

            // M = (L + R) / 2, S = (L - R) / 2
            double mid = (L + R) * 0.5;
            double side = (L - R) * 0.5;

            output.outputChannels[0][i] = static_cast<float>(mid);
            output.outputChannels[1][i] = static_cast<float>(side);
        }
    } else {
        // Single precision for performance
        for (size_t i = 0; i < numSamples; ++i) {
            float mid = (left[i] + right[i]) * 0.5f;
            float side = (left[i] - right[i]) * 0.5f;

            output.outputChannels[0][i] = mid;
            output.outputChannels[1][i] = side;
        }
    }
}

void SignalProcessor::processLeftOnly(const float* left, const float* right,
                                     size_t numSamples, ProcessedOutput& output) {
    // Copy left channel only
    std::copy(left, left + numSamples, output.outputChannels[0]);
    (void)right; // Suppress unused parameter warning
}

void SignalProcessor::processRightOnly(const float* left, const float* right,
                                      size_t numSamples, ProcessedOutput& output) {
    // Copy right channel only
    std::copy(right, right + numSamples, output.outputChannels[0]);
    (void)left; // Suppress unused parameter warning
}

void SignalProcessor::processDifference(const float* left, const float* right,
                                       size_t numSamples, ProcessedOutput& output) {
    // Difference: L - R
    for (size_t i = 0; i < numSamples; ++i) {
        output.outputChannels[0][i] = left[i] - right[i];
    }
}

void SignalProcessor::updateCorrelation(const float* left, const float* right,
                                       size_t numSamples, CorrelationMetrics& metrics) {
    // Check if we need to reset correlation window
    ProcessingConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = currentConfig;
    }

    if (correlationSampleCount >= config.correlationWindowSize) {
        // Calculate final metrics and reset
        correlationState.calculateFinalMetrics();
        metrics = correlationState;
        correlationState.reset();
        correlationSampleCount = 0;
    }

    // Accumulate correlation data
    for (size_t i = 0; i < numSamples; ++i) {
        double L = static_cast<double>(left[i]);
        double R = static_cast<double>(right[i]);

        correlationState.sumL += L;
        correlationState.sumR += R;
        correlationState.sumLL += L * L;
        correlationState.sumRR += R * R;
        correlationState.sumLR += L * R;
        correlationState.sampleCount++;
    }

    correlationSampleCount += numSamples;
}

bool SignalProcessor::shouldUpdateCorrelation() const {
    // For now, update correlation every block
    // Future: implement rate limiting based on correlationUpdateRate
    return true;
}

} // namespace audio
