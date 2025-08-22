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

    // Check if we're running as standalone app - if so, we'll use the input from the audio device
    // For plugins (VST3, AU), we use the plugin's audio input directly
    bool isStandalone = (wrapperType == wrapperType_Standalone);

    if (!isStandalone) {
        // Plugin mode: use the audio input provided by the host
        // The buffer already contains the input audio from the DAW

        // Store input in ring buffers for oscilloscope display
        for (int ch = 0; ch < numChans; ++ch) {
            const auto* inputData = buffer.getReadPointer(ch);
            auto& ringBuffer = getRingBuffer(ch);
            ringBuffer.push(inputData, static_cast<size_t>(numSamples));
        }

        // Pass through the audio unchanged (this is an analyzer/visualizer effect)
        // No processing needed - output equals input
    } else {
        // Standalone mode: the buffer contains microphone/line input from the audio device
        // Store this input in ring buffers for oscilloscope display
        for (int ch = 0; ch < numChans; ++ch) {
            const auto* inputData = buffer.getReadPointer(ch);
            auto& ringBuffer = getRingBuffer(ch);
            ringBuffer.push(inputData, static_cast<size_t>(numSamples));
        }

        // For standalone, we might want to pass the audio through to outputs
        // so users can monitor what they're seeing
        // (Audio already in buffer, so output = input by default)
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
