#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>
#include <memory>
#include <cstring>

namespace audio {

/**
 * Data packet containing audio samples and metadata for UI thread consumption.
 * Fixed-size structure to avoid dynamic allocation during audio processing.
 */
struct AudioDataSnapshot {
    static constexpr size_t MAX_SAMPLES = 1024;  // Maximum samples per snapshot
    static constexpr size_t MAX_CHANNELS = 64;   // Maximum channels supported
    static constexpr double DEFAULT_SAMPLE_RATE = 44100.0;

    size_t numChannels = 0;
    size_t numSamples = 0;
    uint64_t timestamp = 0;          // Frame counter for timing
    double sampleRate = DEFAULT_SAMPLE_RATE;

    // Sample data organized as [channel][sample]
    float samples[MAX_CHANNELS][MAX_SAMPLES];

    AudioDataSnapshot() {
        clear();
    }

    void clear() {
        numChannels = 0;
        numSamples = 0;
        timestamp = 0;
        sampleRate = DEFAULT_SAMPLE_RATE;
        std::memset(samples, 0, sizeof(samples));
    }

    void copyFrom(const float* const* channelData, size_t channels, size_t sampleCount,
                  uint64_t frameTimestamp, double sampleRateValue) {
        numChannels = std::min(channels, MAX_CHANNELS);
        numSamples = std::min(sampleCount, MAX_SAMPLES);
        timestamp = frameTimestamp;
        sampleRate = sampleRateValue;

        for (size_t ch = 0; ch < numChannels; ++ch) {
            if (channelData[ch] != nullptr) {
                std::memcpy(samples[ch], channelData[ch], numSamples * sizeof(float));
            }
        }
    }
};

/**
 * Lock-free communication bridge between audio thread and UI thread.
 * Implements double-buffering with atomic swap for thread-safe data transfer.
 */
class WaveformDataBridge {
public:
    WaveformDataBridge();
    ~WaveformDataBridge() = default;

    /**
     * Called from audio thread to push new audio data.
     * Non-blocking operation that never allocates memory.
     */
    void pushAudioData(const float* const* channelData, size_t numChannels,
                       size_t numSamples, double sampleRate);

    /**
     * Called from UI thread to get latest audio data.
     * Returns true if new data is available since last call.
     */
    bool pullLatestData(AudioDataSnapshot& outSnapshot);

    /**
     * Get performance statistics for monitoring.
     */
    uint64_t getTotalFramesPushed() const { return framesPushed.load(std::memory_order_acquire); }
    uint64_t getTotalFramesPulled() const { return framesPulled.load(std::memory_order_acquire); }

    /**
     * Reset statistics (for testing/debugging).
     */
    void resetStats();

private:
    // Double-buffered snapshots for lock-free communication
    std::unique_ptr<AudioDataSnapshot> writeSnapshot;
    std::unique_ptr<AudioDataSnapshot> readSnapshot;

    // Atomic flag to coordinate buffer swapping
    std::atomic<bool> dataReady{false};

    // Performance counters
    std::atomic<uint64_t> framesPushed{0};
    std::atomic<uint64_t> framesPulled{0};
    std::atomic<uint64_t> frameCounter{0};

    // Cached sample rate for timestamp calculations
    double currentSampleRate = AudioDataSnapshot::DEFAULT_SAMPLE_RATE;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDataBridge)
};

} // namespace audio
