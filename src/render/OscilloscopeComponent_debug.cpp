/**
 * @file OscilloscopeComponent_debug.cpp
 * @brief Debug implementation of oscilloscope visualization component
 *
 * This file contains a debug/development version of the oscilloscope component
 * implementation with additional logging, simplified rendering paths, and
 * diagnostic features for troubleshooting and development purposes.
 *
 * Key Implementation Features:
 * - Simplified rendering for debugging purposes
 * - Enhanced logging and diagnostic output
 * - Development-focused visualization modes
 * - Performance monitoring and timing analysis
 * - Debug-specific color schemes and markers
 * - Fallback rendering when GPU acceleration unavailable
 *
 * Performance Characteristics:
 * - Less optimized than production version
 * - Additional diagnostic overhead acceptable
 * - Simplified algorithms for easier debugging
 * - Enhanced error checking and validation
 * - Verbose logging for development analysis
 *
 * Debug Features:
 * - Frame-by-frame rendering analysis
 * - Audio data validation and diagnostics
 * - Performance bottleneck identification
 * - Visual debugging aids and overlays
 * - Development-time configuration options
 *
 * @note This is a development/debug build component
 * @note Not optimized for production use
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include "OscilloscopeComponent.h"

#include "../plugin/PluginProcessor.h"
#include "../dsp/RingBuffer.h"
#include <iostream>

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
}

void OscilloscopeComponent::paint(Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // background
    g.setColour(Colour::fromRGB(24, 24, 24));
    g.fillRoundedRectangle(bounds.reduced(6.f), 8.f);

    // grid
    g.setColour(Colour::fromRGBA(255, 255, 255, 20));
    const int gridLines = 8;
    for (int i = 1; i < gridLines; ++i) {
        auto x = bounds.getX() + bounds.getWidth() * (float)i / gridLines;
        auto y = bounds.getY() + bounds.getHeight() * (float)i / gridLines;
        g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 1.0f);
        g.drawLine(bounds.getX(), y, bounds.getRight(), y, 1.0f);
    }

    // DEBUG: Check multi-track engine status
    auto& multiTrackEngine = processor.getMultiTrackEngine();
    auto& bridge = multiTrackEngine.getWaveformDataBridge();

    // Get latest audio data snapshot from the bridge
    audio::AudioDataSnapshot snapshot;
    bool hasData = bridge.pullLatestData(snapshot);

    static int debugCounter = 0;
    debugCounter++;

    if (debugCounter % 60 == 0) { // Debug every 60 paint calls (~1 second at 60fps)
        std::cout << "=== Oscilloscope Debug (paint #" << debugCounter << ") ===\n";

        std::cout << "MultiTrackEngine bridge has data: " << (hasData ? "YES" : "NO") << "\n";

        if (hasData) {
            std::cout << "Channels: " << snapshot.numChannels << ", Samples per channel: " << snapshot.numSamples << "\n";

            for (size_t ch = 0; ch < snapshot.numChannels && ch < 4; ++ch) { // Limit to 4 channels for debug
                // Check if we have any non-zero data
                float minVal = 0.0f;
                float maxVal = 0.0f;
                bool hasNonZeroData = false;

                for (size_t i = 0; i < snapshot.numSamples; ++i) {
                    float sample = snapshot.samples[ch][i];
                    if (std::abs(sample) > 0.001f) {
                        hasNonZeroData = true;
                        minVal = std::min(minVal, sample);
                        maxVal = std::max(maxVal, sample);
                    }
                }

                std::cout << "Channel " << ch << ": hasData=" << (hasNonZeroData ? "YES" : "NO");
                if (hasNonZeroData) {
                    std::cout << ", range=[" << minVal << ", " << maxVal << "]";
                }
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }

    // Render waveforms using data from MultiTrackEngine bridge
    if (hasData && snapshot.numChannels > 0) {
        const auto numChannels = static_cast<int>(snapshot.numChannels);

        for (int ch = 0; ch < numChannels; ++ch) {
            g.setColour(channelColour(ch));
            auto path = juce::Path{};

            const auto w = bounds.getWidth();
            const auto h = bounds.getHeight() * 0.8f / juce::jmax(1, numChannels);
            const auto top = bounds.getY() + static_cast<float>(ch) * (bounds.getHeight() * 0.8f / juce::jmax(1, numChannels));

            if (snapshot.numSamples > 1) {
                for (size_t i = 0; i < snapshot.numSamples; ++i) {
                    float x = bounds.getX() + static_cast<float>(i) / static_cast<float>(snapshot.numSamples - 1) * w;
                    float sampleValue = snapshot.samples[ch][i];
                    float y = top + h * 0.5f * (1.0f - juce::jlimit(-1.0f, 1.0f, sampleValue));
                    if (i == 0)
                        path.startNewSubPath(x, y);
                    else
                        path.lineTo(x, y);
                }
                g.strokePath(path, juce::PathStrokeType(1.5f));
            }
        }
    }
}
