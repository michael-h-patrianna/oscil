#include "PluginProcessor.h"

#include "PluginEditor.h"

OscilAudioProcessor::OscilAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , trackState(0)  // Initialize with track ID 0
{}

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

    // Pre-allocate channel pointers for WaveformDataBridge
    channelPointers.reserve((size_t)numChans);

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

        // Store input in ring buffers for oscilloscope display (legacy)
        for (int ch = 0; ch < numChans; ++ch) {
            const auto* inputData = buffer.getReadPointer(ch);
            auto& ringBuffer = getRingBuffer(ch);
            ringBuffer.push(inputData, static_cast<size_t>(numSamples));
        }

        // NEW: Send data to WaveformDataBridge for thread-safe UI communication
        channelPointers.clear();
        for (int ch = 0; ch < numChans; ++ch) {
            channelPointers.push_back(buffer.getReadPointer(ch));
        }
        waveformBridge.pushAudioData(channelPointers.data(),
                                     static_cast<size_t>(numChans),
                                     static_cast<size_t>(numSamples),
                                     getSampleRate());

        // Pass through the audio unchanged (this is an analyzer/visualizer effect)
        // No processing needed - output equals input
    } else {
        // Standalone mode: the buffer contains microphone/line input from the audio device
        // Store this input in ring buffers for oscilloscope display (legacy)
        for (int ch = 0; ch < numChans; ++ch) {
            const auto* inputData = buffer.getReadPointer(ch);
            auto& ringBuffer = getRingBuffer(ch);
            ringBuffer.push(inputData, static_cast<size_t>(numSamples));
        }

        // NEW: Send data to WaveformDataBridge for thread-safe UI communication
        channelPointers.clear();
        for (int ch = 0; ch < numChans; ++ch) {
            channelPointers.push_back(buffer.getReadPointer(ch));
        }
        waveformBridge.pushAudioData(channelPointers.data(),
                                     static_cast<size_t>(numChans),
                                     static_cast<size_t>(numSamples),
                                     getSampleRate());

        // For standalone, we might want to pass the audio through to outputs
        // so users can monitor what they're seeing
        // (Audio already in buffer, so output = input by default)
    }
}

void OscilAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Create a ValueTree containing all plugin state
    juce::ValueTree pluginState("OscilPluginState");

    // Add track state
    pluginState.appendChild(trackState.getState(), nullptr);

    // Add theme state
    juce::ValueTree themeState("ThemeState");
    themeState.setProperty("currentThemeId", static_cast<int>(themeManager.getCurrentThemeId()), nullptr);
    themeState.setProperty("themeName", themeManager.getCurrentTheme().name, nullptr);
    pluginState.appendChild(themeState, nullptr);

    // Add version information
    pluginState.setProperty("version", 1, nullptr);

    // Convert to XML and store in memory block
    auto xml = pluginState.createXml();
    if (xml) {
        copyXmlToBinary(*xml, destData);
    }
}

void OscilAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    // Parse XML from memory block
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml) {
        auto pluginState = juce::ValueTree::fromXml(*xml);
        if (pluginState.isValid()) {
            // Find and restore track state
            auto trackStateChild = pluginState.getChildWithName("TrackState");
            if (trackStateChild.isValid()) {
                trackState.replaceState(trackStateChild);
            }

            // Find and restore theme state
            auto themeStateChild = pluginState.getChildWithName("ThemeState");
            if (themeStateChild.isValid()) {
                int themeIdInt = themeStateChild.getProperty("currentThemeId", 0);
                auto themeId = static_cast<oscil::theme::ThemeManager::ThemeId>(themeIdInt);

                // Try to restore by theme ID first, then by name as fallback
                if (!themeManager.setCurrentTheme(themeId)) {
                    juce::String themeName = themeStateChild.getProperty("themeName", "Dark Professional");
                    themeManager.setCurrentTheme(themeName);
                }
            }

            // Apply the restored state
            applyTrackStateChanges();
        }
    }
}

void OscilAudioProcessor::applyTrackStateChanges() {
    // This method will be called when track state changes
    // For now, this is a placeholder - in the future it will notify
    // UI components that the state has changed

    // The UI should listen to track state changes and update accordingly
    // This could be implemented using listeners or callbacks
}

juce::AudioProcessorEditor* OscilAudioProcessor::createEditor() {
    return new OscilAudioProcessorEditor(*this);
}

// JUCE factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OscilAudioProcessor();
}
