#include "PluginEditor.h"

#include "OscilloscopeComponent.h"
#include "PluginProcessor.h"

OscilAudioProcessorEditor::OscilAudioProcessorEditor(OscilAudioProcessor& p) : juce::AudioProcessorEditor(&p), ap(p) {
    oscilloscope = std::make_unique<OscilloscopeComponent>(ap);
    this->addAndMakeVisible(*oscilloscope);

    this->setResizable(true, true);
    this->setResizeLimits(480, 320, 4096, 2160);
    this->setSize(800, 500);

    this->startTimerHz(30);  // refresh UI ~30 FPS
}

OscilAudioProcessorEditor::~OscilAudioProcessorEditor() = default;

void OscilAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(18, 18, 18));
}

void OscilAudioProcessorEditor::resized() {
    oscilloscope->setBounds(this->getLocalBounds());
}

void OscilAudioProcessorEditor::timerCallback() {
    oscilloscope->triggerAsyncRepaint();
}
