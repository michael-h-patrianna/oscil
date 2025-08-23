/**
 * @file AdvancedDecimationProcessor.cpp
 * @brief Implementation of multi-level decimation processor for 64-track performance
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#include "AdvancedDecimationProcessor.h"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace oscil::render {

//==============================================================================
// Constants

static constexpr size_t MIN_SAMPLES_FOR_DECIMATION = 64;
static constexpr double CPU_USAGE_ESTIMATE_FACTOR = 0.25; // Estimated CPU % per ms frame time

//==============================================================================
// MemoryPool Implementation

void AdvancedDecimationProcessor::MemoryPool::allocateForTracks(size_t numTracks, size_t bufferSize) {
    clear();

    const size_t actualTracks = std::min(numTracks, MAX_TRACKS);

    for (size_t i = 0; i < actualTracks; ++i) {
        trackBuffers[i].reserve(bufferSize * 2); // Reserve space for min/max pairs
        totalAllocatedBytes.store(totalAllocatedBytes.load() + bufferSize * 2 * sizeof(float));
    }

    // Allocate scratch buffer for intermediate calculations
    scratchBuffer.reserve(bufferSize * 4);
    totalAllocatedBytes.store(totalAllocatedBytes.load() + bufferSize * 4 * sizeof(float));
}

void AdvancedDecimationProcessor::MemoryPool::clear() {
    for (auto& buffer : trackBuffers) {
        buffer.clear();
        buffer.shrink_to_fit();
    }
    scratchBuffer.clear();
    scratchBuffer.shrink_to_fit();
    totalAllocatedBytes.store(0);
}

//==============================================================================
// AdvancedDecimationProcessor Implementation

AdvancedDecimationProcessor::AdvancedDecimationProcessor()
    : lastFrameTime(std::chrono::steady_clock::now()) {
    // Initialize with default settings
    reset();
}

AdvancedDecimationProcessor::~AdvancedDecimationProcessor() {
    memoryPool.clear();
}

void AdvancedDecimationProcessor::prepareForTracks(size_t numTracks, int displayWidth, double sampleRate) {
    std::lock_guard<std::mutex> lock(stateMutex);

    if (numTracks > MAX_TRACKS) {
        numTracks = MAX_TRACKS;
    }

    const size_t bufferSize = static_cast<size_t>(sampleRate * 0.2); // 200ms buffer
    memoryPool.allocateForTracks(numTracks, bufferSize);

    // Initialize pyramid structures
    for (size_t i = 0; i < numTracks; ++i) {
        auto& pyramid = trackPyramids[i];
        pyramid.validLevels = 0;

        // Pre-allocate pyramid levels
        size_t levelSize = bufferSize;
        for (size_t level = 0; level < MAX_PYRAMID_LEVELS && levelSize > MIN_SAMPLES_FOR_DECIMATION; ++level) {
            pyramid.levels[level].data.reserve(levelSize);
            pyramid.levels[level].compressionRatio = static_cast<float>(std::pow(2.0, level + 1));
            levelSize /= 2;
        }
    }

    preparedTrackCount.store(numTracks);
    preparedDisplayWidth.store(displayWidth);
    preparedSampleRate.store(sampleRate);
}

AdvancedDecimationProcessor::MultiTrackDecimationResult
AdvancedDecimationProcessor::processMultipleTracks(
    const TrackDecimationInput* inputs,
    size_t inputCount,
    int targetPixels,
    double frameTimeBudgetMs) {

    const auto frameStartTime = std::chrono::steady_clock::now();

    MultiTrackDecimationResult result;
    result.trackResults.reserve(inputCount);

    size_t visibleTracks = 0;

    // Calculate time budget per visible track
    for (size_t i = 0; i < inputCount; ++i) {
        if (inputs[i].isVisible) {
            visibleTracks++;
        }
    }

    if (visibleTracks == 0) {
        return result;
    }

    const double timePerTrack = frameTimeBudgetMs / static_cast<double>(visibleTracks);

    // Determine quality mode based on load
    QualityMode effectiveQuality = currentQuality.load();
    if (autoQualityEnabled.load() && visibleTracks > 16) {
        effectiveQuality = calculateAdaptiveQuality(frameTimeBudgetMs, visibleTracks);
    }

    // Process each track
    for (size_t i = 0; i < inputCount; ++i) {
        const auto& input = inputs[i];

        if (!input.isVisible) {
            // Skip invisible tracks but add placeholder result
            TrackDecimationResult trackResult;
            trackResult.trackIndex = input.trackIndex;
            trackResult.samples = input.samples;
            trackResult.sampleCount = input.sampleCount;
            trackResult.wasDecimated = false;
            result.trackResults.push_back(trackResult);
            continue;
        }

        auto trackResult = processSingleTrack(input, targetPixels, effectiveQuality, timePerTrack);
        result.trackResults.push_back(trackResult);
    }

    // Calculate overall results
    const auto frameEndTime = std::chrono::steady_clock::now();
    const double frameTimeMs = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();

    result.visibleTrackCount = visibleTracks;
    result.totalProcessingTimeMs = frameTimeMs;
    result.overallQuality = effectiveQuality;
    result.memoryUsageBytes = memoryPool.totalAllocatedBytes.load();

    // OpenGL recommendation
    if (openGLHintsEnabled.load()) {
        const auto metrics = getPerformanceMetrics();
        result.shouldEnableOpenGL = shouldRecommendOpenGL(metrics);
    }

    // Update performance metrics
    updatePerformanceMetrics(result);

    totalFramesProcessed.fetch_add(1);
    totalTracksProcessed.fetch_add(visibleTracks);

    return result;
}

AdvancedDecimationProcessor::TrackDecimationResult
AdvancedDecimationProcessor::processSingleTrack(
    const TrackDecimationInput& input,
    int targetPixels,
    QualityMode quality,
    double /* timeBudgetMs */) {

    const auto startTime = std::chrono::steady_clock::now();

    TrackDecimationResult result;
    result.trackIndex = input.trackIndex;
    result.appliedQuality = quality;

    // Check if decimation is needed
    const float densityRatio = static_cast<float>(input.sampleCount) / static_cast<float>(targetPixels);
    const float threshold = (quality == QualityMode::Highest) ? 2.0F :
                           (quality == QualityMode::High) ? 1.5F :
                           (quality == QualityMode::Balanced) ? 1.2F :
                           (quality == QualityMode::Performance) ? 1.0F : 0.8F;

    if (densityRatio <= threshold || input.sampleCount < MIN_SAMPLES_FOR_DECIMATION) {
        // No decimation needed
        result.samples = input.samples;
        result.sampleCount = input.sampleCount;
        result.wasDecimated = false;
    } else {
        // Apply decimation
        const size_t trackIndex = static_cast<size_t>(input.trackIndex) % MAX_TRACKS;
        auto& buffer = memoryPool.trackBuffers[trackIndex];

        // Ensure buffer has enough space
        const size_t requiredSize = static_cast<size_t>(targetPixels * 2); // Min/max pairs
        if (buffer.capacity() < requiredSize) {
            buffer.reserve(requiredSize);
        }
        buffer.resize(requiredSize);

        // Perform decimation
        const size_t outputSamples = performProgressiveDecimation(
            input.samples, input.sampleCount,
            buffer.data(), targetPixels, quality);

        result.samples = buffer.data();
        result.sampleCount = outputSamples;
        result.wasDecimated = true;
    }

    const auto endTime = std::chrono::steady_clock::now();
    result.processingTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return result;
}

