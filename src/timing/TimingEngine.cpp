/**
 * @file TimingEngine.cpp
 * @brief Implementation of timing and synchronization engine for Oscil plugin
 *
 * This file contains the implementation of the TimingEngine class that provides
 * precise timing control for oscilloscope capture with multiple synchronization
 * modes including DAW sync, trigger detection, and musical timing.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include "TimingEngine.h"
#include <algorithm>
#include <cmath>

namespace oscil {
namespace timing {

// === Constants ===

namespace {
    constexpr double DEFAULT_BPM = 120.0;
    constexpr double MIN_BPM = 60.0;
    constexpr double MAX_BPM = 300.0;
    constexpr double DEFAULT_TIME_INTERVAL_MS = 100.0;
    constexpr float DEFAULT_TRIGGER_THRESHOLD = 0.5F;
    constexpr float DEFAULT_TRIGGER_HYSTERESIS = 0.1F;
    constexpr int DEFAULT_TRIGGER_HOLDOFF = 512;
    constexpr int DEFAULT_SLOPE_WINDOW = 8;
    constexpr size_t TRIGGER_HISTORY_SIZE = 256;
    constexpr double PROCESSING_TIME_SMOOTHING = 0.95; // Exponential smoothing factor
}

// === TimingEngine Implementation ===

TimingEngine::TimingEngine()
    : currentMode(TimingMode::FreeRunning)
    , isActive(false)
    , sampleRate(44100.0)
    , samplesPerBlock(512)
    , isPrepared(false)
    , samplesProcessed(0)
    , captureEvents(0)
    , missedTriggers(0)
    , triggerHistoryIndex(0)
    , lastTriggerSample(0)
    , lastSampleValue(0.0F)
    , lastBPM(DEFAULT_BPM)
    , samplesPerBeat(22050.0) // 120 BPM at 44.1kHz
    , lastBeatSample(0)
    , lastTimeBasedCapture(0)
    , timeBasedInterval(0.0)
{
    // Initialize default configurations
    triggerConfig.type = TriggerType::Level;
    triggerConfig.edge = TriggerEdge::Rising;
    triggerConfig.threshold = DEFAULT_TRIGGER_THRESHOLD;
    triggerConfig.hysteresis = DEFAULT_TRIGGER_HYSTERESIS;
    triggerConfig.holdOffSamples = DEFAULT_TRIGGER_HOLDOFF;
    triggerConfig.slopeWindowSamples = DEFAULT_SLOPE_WINDOW;
    triggerConfig.enabled = true;

    musicalConfig.beatDivision = 4; // Quarter note
    musicalConfig.barLength = 4;    // 4/4 time
    musicalConfig.snapToBeats = true;
    musicalConfig.followTempoChanges = true;
    musicalConfig.customBPM = DEFAULT_BPM;

    timeBasedConfig.intervalMs = DEFAULT_TIME_INTERVAL_MS;
    timeBasedConfig.driftCompensation = true;
    timeBasedConfig.adaptToSampleRate = true;

    // Initialize trigger history buffer
    triggerHistory.fill(0.0F);

    // Reset performance statistics
    performanceStats.reset();
}

TimingEngine::~TimingEngine() {
    // Ensure resources are released
    if (isPrepared) {
        releaseResources();
    }
}

void TimingEngine::prepareToPlay(double newSampleRate, int newSamplesPerBlock) {
    if (newSampleRate <= 0.0 || newSamplesPerBlock <= 0) {
        jassertfalse; // Invalid parameters
        return;
    }

    // Update audio processing parameters
    sampleRate = newSampleRate;
    samplesPerBlock = newSamplesPerBlock;

    // Calculate derived timing values
    samplesPerBeat = bpmToSamplesPerBeat(lastBPM, sampleRate);
    timeBasedInterval = timeToSamples(timeBasedConfig.intervalMs, sampleRate);

    // Reset internal state
    samplesProcessed = 0;
    captureEvents = 0;
    missedTriggers = 0;
    triggerHistoryIndex = 0;
    lastTriggerSample = 0;
    lastBeatSample = 0;
    lastTimeBasedCapture = 0;
    lastSampleValue = 0.0F;

    // Clear trigger history
    triggerHistory.fill(0.0F);

    // Reset performance statistics
    performanceStats.reset();

    // Mark as prepared
    isPrepared = true;
    isActive.store(true);
}

void TimingEngine::releaseResources() {
    isPrepared = false;
    isActive.store(false);

    // Reset all state
    samplesProcessed = 0;
    captureEvents = 0;
    missedTriggers = 0;
    triggerHistoryIndex = 0;
    lastTriggerSample = 0;
    lastBeatSample = 0;
    lastTimeBasedCapture = 0;
    lastSampleValue = 0.0F;

    // Clear trigger history
    triggerHistory.fill(0.0F);
}

// === Timing Mode Management ===

void TimingEngine::setTimingMode(TimingMode mode) {
    if (!isValidTimingMode(static_cast<int>(mode))) {
        jassertfalse; // Invalid timing mode
        return;
    }

    // Atomic mode change - no locking needed
    currentMode.store(mode);
    performanceStats.modeChanges.fetch_add(1);

    // Reset timing state when mode changes
    if (isPrepared) {
        lastTriggerSample = samplesProcessed;
        lastBeatSample = samplesProcessed;
        lastTimeBasedCapture = samplesProcessed;
    }
}

TimingMode TimingEngine::getTimingMode() const {
    return currentMode.load();
}

// === Configuration Management ===

bool TimingEngine::setTriggerConfig(const TriggerConfig& config) {
    if (!config.isValid()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(configMutex);
    triggerConfig = config;
    return true;
}

TriggerConfig TimingEngine::getTriggerConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return triggerConfig;
}

bool TimingEngine::setMusicalConfig(const MusicalConfig& config) {
    if (!config.isValid()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(configMutex);
    musicalConfig = config;

    // Update derived values if prepared
    if (isPrepared) {
        double bpm = config.followTempoChanges ? lastBPM : config.customBPM;
        samplesPerBeat = bpmToSamplesPerBeat(bpm, sampleRate) / config.beatDivision;
    }

    return true;
}

MusicalConfig TimingEngine::getMusicalConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return musicalConfig;
}

bool TimingEngine::setTimeBasedConfig(const TimeBasedConfig& config) {
    if (!config.isValid()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(configMutex);
    timeBasedConfig = config;

    // Update derived values if prepared
    if (isPrepared) {
        timeBasedInterval = timeToSamples(config.intervalMs, sampleRate);
    }

    return true;
}

TimeBasedConfig TimingEngine::getTimeBasedConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return timeBasedConfig;
}

// === Core Timing Operations ===

bool TimingEngine::shouldCaptureAtCurrentTime(juce::AudioPlayHead* playHead,
                                               const float* const* audioData,
                                               int numSamples) {
    if (!isPrepared || !isActive.load()) {
        return false;
    }

    // Start timing measurement
    auto startTime = juce::Time::getHighResolutionTicks();

    bool shouldCapture = false;
    TimingMode mode = currentMode.load();

    switch (mode) {
        case TimingMode::FreeRunning:
            shouldCapture = processFreeRunningMode();
            break;

        case TimingMode::HostSync:
            shouldCapture = processHostSyncMode(playHead, numSamples);
            break;

        case TimingMode::TimeBased:
            shouldCapture = processTimeBasedMode(numSamples);
            break;

        case TimingMode::Musical:
            shouldCapture = processMusicalMode(playHead, numSamples);
            break;

        case TimingMode::Trigger:
            shouldCapture = processTriggerMode(audioData, numSamples);
            break;

        default:
            jassertfalse; // Unknown timing mode
            break;
    }

    // Update performance statistics
    auto endTime = juce::Time::getHighResolutionTicks();
    double processingTimeMs = juce::Time::highResolutionTicksToSeconds(endTime - startTime) * 1000.0;
    updatePerformanceStats(processingTimeMs);

    performanceStats.timingCalculations.fetch_add(1);

    if (shouldCapture) {
        captureEvents++;
    }

    return shouldCapture;
}

void TimingEngine::processTimingBlock(juce::AudioPlayHead* playHead, int numSamples) {
    if (!isPrepared || !isActive.load()) {
        return;
    }

    performanceStats.processBlockCalls.fetch_add(1);

    // Update BPM from play head if available
    updateBPMFromPlayHead(playHead);

    // Update samples processed
    samplesProcessed += static_cast<uint64_t>(numSamples);
}

void TimingEngine::forceTrigger() {
    if (isPrepared && isActive.load()) {
        captureEvents++;
        lastTriggerSample = samplesProcessed;
    }
}

// === State and Statistics ===

TimingState TimingEngine::getTimingState() const {
    TimingState state;
    state.currentMode = currentMode.load();
    state.isActive = isActive.load();
    state.samplesProcessed = samplesProcessed;
    state.captureEvents = captureEvents;
    state.missedTriggers = missedTriggers;
    state.currentBPM = lastBPM;
    state.sampleRate = sampleRate;

    // Calculate timing accuracy statistics
    if (captureEvents > 0) {
        // These would be calculated from actual timing measurements
        state.averageTimingError = 0.0; // Placeholder
        state.maxTimingError = 0.0;     // Placeholder
        state.accuracyMeasurements = captureEvents;
    }

    return state;
}

TimingPerformanceStats TimingEngine::getPerformanceStats() const {
    return performanceStats;
}

void TimingEngine::resetStatistics() {
    captureEvents = 0;
    missedTriggers = 0;
    performanceStats.reset();
}

// === Utility Functions ===

double TimingEngine::bpmToSamplesPerBeat(double bpm, double sampleRate) {
    if (bpm <= 0.0 || sampleRate <= 0.0) {
        return 22050.0; // Default for 120 BPM at 44.1kHz
    }
    return (60.0 * sampleRate) / bpm;
}

int TimingEngine::timeToSamples(double timeMs, double sampleRate) {
    if (timeMs <= 0.0 || sampleRate <= 0.0) {
        return 0;
    }
    return static_cast<int>((timeMs / 1000.0) * sampleRate);
}

bool TimingEngine::isValidTimingMode(int mode) {
    return mode >= static_cast<int>(TimingMode::FreeRunning) &&
           mode <= static_cast<int>(TimingMode::Trigger);
}

// === Private Implementation Methods ===

bool TimingEngine::processFreeRunningMode() {
    // Free running mode captures continuously
    // Could implement a minimal interval to prevent excessive captures
    static constexpr uint64_t MIN_FREE_RUNNING_INTERVAL = 1024; // ~23ms at 44.1kHz

    if (samplesProcessed - lastTriggerSample >= MIN_FREE_RUNNING_INTERVAL) {
        lastTriggerSample = samplesProcessed;
        return true;
    }

    return false;
}

bool TimingEngine::processHostSyncMode(juce::AudioPlayHead* playHead, int numSamples) {
    if (!playHead) {
        // No play head available, fall back to free running
        return processFreeRunningMode();
    }

    // Get position info from DAW
    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue()) {
        return false;
    }

    auto position = *positionInfo;

    // Check if DAW is playing
    bool isPlaying = position.getIsPlaying();
    if (!isPlaying) {
        return false; // Don't capture when not playing
    }

    // Get current position in PPQ (Pulses Per Quarter note)
    auto ppqPosition = position.getPpqPosition();
    if (!ppqPosition.hasValue()) {
        return false;
    }

    // Calculate if we should trigger based on musical position
    // This is a simplified implementation - could be more sophisticated
    double currentPpq = *ppqPosition;
    double ppqPerCapture = 1.0; // Capture every quarter note

    // Check if we've crossed a capture boundary
    double lastPpq = currentPpq - (static_cast<double>(numSamples) / samplesPerBeat);

    if (std::floor(currentPpq / ppqPerCapture) > std::floor(lastPpq / ppqPerCapture)) {
        return true;
    }

    return false;
}

bool TimingEngine::processTimeBasedMode(int /* numSamples */) {
    // Check if enough time has elapsed since last capture
    if (samplesProcessed - lastTimeBasedCapture >= static_cast<uint64_t>(timeBasedInterval)) {
        lastTimeBasedCapture = samplesProcessed;
        return true;
    }

    return false;
}

