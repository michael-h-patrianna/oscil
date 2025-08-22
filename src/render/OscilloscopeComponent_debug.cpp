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

    // DEBUG: Check ring buffer status
    const int channels = processor.getNumRingBuffers();
    static int debugCounter = 0;
    debugCounter++;

    if (debugCounter % 60 == 0) { // Debug every 60 paint calls (~1 second at 60fps)
        std::cout << "=== Oscilloscope Debug (paint #" << debugCounter << ") ===\n";
        std::cout << "Channels: " << channels << "\n";

        for (int ch = 0; ch < channels; ++ch) {
            auto& rb = processor.getRingBuffer(ch);
            const size_t N = juce::jmin((size_t)1024, rb.size());
            std::vector<float> temp(N, 0.f);
            rb.peekLatest(temp.data(), N);

            // Check if we have any non-zero data
            float minVal = 0.0f, maxVal = 0.0f;
            bool hasData = false;
            for (size_t i = 0; i < N; ++i) {
                if (std::abs(temp[i]) > 0.001f) {
                    hasData = true;
                    minVal = std::min(minVal, temp[i]);
                    maxVal = std::max(maxVal, temp[i]);
                }
            }

            std::cout << "Channel " << ch << ": Ring buffer size=" << rb.size()
                      << ", hasData=" << (hasData ? "YES" : "NO");
            if (hasData) {
                std::cout << ", range=[" << minVal << ", " << maxVal << "]";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    // waveforms per channel
    for (int ch = 0; ch < channels; ++ch) {
        g.setColour(channelColour(ch));
        auto path = juce::Path{};
        auto& rb = processor.getRingBuffer(ch);

        const size_t N = juce::jmin((size_t)1024, rb.size());
        std::vector<float> temp(N, 0.f);
        rb.peekLatest(temp.data(), N);

        const auto w = bounds.getWidth();
        const auto h = bounds.getHeight() * 0.8f / juce::jmax(1, channels);
        const auto top = bounds.getY() + (float)ch * (bounds.getHeight() * 0.8f / juce::jmax(1, channels));
        if (N > 1) {
            for (size_t i = 0; i < N; ++i) {
                float x = bounds.getX() + (float)i / (N - 1) * w;
                float y = top + h * 0.5f * (1.0f - juce::jlimit(-1.0f, 1.0f, temp[i]));
                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }
}
