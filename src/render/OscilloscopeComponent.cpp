/**
 * @file OscilloscopeComponent.cpp
 * @brief Implementation of the main oscilloscope visualization component
 *
 * This file implements the OscilloscopeComponent class which provides real-time
 * waveform visualization for the Oscil oscilloscope plugin. The implementation
 * focuses on high-performance rendering with zero-allocation paths and adaptive
 * quality based on available processing time.
 *
 * Key Implementation Features:
 * - Lock-free audio data acquisition from the audio thread
 * - Zero-allocation rendering paths for real-time performance
 * - Adaptive level-of-detail based on performance budget
 * - GPU acceleration support through OpenGL (optional)
 * - Multi-channel waveform rendering with distinct colors
 * - Performance monitoring and statistics collection
 * - Responsive layout with cached bounds optimization
 *
 * Performance Characteristics:
 * - Target: 60 FPS with <1ms average frame time
 * - Memory: Zero allocations in main rendering path
 * - Threading: All rendering on message thread, lock-free data access
 * - Scalability: Supports up to 8 simultaneous audio channels
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include "OscilloscopeComponent.h"

#include "../plugin/PluginProcessor.h"
#include "../dsp/RingBuffer.h"
#include "../theme/ThemeManager.h"
#include "../ui/layout/LayoutManager.h"
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

    // Pre-allocate paths for maximum expected channels (64)
    cachedPaths.resize(audio::AudioDataSnapshot::MAX_CHANNELS);

    // Initialize all tracks as visible by default
    trackVisibility.fill(true);
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

void OscilloscopeComponent::setTrackVisible(int trackIndex, bool visible) {
    if (trackIndex >= 0 && trackIndex < static_cast<int>(trackVisibility.size())) {
        trackVisibility[static_cast<size_t>(trackIndex)] = visible;
        repaint(); // Trigger immediate visual update
    }
}

bool OscilloscopeComponent::isTrackVisible(int trackIndex) const {
    if (trackIndex >= 0 && trackIndex < static_cast<int>(trackVisibility.size())) {
        return trackVisibility[static_cast<size_t>(trackIndex)];
    }
    return false;
}

void OscilloscopeComponent::setAllTracksVisible(bool visible) {
    trackVisibility.fill(visible);
    repaint(); // Trigger immediate visual update
}

int OscilloscopeComponent::getNumVisibleTracks() const {
    return static_cast<int>(std::count(trackVisibility.begin(), trackVisibility.end(), true));
}

void OscilloscopeComponent::setLayoutManager(oscil::ui::layout::LayoutManager* manager) {
    layoutManager = manager;
    if (layoutManager) {
        layoutManager->setComponentBounds(getLocalBounds().toFloat());
    }
    repaint(); // Trigger immediate visual update
}

bool OscilloscopeComponent::assignTrackToRegion(int trackIndex, int regionIndex) {
    if (layoutManager) {
        return layoutManager->assignTrackToRegion(trackIndex, regionIndex);
    }
    return false;
}

void OscilloscopeComponent::autoDistributeTracks(int numTracks) {
    if (layoutManager) {
        layoutManager->autoDistributeTracks(numTracks);
        repaint(); // Trigger visual update
    }
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
        return themeManager->getMultiTrackWaveformColor(channelIndex);
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

void OscilloscopeComponent::renderOverlayLayout(juce::Graphics& graphics, int numChannels) {
    // Render all visible tracks overlaid in the same region (original behavior)
    for (int ch = 0; ch < numChannels; ++ch) {
        // Check visibility before rendering (performance optimization)
        if (ch < static_cast<int>(trackVisibility.size()) &&
            trackVisibility[static_cast<size_t>(ch)]) {

            renderChannel(graphics, ch,
                         currentSnapshot.samples[ch],
                         currentSnapshot.numSamples);
        }
    }
}

void OscilloscopeComponent::renderMultiRegionLayout(juce::Graphics& graphics, int numChannels) {
    juce::ignoreUnused(numChannels);
    if (!layoutManager) return;

    const auto& layout = layoutManager->getCurrentLayout();

    // Render each region separately
    for (size_t regionIndex = 0; regionIndex < layout.getNumRegions(); ++regionIndex) {
        const auto& region = layout.regions[regionIndex];
        if (region.isActive && !region.assignedTracks.empty()) {
            renderLayoutRegion(graphics, region, static_cast<int>(regionIndex));
        }
    }
}

void OscilloscopeComponent::renderLayoutRegion(juce::Graphics& graphics,
                                              const oscil::ui::layout::LayoutRegion& region,
                                              int regionIndex) {
    juce::ignoreUnused(regionIndex);
    // Create a clipped graphics context for this region
    juce::Graphics::ScopedSaveState saveState(graphics);
    graphics.reduceClipRegion(region.bounds.toNearestInt());

    // Draw region background if configured
    if (region.backgroundColor != juce::Colours::transparentBlack) {
        graphics.setColour(region.backgroundColor);
        graphics.fillRect(region.bounds);
    }

    // Draw region border if enabled
    const auto& layout = layoutManager->getCurrentLayout();
    if (layout.showRegionBorders) {
        graphics.setColour(layout.borderColor);
        graphics.drawRect(region.bounds, 1.0f);
    }

    // Render each track assigned to this region
    for (int trackIndex : region.assignedTracks) {
        // Check if track has audio data and is visible
        if (trackIndex < static_cast<int>(currentSnapshot.numChannels) &&
            trackIndex < static_cast<int>(trackVisibility.size()) &&
            trackVisibility[static_cast<size_t>(trackIndex)]) {

            // Temporarily modify cached bounds to match region
            auto originalBounds = cachedBounds.bounds;
            auto originalHeight = cachedBounds.channelHeight;
            auto originalSpacing = cachedBounds.channelSpacing;

            // Calculate track layout within region
            const int numTracksInRegion = static_cast<int>(region.assignedTracks.size());
            const float trackHeight = region.bounds.getHeight() * 0.8f / static_cast<float>(numTracksInRegion);
            const float trackSpacing = trackHeight;

            // Find this track's index within the region
            int trackIndexInRegion = 0;
            for (size_t i = 0; i < region.assignedTracks.size(); ++i) {
                if (region.assignedTracks[i] == trackIndex) {
                    trackIndexInRegion = static_cast<int>(i);
                    break;
                }
            }

            // Update cached bounds for region-local rendering
            cachedBounds.bounds = region.bounds;
            cachedBounds.channelHeight = trackHeight;
            cachedBounds.channelSpacing = trackSpacing;

            // Render the track
            renderChannelInRegion(graphics, trackIndex, trackIndexInRegion,
                                 currentSnapshot.samples[trackIndex],
                                 currentSnapshot.numSamples);

            // Restore original cached bounds
            cachedBounds.bounds = originalBounds;
            cachedBounds.channelHeight = originalHeight;
            cachedBounds.channelSpacing = originalSpacing;
        }
    }
}

void OscilloscopeComponent::renderChannelInRegion(juce::Graphics& graphics, int channelIndex, int regionChannelIndex,
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
                     static_cast<float>(regionChannelIndex) * cachedBounds.channelSpacing;

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

    // Render waveforms from thread-safe snapshot data (multi-track optimized path)
    if (hasNewData && currentSnapshot.numChannels > 0) {
        const auto numChannels = static_cast<int>(currentSnapshot.numChannels);

        // Update cached bounds for performance
        updateCachedBounds(numChannels);

        // Render based on layout mode (layout-aware rendering)
        if (layoutManager && layoutManager->getLayoutMode() != oscil::ui::layout::LayoutMode::Overlay) {
            // Multi-region layout mode - render tracks by region
            renderMultiRegionLayout(graphics, numChannels);
        } else {
            // Overlay mode - render all tracks in same region (current behavior)
            renderOverlayLayout(graphics, numChannels);
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

void OscilloscopeComponent::resized() {
    // Update layout manager bounds when component is resized
    if (layoutManager) {
        layoutManager->setComponentBounds(getLocalBounds().toFloat());
    }
}