bool TimingEngine::processMusicalMode(juce::AudioPlayHead* playHead, int /* numSamples */) {
    // Update BPM if following tempo changes
    MusicalConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = musicalConfig;
    }

    if (config.followTempoChanges && playHead) {
        updateBPMFromPlayHead(playHead);
    } else {
        lastBPM = config.customBPM;
    }

    // Calculate samples per beat division
    double currentSamplesPerBeat = bpmToSamplesPerBeat(lastBPM, sampleRate) / config.beatDivision;

    // Check if we've reached the next beat
    if (samplesProcessed - lastBeatSample >= static_cast<uint64_t>(currentSamplesPerBeat)) {
        lastBeatSample = samplesProcessed;
        return true;
    }

    return false;
}

bool TimingEngine::processTriggerMode(const float* const* audioData, int numSamples) {
    if (!audioData || numSamples <= 0) {
        return false;
    }

    TriggerConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = triggerConfig;
    }

    if (!config.enabled) {
        return false;
    }

    // Check hold-off period
    if (samplesProcessed - lastTriggerSample < static_cast<uint64_t>(config.holdOffSamples)) {
        return false;
    }

    // Process samples for trigger detection
    // Use first channel for trigger detection
    const float* samples = audioData[0];
    bool triggered = false;

    for (int i = 0; i < numSamples && !triggered; ++i) {
        float sample = samples[i];

        switch (config.type) {
            case TriggerType::Level:
                triggered = detectLevelTrigger(sample);
                break;

            case TriggerType::Edge:
                triggered = detectEdgeTrigger(sample);
                break;

            case TriggerType::Slope:
                // For slope detection, process all samples at once
                triggered = detectSlopeTrigger(samples, numSamples);
                break;
        }

        // Update trigger history
        triggerHistory[static_cast<size_t>(triggerHistoryIndex)] = sample;
        triggerHistoryIndex = (triggerHistoryIndex + 1) % static_cast<int>(TRIGGER_HISTORY_SIZE);
        // Note: lastSampleValue is updated within the individual trigger detection functions

        if (triggered) {
            lastTriggerSample = samplesProcessed + static_cast<uint64_t>(i);
            performanceStats.triggerDetections.fetch_add(1);
            break;
        }
    }

    return triggered;
}

