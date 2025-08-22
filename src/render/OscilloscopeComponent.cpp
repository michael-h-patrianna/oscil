#include "OscilloscopeComponent.h"

#include "../plugin/PluginProcessor.h"
#include "../dsp/RingBuffer.h"
#include "../theme/ThemeManager.h"
#include "OpenGLManager.h"
#include "GpuRenderHook.h"
#include <iostream>

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#endif

using juce::Colour;
using juce::Graphics;

static Colour channelColour(int idx) {
    static const Colour palette[] = {
        Colour::fromFloatRGBA(0.25f, 0.85f, 0.9f, 1.0f), Colour::fromFloatRGBA(0.9f, 0.6f, 0.3f, 1.0f),
        Colour::fromFloatRGBA(0.5f, 0.8f, 0.4f, 1.0f), Colour::fromFloatRGBA(0.8f, 0.4f, 0.9f, 1.0f)};
    return palette[idx % (int)(sizeof(palette) / sizeof(palette[0]))];
}

OscilloscopeComponent::OscilloscopeComponent(OscilAudioProcessor& proc) : processor(proc) {
    setInterceptsMouseClicks(false, false);

    // Pre-allocate paths for expected maximum channels
    cachedPaths.resize(8); // Start with 8 channels, will grow if needed
}

void OscilloscopeComponent::setOpenGLManager(OpenGLManager* manager) {
    openGLManager = manager;
}

void OscilloscopeComponent::setThemeManager(oscil::theme::ThemeManager* manager) {
    themeManager = manager;
}

oscil::util::PerformanceMonitor::FrameStats OscilloscopeComponent::getPerformanceStats() const {
    return performanceMonitor.getStats();
}

void OscilloscopeComponent::updateCachedBounds(int channelCount) {
    auto bounds = getLocalBounds().toFloat();

    if (cachedBounds.isValid &&
        cachedBounds.bounds == bounds &&
        cachedBounds.lastChannelCount == channelCount) {
        return; // Cache is still valid
    }

    cachedBounds.bounds = bounds;
    cachedBounds.lastChannelCount = channelCount;
    cachedBounds.channelHeight = bounds.getHeight() * 0.8f / juce::jmax(1, channelCount);
    cachedBounds.channelSpacing = bounds.getHeight() * 0.8f / juce::jmax(1, channelCount);
    cachedBounds.isValid = true;
}

juce::Colour OscilloscopeComponent::getChannelColor(int channelIndex) const {
    if (themeManager != nullptr) {
        return themeManager->getWaveformColor(channelIndex);
    }
    // Fallback to old static method if no theme manager
    return channelColour(channelIndex);
}

