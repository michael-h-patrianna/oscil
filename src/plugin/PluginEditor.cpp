/**
 * @file PluginEditor.cpp
 * @brief Implementation of the main plugin editor interface
 *
 * This file implements the OscilAudioProcessorEditor class which provides the
 * primary user interface for the Oscil oscilloscope plugin. The implementation
 * manages the oscilloscope visualization, user controls, debug interface, and
 * integration with OpenGL rendering systems.
 *
 * Key Implementation Features:
 * - JUCE-based user interface with custom components
 * - OpenGL integration for GPU-accelerated rendering
 * - Real-time oscilloscope visualization management
 * - Theme system integration for consistent styling
 * - Debug interface for development and troubleshooting
 * - Responsive layout with adaptive sizing
 * - Performance monitoring and display
 *
 * Performance Characteristics:
 * - UI refresh rate: 60 FPS typical
 * - GPU rendering when available
 * - Minimal CPU usage for UI updates
 * - Responsive interaction with <50ms latency
 * - Memory efficient component management
 *
 * UI Architecture:
 * - Main oscilloscope visualization component
 * - Optional debug interface overlay
 * - Theme-aware color management
 * - OpenGL context management for acceleration
 * - Integration with audio processor for data access
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include "PluginEditor.h"

#include "../render/OscilloscopeComponent.h"
#include "../render/OpenGLManager.h"
#include "../render/GpuRenderHook.h"
#include "PluginProcessor.h"

OscilAudioProcessorEditor::OscilAudioProcessorEditor(OscilAudioProcessor& p) : juce::AudioProcessorEditor(&p), ap(p) {
    oscilloscope = std::make_unique<OscilloscopeComponent>(ap);
    this->addAndMakeVisible(*oscilloscope);

    // Create OpenGL manager
    openGLManager = std::make_unique<OpenGLManager>();

    // Connect OpenGL manager to oscilloscope component for GPU hook integration
    oscilloscope->setOpenGLManager(openGLManager.get());

    // Connect theme manager to oscilloscope component
    oscilloscope->setThemeManager(&ap.getThemeManager());

    // Setup debug UI
    setupDebugUI();

    this->setResizable(true, true);
    this->setResizeLimits(480, 320 + DEBUG_UI_HEIGHT, 4096, 2160);
    this->setSize(800, 500 + DEBUG_UI_HEIGHT);

    this->startTimerHz(60);  // refresh UI ~60 FPS

    // Conditionally enable OpenGL if compiled in
#if OSCIL_ENABLE_OPENGL
    enableOpenGL();

    // Set up debug GPU hook if enabled
    #ifdef OSCIL_DEBUG_HOOKS
    auto debugHook = std::make_shared<DebugGpuRenderHook>();
    openGLManager->setGpuRenderHook(debugHook);
    #endif
#endif
}

OscilAudioProcessorEditor::~OscilAudioProcessorEditor() {
    // Ensure OpenGL is disabled before destruction
    disableOpenGL();
}

void OscilAudioProcessorEditor::paint(juce::Graphics& g) {
    // Fill debug UI bar with a different color
    g.setColour(juce::Colour::fromRGB(40, 40, 40));
    g.fillRect(0, 0, getWidth(), DEBUG_UI_HEIGHT);

    // Fill main area with default background
    g.setColour(juce::Colour::fromRGB(18, 18, 18));
    g.fillRect(0, DEBUG_UI_HEIGHT, getWidth(), getHeight() - DEBUG_UI_HEIGHT);
}

void OscilAudioProcessorEditor::resized() {
    // Position debug UI elements in the top bar
    auto debugArea = juce::Rectangle<int>(0, 0, getWidth(), DEBUG_UI_HEIGHT);
    auto currentX = 10;

    // Theme label and selector
    themeLabel.setBounds(currentX, 5, 50, 20);
    currentX += 55;
    themeSelector.setBounds(currentX, 5, 120, 20);

    // Position oscilloscope below debug UI
    auto oscilloscopeArea = juce::Rectangle<int>(0, DEBUG_UI_HEIGHT, getWidth(), getHeight() - DEBUG_UI_HEIGHT);
    oscilloscope->setBounds(oscilloscopeArea);
}

void OscilAudioProcessorEditor::setupDebugUI() {
    // Setup theme label
    themeLabel.setText("Theme:", juce::dontSendNotification);
    themeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(themeLabel);

    // Setup theme selector
    auto& themeManager = ap.getThemeManager();
    auto themeNames = themeManager.getAvailableThemeNames();

    for (int i = 0; i < themeNames.size(); ++i) {
        themeSelector.addItem(themeNames[i], i + 1);
    }

    // Set current theme as selected
    auto currentThemeName = themeManager.getCurrentTheme().name;
    themeSelector.setText(currentThemeName, juce::dontSendNotification);

    // Setup theme change callback
    themeSelector.onChange = [this]() {
        auto selectedTheme = themeSelector.getText();
        ap.getThemeManager().setCurrentTheme(selectedTheme);
        // Trigger repaint to update colors
        repaint();
        oscilloscope->repaint();
    };

    addAndMakeVisible(themeSelector);
}

void OscilAudioProcessorEditor::timerCallback() {
    oscilloscope->repaint();
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