size_t AdvancedDecimationProcessor::performProgressiveDecimation(
    const float* input, size_t inputCount,
    float* output, int targetPixels,
    QualityMode /* quality */) {

    if (targetPixels <= 0 || inputCount == 0) {
        return 0;
    }

    const size_t samplesPerPixel = inputCount / static_cast<size_t>(targetPixels);
    if (samplesPerPixel <= 1) {
        // Copy directly if no decimation needed
        const size_t copyCount = std::min(inputCount, static_cast<size_t>(targetPixels));
        std::copy(input, input + copyCount, output);
        return copyCount;
    }

    size_t outputIndex = 0;
    const size_t maxOutput = static_cast<size_t>(targetPixels * 2); // Space for min/max pairs

    // Progressive min/max decimation
    for (int pixel = 0; pixel < targetPixels && outputIndex < maxOutput; ++pixel) {
        const size_t startSample = (static_cast<size_t>(pixel) * inputCount) / static_cast<size_t>(targetPixels);
        const size_t endSample = std::min(
            ((static_cast<size_t>(pixel) + 1) * inputCount) / static_cast<size_t>(targetPixels),
            inputCount);

        if (startSample >= endSample) continue;

        // Find min and max in this range
        float minVal = input[startSample];
        float maxVal = input[startSample];

        for (size_t i = startSample + 1; i < endSample; ++i) {
            const float sample = input[i];
            if (sample < minVal) minVal = sample;
            if (sample > maxVal) maxVal = sample;
        }

        // Store min/max pair
        if (outputIndex < maxOutput) {
            output[outputIndex++] = minVal;
        }
        if (outputIndex < maxOutput) {
            output[outputIndex++] = maxVal;
        }
    }

    return outputIndex;
}

void AdvancedDecimationProcessor::setQualityMode(QualityMode mode) {
    currentQuality.store(mode);
    autoQualityEnabled.store(mode == QualityMode::Highest);
}

