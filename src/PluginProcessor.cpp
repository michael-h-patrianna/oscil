#include "PluginProcessor.h"

#include "PluginEditor.h"

OscilAudioProcessor::OscilAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

OscilAudioProcessor::~OscilAudioProcessor() = default;

void OscilAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const int numChans = juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels());
    ringBuffers.clear();
    ringBuffers.reserve((size_t)numChans);

    const size_t desiredMs = 200;  // capture 200ms of audio per channel
    const size_t capacity = static_cast<size_t>(sampleRate * desiredMs / 1000.0);

    for (int ch = 0; ch < numChans; ++ch) {
        auto rb = std::make_unique<dsp::RingBuffer<float>>(capacity);
        ringBuffers.emplace_back(std::move(rb));
    }

    juce::ignoreUnused(samplesPerBlock);
}

void OscilAudioProcessor::releaseResources() {}

bool OscilAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Require same number of channels in and out, at least mono
    if (layouts.getMainInputChannelSet().size() == 0 ||
        layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return true;
}

void OscilAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const int numChans = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // For an analyzer effect, pass audio through unchanged
    for (int ch = 0; ch < numChans; ++ch) {
        auto* data = buffer.getWritePointer(ch);
        // write into ring buffer
        auto& rb = getRingBuffer(ch);
        rb.push(data, (size_t)numSamples);
    }
}

void OscilAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ignoreUnused(destData);
}

void OscilAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessorEditor* OscilAudioProcessor::createEditor() {
    return new OscilAudioProcessorEditor(*this);
}

// JUCE factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OscilAudioProcessor();
}
