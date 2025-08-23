#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <memory>
#include <atomic>
#include <map>
#include <string>

#include "../dsp/RingBuffer.h"
#include "WaveformDataBridge.h"
#include "SignalProcessor.h"

namespace audio {

/**
 * Unique identifier for audio tracks.
 * Uses UUID for global uniqueness and persistence.
 */
class TrackId {
public:
    TrackId() = default;
    explicit TrackId(const juce::String& uuidString) : uuid(uuidString) {}

    // Create a new valid track ID with a new UUID
    static TrackId createNew() { return TrackId(juce::Uuid().toString()); }

    juce::String toString() const { return uuid; }
    bool isValid() const { return uuid.isNotEmpty(); }
    bool operator==(const TrackId& other) const { return uuid == other.uuid; }
    bool operator!=(const TrackId& other) const { return uuid != other.uuid; }
    bool operator<(const TrackId& other) const { return uuid < other.uuid; }

private:
    juce::String uuid; // Empty string represents invalid ID
};

/**
 * Configuration and metadata for a single audio track.
 */
struct TrackInfo {
    TrackId id;
    std::string name;
    int channelIndex = 0;           // Which input channel this track captures
    bool isActive = true;           // Whether this track is currently capturing
    bool isVisible = true;          // Whether this track should be displayed
    juce::Colour color = juce::Colours::white;
    uint64_t samplesProcessed = 0;  // Total samples processed by this track

    TrackInfo() = default;
    TrackInfo(const TrackId& trackId, const std::string& trackName, int channel)
        : id(trackId), name(trackName), channelIndex(channel) {}
};

/**
 * Audio capture state for a single track.
 * Contains ring buffer, signal processor, and associated metadata.
 */
struct TrackCaptureState {
    std::unique_ptr<dsp::RingBuffer<float>> ringBuffer;
    std::unique_ptr<SignalProcessor> signalProcessor;
    TrackInfo info;
    std::atomic<bool> needsUpdate{false};

    TrackCaptureState(const TrackInfo& trackInfo, size_t bufferCapacity)
        : ringBuffer(std::make_unique<dsp::RingBuffer<float>>(bufferCapacity))
        , signalProcessor(std::make_unique<SignalProcessor>())
        , info(trackInfo) {}

    // Non-copyable but movable - remove explicit default moves to let compiler generate them
    TrackCaptureState(const TrackCaptureState&) = delete;
    TrackCaptureState& operator=(const TrackCaptureState&) = delete;
};

/**
 * Multi-track audio capture engine supporting up to 64 simultaneous tracks.
 *
 * Features:
 * - Dynamic track addition/removal without audio interruption
 * - Unique persistent track identifiers
 * - Per-track configuration and state management
 * - Thread-safe operations for audio and UI threads
 * - Memory-efficient scaling with linear resource usage
 *
 * Performance targets:
 * - <10MB memory per track
 * - <1ms track operation latency
 * - Zero audio thread blocking
 * - Support for 64 simultaneous tracks
 */
class MultiTrackEngine {
public:
    static constexpr size_t MAX_TRACKS = 64;
    static constexpr size_t DEFAULT_BUFFER_SIZE = 8192; // ~185ms at 44.1kHz

    MultiTrackEngine();
    ~MultiTrackEngine() = default;

    // === Core Lifecycle ===

    /**
     * Prepares the engine for audio processing.
     * Must be called before processAudioBlock.
     */
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numInputChannels);

    /**
     * Releases audio resources.
     */
    void releaseResources();

    /**
     * Processes audio block for all active tracks.
     * Called from audio thread - must be real-time safe.
     */
    void processAudioBlock(const float* const* channelData, int numChannels, int numSamples);

    // === Track Management ===

    /**
     * Adds a new track capturing from the specified input channel.
     * Returns the unique track ID for future reference.
     * Thread-safe, can be called from UI thread.
     */
    TrackId addTrack(const std::string& name, int channelIndex);