void OscilloscopeComponent::renderChannel(juce::Graphics& graphics, int channelIndex,
                                        const float* samples, size_t sampleCount) {
    if (sampleCount <= 1) return;

    graphics.setColour(getChannelColor(channelIndex));

    // Ensure we have enough cached paths
    if (static_cast<size_t>(channelIndex) >= cachedPaths.size()) {
        cachedPaths.resize(static_cast<size_t>(channelIndex) + 1);
    }

    auto& path = cachedPaths[static_cast<size_t>(channelIndex)];
    path.clear(); // Reuse existing path object

    const auto w = cachedBounds.bounds.getWidth();
    const auto h = cachedBounds.channelHeight;
    const auto top = cachedBounds.bounds.getY() +
                     static_cast<float>(channelIndex) * cachedBounds.channelSpacing;

    // Apply decimation if needed
    int targetPixels = static_cast<int>(w);
    auto decimationResult = decimationProcessor.process(samples, sampleCount, targetPixels);

    // Render using decimated or original data
    for (size_t i = 0; i < decimationResult.sampleCount; ++i) {
        float x = cachedBounds.bounds.getX() + static_cast<float>(i) /
                 static_cast<float>(decimationResult.sampleCount - 1) * w;
        float sampleValue = decimationResult.samples[i];
        float y = top + h * 0.5f * (1.0f - juce::jlimit(-1.0f, 1.0f, sampleValue));

        if (i == 0) {
            path.startNewSubPath(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    graphics.strokePath(path, juce::PathStrokeType(1.5f));
}

void OscilloscopeComponent::paint(Graphics& graphics) {
    // Start performance timing
    auto paintStartTime = performanceMonitor.startTiming();

    frameCounter++;
    performanceMonitor.recordFrame();

    auto bounds = getLocalBounds().toFloat();

    // Get GPU render hook if available
    std::shared_ptr<GpuRenderHook> gpuHook = nullptr;
    bool useGpuHook = false;

#if OSCIL_ENABLE_OPENGL
    if (openGLManager && openGLManager->isOpenGLActive()) {
        gpuHook = openGLManager->getGpuRenderHook();
        useGpuHook = gpuHook && gpuHook->isActive();
    }
#endif

    // GPU Hook: Begin frame
    if (useGpuHook) {
        gpuHook->beginFrame(bounds, frameCounter);
    }

    // Try to get new audio data from the bridge
    auto& bridge = processor.getWaveformDataBridge();
    if (bridge.pullLatestData(currentSnapshot)) {
        hasNewData = true;
    }

    // Reduced debug output to avoid performance impact
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 3600 == 0) { // Every minute at 60fps
#if OSCIL_ENABLE_OPENGL
        if (juce::OpenGLContext::getCurrentContext() != nullptr) {
            if (useGpuHook) {
                std::cout << "[OSCIL] Renderer: OpenGL + GPU Hook ACTIVE\n";
            } else {
                std::cout << "[OSCIL] Renderer: OpenGL acceleration\n";
            }
        } else {
            std::cout << "[OSCIL] Renderer: CPU-based drawing\n";
        }
#else
        std::cout << "[OSCIL] Renderer: CPU-only\n";
#endif
    }

    // Background - use theme colors if available
    if (themeManager != nullptr) {
        graphics.setColour(themeManager->getBackgroundColor());
    } else {
        graphics.setColour(Colour::fromRGB(24, 24, 24));  // Fallback dark background
    }
    graphics.fillRoundedRectangle(bounds.reduced(6.f), 8.f);

    // Grid - use theme colors if available
    if (themeManager != nullptr) {
        graphics.setColour(themeManager->getGridColor().withAlpha(0.3f));
    } else {
        graphics.setColour(Colour::fromRGBA(255, 255, 255, 76));  // Fallback grid (30% opacity)
    }
    const int gridLines = 8;
    for (int i = 1; i < gridLines; ++i) {
        auto x = bounds.getX() + bounds.getWidth() * static_cast<float>(i) / static_cast<float>(gridLines);
        auto y = bounds.getY() + bounds.getHeight() * static_cast<float>(i) / static_cast<float>(gridLines);
        graphics.drawLine(x, bounds.getY(), x, bounds.getBottom(), 1.0f);
        graphics.drawLine(bounds.getX(), y, bounds.getRight(), y, 1.0f);
    }

    // GPU Hook: Prepare for waveform drawing
    if (useGpuHook) {
        gpuHook->drawWaveforms(static_cast<int>(currentSnapshot.numChannels));
    }

    // Render waveforms from thread-safe snapshot data (optimized path)
    if (hasNewData && currentSnapshot.numChannels > 0) {
        const auto numChannels = static_cast<int>(currentSnapshot.numChannels);

        // Update cached bounds for performance
        updateCachedBounds(numChannels);

        for (int ch = 0; ch < numChannels; ++ch) {
            renderChannel(graphics, ch,
                         currentSnapshot.samples[ch],
                         currentSnapshot.numSamples);
        }
    }

    // GPU Hook: Apply post-processing effects
    if (useGpuHook) {
        gpuHook->applyPostFx(nullptr); // nullptr = use default framebuffer
    }

    // GPU Hook: End frame
    if (useGpuHook) {
        gpuHook->endFrame();
    }

    // End performance timing
    performanceMonitor.endTiming(paintStartTime);
}

void OscilloscopeComponent::resized() {}
