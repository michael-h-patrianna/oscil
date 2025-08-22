#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

class OscilAudioProcessor;
class OscilloscopeComponent;
class OpenGLManager;

class OscilAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
   public:
    explicit OscilAudioProcessorEditor(OscilAudioProcessor&);
    ~OscilAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // OpenGL management
    void enableOpenGL();
    void disableOpenGL();
    [[nodiscard]] bool isOpenGLEnabled() const;

   private:
    void timerCallback() override;

    OscilAudioProcessor& ap;
    std::unique_ptr<OscilloscopeComponent> oscilloscope;
    std::unique_ptr<OpenGLManager> openGLManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilAudioProcessorEditor)
};
