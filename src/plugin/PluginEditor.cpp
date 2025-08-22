#include "PluginEditor.h"

#include "../render/OscilloscopeComponent.h"
#include "../render/OpenGLManager.h"
#include "PluginProcessor.h"

OscilAudioProcessorEditor::OscilAudioProcessorEditor(OscilAudioProcessor& p) : juce::AudioProcessorEditor(&p), ap(p) {
    oscilloscope = std::make_unique<OscilloscopeComponent>(ap);
    this->addAndMakeVisible(*oscilloscope);

    // Create OpenGL manager
    openGLManager = std::make_unique<OpenGLManager>();

    this->setResizable(true, true);
    this->setResizeLimits(480, 320, 4096, 2160);
    this->setSize(800, 500);

    this->startTimerHz(60);  // refresh UI ~60 FPS

    // Conditionally enable OpenGL if compiled in
#if OSCIL_ENABLE_OPENGL
    enableOpenGL();
#endif
}

OscilAudioProcessorEditor::~OscilAudioProcessorEditor() {
    // Ensure OpenGL is disabled before destruction
    disableOpenGL();
}

void OscilAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(18, 18, 18));
}

void OscilAudioProcessorEditor::resized() {
    oscilloscope->setBounds(this->getLocalBounds());
}

void OscilAudioProcessorEditor::timerCallback() {
    oscilloscope->triggerAsyncRepaint();
}

void OscilAudioProcessorEditor::enableOpenGL() {
    if (openGLManager && OpenGLManager::isOpenGLAvailable()) {
        if (openGLManager->attachTo(*this)) {
            // OpenGL enabled successfully
            oscilloscope->repaint(); // Trigger repaint to ensure visual consistency
        }
    }
}

void OscilAudioProcessorEditor::disableOpenGL() {
    if (openGLManager) {
        openGLManager->detach();
        oscilloscope->repaint(); // Trigger repaint to ensure visual consistency
    }
}

bool OscilAudioProcessorEditor::isOpenGLEnabled() const {
    return openGLManager && openGLManager->isOpenGLActive();
}
