#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../dsp/RingBuffer.h"
#include "../audio/WaveformDataBridge.h"
#include "../state/TrackState.h"
#include "../theme/ThemeManager.h"

class OscilAudioProcessor : public juce::AudioProcessor {
   public:
    OscilAudioProcessor();
    ~OscilAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {
        return true;
    }

    //==============================================================================
    const juce::String getName() const override {
        return "Oscil";
    }

    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    bool isMidiEffect() const override {
        return false;
    }

    double getTailLengthSeconds() const override {
        return 0.0;
    }

    //==============================================================================
    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {
        return {};
    }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Access to oscilloscope buffers (legacy for backward compatibility)
    dsp::RingBuffer<float>& getRingBuffer(int channel) {
        return *ringBuffers[(size_t)juce::jlimit(0, 63, channel)];
    }
    int getNumRingBuffers() const {
        return (int)ringBuffers.size();
    }

    // New thread-safe audio data access
    audio::WaveformDataBridge& getWaveformDataBridge() {
        return waveformBridge;
    }

    // === Theme Management ===

    /**
     * Gets the theme manager for accessing color themes.
     */
    oscil::theme::ThemeManager& getThemeManager() { return themeManager; }
    const oscil::theme::ThemeManager& getThemeManager() const { return themeManager; }

    // === Track State Management ===

    /**
     * Gets the track state for the primary track (track 0).
     * For single-track implementation, we use track 0 as the default.
     */
    oscil::state::TrackState& getTrackState() { return trackState; }
    const oscil::state::TrackState& getTrackState() const { return trackState; }

    /**
     * Applies the track state settings to the current configuration.
     * This should be called when track state changes to update the UI.
     */
    void applyTrackStateChanges();

   private:
    std::vector<std::unique_ptr<dsp::RingBuffer<float>>> ringBuffers;
    audio::WaveformDataBridge waveformBridge;

    // Temporary buffer for preparing channel pointers
    std::vector<const float*> channelPointers;

    // Single track state (for Task 2.2 - Single Track State Management)
    oscil::state::TrackState trackState;

    // Theme management (for Task 2.3 - Basic Color and Theme System)
    oscil::theme::ThemeManager themeManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilAudioProcessor)
};