    /**
     * Removes a track by ID.
     * Thread-safe, can be called from UI thread.
     */
    bool removeTrack(const TrackId& trackId);

    /**
     * Gets track information by ID.
     * Returns nullptr if track doesn't exist.
     */
    const TrackInfo* getTrackInfo(const TrackId& trackId) const;

    /**
     * Updates track configuration.
     * Thread-safe, changes take effect on next audio block.
     */
    bool updateTrackInfo(const TrackId& trackId, const TrackInfo& newInfo);

    /**
     * Gets all current track IDs.
     * Returns a snapshot of track IDs at time of call.
     */
    std::vector<TrackId> getAllTrackIds() const;

    /**
     * Gets the number of active tracks.
     */
    size_t getNumTracks() const;

    // === Data Access ===

    /**
     * Gets the ring buffer for a specific track.
     * Returns nullptr if track doesn't exist.
     * Safe to call from UI thread for reading latest samples.
     */
    const dsp::RingBuffer<float>* getTrackRingBuffer(const TrackId& trackId) const;

    /**
     * Gets the waveform data bridge for multi-track UI communication.
     * This aggregates data from all active tracks.
     */
    WaveformDataBridge& getWaveformDataBridge() { return waveformBridge; }
    const WaveformDataBridge& getWaveformDataBridge() const { return waveformBridge; }

    // === Signal Processing ===

    /**
     * Sets signal processing configuration for a track.
     * Thread-safe, changes take effect on next audio block.
     */
    bool setTrackSignalProcessing(const TrackId& trackId, const ProcessingConfig& config);

    /**
     * Gets signal processing configuration for a track.
     */
    ProcessingConfig getTrackSignalProcessing(const TrackId& trackId) const;

    /**
     * Sets global signal processing mode for all tracks.
     * Convenience method for unified processing.
     */
    void setGlobalProcessingMode(SignalProcessingMode mode);

    /**
     * Gets the signal processor for a track.
     * Returns nullptr if track doesn't exist.
     */
    const SignalProcessor* getTrackSignalProcessor(const TrackId& trackId) const;

    // === Statistics ===

    /**
     * Gets memory usage statistics.
     */
    struct MemoryStats {
        size_t totalMemoryBytes = 0;
        size_t memoryPerTrack = 0;
        size_t numTracks = 0;
    };
    MemoryStats getMemoryStats() const;

    /**
     * Gets performance statistics.
     */
    struct PerformanceStats {
        uint64_t totalSamplesProcessed = 0;
        uint64_t totalTracksAdded = 0;
        uint64_t totalTracksRemoved = 0;
        std::atomic<uint64_t> audioBlocksProcessed{0};
    };
    const PerformanceStats& getPerformanceStats() const { return perfStats; }

private:
    // === Internal State ===

    mutable std::mutex trackMutex;                              // Protects track map modifications
    std::map<TrackId, std::unique_ptr<TrackCaptureState>> tracks;

    // Audio processing state
    static constexpr double DEFAULT_SAMPLE_RATE = 44100.0;
    double currentSampleRate = DEFAULT_SAMPLE_RATE;
    static constexpr int DEFAULT_BLOCK_SIZE = 512;
    int currentBlockSize = DEFAULT_BLOCK_SIZE;
    int numInputChannels = 2;
    bool isPrepared = false;

    // Multi-track data bridge for UI communication
    WaveformDataBridge waveformBridge;

    // Performance tracking
    mutable PerformanceStats perfStats;

    // === Internal Methods ===

    /**
     * Creates a new track capture state.
     * Called with mutex held.
     */
    std::unique_ptr<TrackCaptureState> createTrackCaptureState(const TrackInfo& info);

    /**
     * Updates the waveform bridge with data from all active tracks.
     * Called from audio thread.
     */
    void updateWaveformBridge(const float* const* channelData, int numChannels, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiTrackEngine)
};

} // namespace audio
