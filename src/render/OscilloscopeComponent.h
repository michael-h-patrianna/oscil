#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../audio/WaveformDataBridge.h"
#include "../util/PerformanceMonitor.h"
#include "DecimationProcessor.h"

// Forward declarations
class OscilAudioProcessor;
class OpenGLManager;
class GpuRenderHook;

namespace oscil::theme {
    class ThemeManager;
}

class OscilloscopeComponent : public juce::Component {
public:
    explicit OscilloscopeComponent(OscilAudioProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;

    /**
     * Sets the OpenGL manager for GPU render hook integration.
     * @param manager Pointer to OpenGL manager (can be nullptr)
     */
    void setOpenGLManager(OpenGLManager* manager);

    /**
     * Sets the theme manager for color theming.
     * @param manager Pointer to theme manager (can be nullptr)
     */
    void setThemeManager(oscil::theme::ThemeManager* manager);

    /**
     * Get current performance statistics.
     */
    oscil::util::PerformanceMonitor::FrameStats getPerformanceStats() const;

private:
    OscilAudioProcessor& processor;
    OpenGLManager* openGLManager = nullptr;
    oscil::theme::ThemeManager* themeManager = nullptr;

    // Performance monitoring
    oscil::util::PerformanceMonitor performanceMonitor;

    // Decimation processor for LOD optimization
    oscil::render::DecimationProcessor decimationProcessor;

    // Frame counter for time-based effects and debugging
    uint64_t frameCounter = 0;

    // Thread-safe audio data snapshot
    audio::AudioDataSnapshot currentSnapshot;
    bool hasNewData = false;

    // Pre-allocated buffers to avoid allocations in paint()
    std::vector<juce::Path> cachedPaths;

    // Cached bounds to avoid recalculation
    struct CachedBounds {
        juce::Rectangle<float> bounds;
        float channelHeight = 0.0f;
        float channelSpacing = 0.0f;
        int lastChannelCount = 0;
        bool isValid = false;
    } cachedBounds;

    /**
     * Gets waveform color for a specific channel, using theme if available
     */
    juce::Colour getChannelColor(int channelIndex) const;

    /**
     * Update cached bounds if needed
     */
    void updateCachedBounds(int channelCount);

    /**
     * Render a single channel with optimizations
     */
    void renderChannel(juce::Graphics& g, int channelIndex,
                      const float* samples, size_t sampleCount);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeComponent)
};