AdvancedDecimationProcessor::PerformanceMetrics
AdvancedDecimationProcessor::getPerformanceMetrics() const {
    PerformanceMetrics metrics;

    metrics.averageFrameTimeMs = averageFrameTime.load();
    metrics.peakFrameTimeMs = peakFrameTime.load();
    metrics.frameRate = currentFrameRate.load();
    metrics.framesProcessed = totalFramesProcessed.load();
    metrics.tracksProcessed = totalTracksProcessed.load();
    metrics.cpuUsagePercent = metrics.averageFrameTimeMs * CPU_USAGE_ESTIMATE_FACTOR;
    metrics.memoryPoolUsageBytes = memoryPool.totalAllocatedBytes.load();
    metrics.currentQuality = currentQuality.load();

    return metrics;
}

void AdvancedDecimationProcessor::reset() {
    std::lock_guard<std::mutex> lock(stateMutex);

    memoryPool.clear();

    // Reset pyramid caches
    for (auto& pyramid : trackPyramids) {
        pyramid.validLevels = 0;
        for (auto& level : pyramid.levels) {
            level.data.clear();
            level.sampleCount = 0;
        }
    }

    // Reset performance metrics
    averageFrameTime.store(0.0);
    peakFrameTime.store(0.0);
    totalFramesProcessed.store(0);
    totalTracksProcessed.store(0);
    currentFrameRate.store(60.0);

    // Reset quality settings
    currentQuality.store(QualityMode::Highest);
    autoQualityEnabled.store(true);

    lastFrameTime = std::chrono::steady_clock::now();
}

bool AdvancedDecimationProcessor::meetsPerformanceTargets() const {
    const auto metrics = getPerformanceMetrics();

    // Check PRD requirements for 64-track operation
    const bool frameRateOk = metrics.frameRate >= 30.0; // Minimum 30fps
    const bool cpuUsageOk = metrics.cpuUsagePercent <= 16.0; // <16% CPU
    const bool memoryOk = metrics.memoryPoolUsageBytes <= (640 * 1024 * 1024); // <640MB
    const bool frameTimeOk = metrics.averageFrameTimeMs <= 33.33; // 30fps = 33.33ms max

    return frameRateOk && cpuUsageOk && memoryOk && frameTimeOk;
}

void AdvancedDecimationProcessor::updatePerformanceMetrics(const MultiTrackDecimationResult& result) const {
    const double frameTime = result.totalProcessingTimeMs;

    // Update frame time statistics
    const double currentAvg = averageFrameTime.load();
    const double newAvg = (currentAvg == 0.0) ? frameTime : (currentAvg * 0.9 + frameTime * 0.1);
    averageFrameTime.store(newAvg);

    const double currentPeak = peakFrameTime.load();
    if (frameTime > currentPeak) {
        peakFrameTime.store(frameTime);
    }

    // Update frame rate
    const auto now = std::chrono::steady_clock::now();
    const auto timeSinceLastFrame = std::chrono::duration<double>(now - lastFrameTime).count();
    if (timeSinceLastFrame > 0.0) {
        const double instantFps = 1.0 / timeSinceLastFrame;
        const double currentFps = currentFrameRate.load();
        const double newFps = (currentFps == 0.0) ? instantFps : (currentFps * 0.95 + instantFps * 0.05);
        currentFrameRate.store(newFps);
    }
    lastFrameTime = now;
}

bool AdvancedDecimationProcessor::shouldRecommendOpenGL(const PerformanceMetrics& metrics) const {
    // Recommend OpenGL when:
    // 1. Frame rate is below target (< 45fps)
    // 2. CPU usage is high (> 12%)
    // 3. Many tracks are active (> 32)

    const bool lowFrameRate = metrics.frameRate < 45.0;
    const bool highCpuUsage = metrics.cpuUsagePercent > 12.0;
    const bool manyTracks = preparedTrackCount.load() > 32;

    return lowFrameRate || highCpuUsage || manyTracks;
}

AdvancedDecimationProcessor::QualityMode
AdvancedDecimationProcessor::calculateAdaptiveQuality(double frameTimeMs, size_t visibleTracks) const {
    const double targetFrameTime = 16.67; // 60fps target
    const double acceptableFrameTime = 33.33; // 30fps minimum

    if (frameTimeMs <= targetFrameTime && visibleTracks <= 16) {
        return QualityMode::Highest;
    } else if (frameTimeMs <= targetFrameTime * 1.2 && visibleTracks <= 32) {
        return QualityMode::High;
    } else if (frameTimeMs <= acceptableFrameTime && visibleTracks <= 48) {
        return QualityMode::Balanced;
    } else if (frameTimeMs <= acceptableFrameTime * 1.5) {
        return QualityMode::Performance;
    } else {
        return QualityMode::Maximum;
    }
}

} // namespace oscil::render