bool TimingEngine::detectLevelTrigger(float sample) {
    TriggerConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = triggerConfig;
    }

    bool triggered = false;

    switch (config.edge) {
        case TriggerEdge::Rising:
            // Simple rising edge: cross from below threshold to above threshold
            triggered = (lastSampleValue < config.threshold) &&
                       (sample >= config.threshold);
            break;

        case TriggerEdge::Falling:
            // Simple falling edge: cross from above threshold to below threshold
            triggered = (lastSampleValue > config.threshold) &&
                       (sample <= config.threshold);
            break;

        case TriggerEdge::Both:
            // Both directions
            triggered = ((lastSampleValue < config.threshold) &&
                        (sample >= config.threshold)) ||
                       ((lastSampleValue > config.threshold) &&
                        (sample <= config.threshold));
            break;
    }

    // Update last sample value for next comparison
    lastSampleValue = sample;

    return triggered;
}

bool TimingEngine::detectEdgeTrigger(float sample) {
    // Edge detection using derivative
    float derivative = sample - lastSampleValue;

    TriggerConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = triggerConfig;
    }

    bool triggered = false;

    switch (config.edge) {
        case TriggerEdge::Rising:
            triggered = derivative > config.threshold;
            break;

        case TriggerEdge::Falling:
            triggered = derivative < -config.threshold;
            break;

        case TriggerEdge::Both:
            triggered = std::abs(derivative) > config.threshold;
            break;
    }

    // Update last sample value for next comparison
    lastSampleValue = sample;

    return triggered;
}

