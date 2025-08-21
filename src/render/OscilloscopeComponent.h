#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OscilAudioProcessor;

class OscilloscopeComponent : public juce::Component, private juce::AsyncUpdater {
   public:
    explicit OscilloscopeComponent(OscilAudioProcessor& proc);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void triggerAsyncRepaint() {
        triggerAsyncUpdate();
    }

   private:
    void handleAsyncUpdate() override {
        repaint();
    }

    OscilAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};
