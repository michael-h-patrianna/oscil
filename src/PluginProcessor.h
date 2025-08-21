#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/RingBuffer.h"

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

    // Access to oscilloscope buffers
    dsp::RingBuffer<float>& getRingBuffer(int channel) {
        return *ringBuffers[(size_t)juce::jlimit(0, 63, channel)];
    }
    int getNumRingBuffers() const {
        return (int)ringBuffers.size();
    }

   private:
    std::vector<std::unique_ptr<dsp::RingBuffer<float>>> ringBuffers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilAudioProcessor)
};