bool TimingEngine::detectSlopeTrigger(const float* audioData, int numSamples) {
    TriggerConfig config;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        config = triggerConfig;
    }

    if (numSamples < config.slopeWindowSamples) {
        return false;
    }

    // Calculate slope over the window
    int windowSize = std::min(config.slopeWindowSamples, numSamples);
    float sumX = 0.0F, sumY = 0.0F, sumXY = 0.0F, sumX2 = 0.0F;

    for (int i = 0; i < windowSize; ++i) {
        float x = static_cast<float>(i);
        float y = audioData[i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    // Calculate slope using least squares regression
    float n = static_cast<float>(windowSize);
    float slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);

    // Check if slope exceeds threshold
    bool triggered = false;

    switch (config.edge) {
        case TriggerEdge::Rising:
            triggered = slope > config.threshold;
            break;

        case TriggerEdge::Falling:
            triggered = slope < -config.threshold;
            break;

        case TriggerEdge::Both:
            triggered = std::abs(slope) > config.threshold;
            break;
    }

    return triggered;
}

void TimingEngine::updateBPMFromPlayHead(juce::AudioPlayHead* playHead) {
    if (!playHead) {
        return;
    }

    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue()) {
        return;
    }

    auto bpm = positionInfo->getBpm();
    if (bpm.hasValue()) {
        double newBPM = *bpm;
        if (newBPM >= MIN_BPM && newBPM <= MAX_BPM) {
            lastBPM = newBPM;
            samplesPerBeat = bpmToSamplesPerBeat(lastBPM, sampleRate);
        }
    }
}

void TimingEngine::updatePerformanceStats(double processingTimeMs) {
    // Update maximum processing time
    double currentMax = performanceStats.maxProcessingTime.load();
    if (processingTimeMs > currentMax) {
        performanceStats.maxProcessingTime.store(processingTimeMs);
    }

    // Update average processing time using exponential smoothing
    double currentAvg = performanceStats.averageProcessingTime.load();
    double newAvg = currentAvg * PROCESSING_TIME_SMOOTHING +
                    processingTimeMs * (1.0 - PROCESSING_TIME_SMOOTHING);
    performanceStats.averageProcessingTime.store(newAvg);
}

} // namespace timing
} // namespace oscil
