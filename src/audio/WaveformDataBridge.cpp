#include "WaveformDataBridge.h"
#include <algorithm>

namespace audio {

WaveformDataBridge::WaveformDataBridge() {
    // Pre-allocate snapshots to avoid runtime allocation
    writeSnapshot = std::make_unique<AudioDataSnapshot>();
    readSnapshot = std::make_unique<AudioDataSnapshot>();
}

void WaveformDataBridge::pushAudioData(const float* const* channelData, size_t numChannels,
                                       size_t numSamples, double sampleRate) {
    if (!writeSnapshot || !channelData) {
        return;
    }

    // Update frame counter and sample rate
    frameCounter.fetch_add(1, std::memory_order_relaxed);
    currentSampleRate = sampleRate;

    // Copy data to write snapshot
    writeSnapshot->copyFrom(channelData, numChannels, numSamples,
                           frameCounter.load(std::memory_order_relaxed), sampleRate);

    // Mark data as ready and swap buffers (atomic operation)
    // This will overwrite any previous unread data
    dataReady.exchange(true, std::memory_order_release);

    // Always swap buffers to make the new data available
    std::swap(writeSnapshot, readSnapshot);
    framesPushed.fetch_add(1, std::memory_order_relaxed);
}

bool WaveformDataBridge::pullLatestData(AudioDataSnapshot& outSnapshot) {
    // Check if new data is available
    bool expected = true;
    if (dataReady.compare_exchange_weak(expected, false, std::memory_order_acquire)) {
        // New data available - copy from read snapshot
        if (readSnapshot) {
            outSnapshot = *readSnapshot;
            framesPulled.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    return false; // No new data
}

void WaveformDataBridge::resetStats() {
    framesPushed.store(0, std::memory_order_relaxed);
    framesPulled.store(0, std::memory_order_relaxed);
    frameCounter.store(0, std::memory_order_relaxed);
}

} // namespace audio
