/**
 * @file test_multi_track_rendering.cpp
 * @brief Test suite for multi-track waveform rendering functionality
 *
 * This file contains comprehensive tests for Task 3.2: Multi-Track Waveform Rendering.
 * Tests cover performance, correctness, and integration with existing systems.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../src/render/OscilloscopeComponent.h"
#include "../src/plugin/PluginProcessor.h"
#include "../src/theme/ThemeManager.h"
#include "../src/audio/MultiTrackEngine.h"

using namespace juce;

/**
 * Test fixture for multi-track rendering tests
 */
class MultiTrackRenderingTestFixture {
public:
    MultiTrackRenderingTestFixture() : processor() {
        themeManager = std::make_unique<oscil::theme::ThemeManager>();
        component = std::make_unique<OscilloscopeComponent>(processor);
        component->setThemeManager(themeManager.get());

        // Prepare for testing
        processor.prepareToPlay(44100.0, 512);
    }

    ~MultiTrackRenderingTestFixture() {
        processor.releaseResources();
    }

    void addTestTracks(int numTracks) {
        auto& multiTrackEngine = processor.getMultiTrackEngine();

        for (int i = 0; i < numTracks; ++i) {
            auto trackId = multiTrackEngine.addTrack("Test Track " + std::to_string(i), i % 8);
            trackIds.push_back(trackId);
        }
    }

    void simulateAudioInput(int numChannels, int numSamples) {
        // Create test audio data
        std::vector<std::vector<float>> audioData(numChannels);
        std::vector<const float*> channelPointers(numChannels);

        for (int ch = 0; ch < numChannels; ++ch) {
            audioData[ch].resize(numSamples);

            // Generate test sine wave with different frequencies per channel
            float frequency = 440.0f * (ch + 1);
            for (int sample = 0; sample < numSamples; ++sample) {
                float time = static_cast<float>(sample) / 44100.0f;
                audioData[ch][sample] = std::sin(2.0f * juce::MathConstants<float>::pi * frequency * time) * 0.5f;
            }

            channelPointers[ch] = audioData[ch].data();
        }

        // Process through multi-track engine
        auto& multiTrackEngine = processor.getMultiTrackEngine();
        multiTrackEngine.processAudioBlock(channelPointers.data(), numChannels, numSamples);
    }

    OscilAudioProcessor processor;
    std::unique_ptr<oscil::theme::ThemeManager> themeManager;
    std::unique_ptr<OscilloscopeComponent> component;
    std::vector<audio::TrackId> trackIds;
};

TEST_CASE_METHOD(MultiTrackRenderingTestFixture, "Multi-track rendering basic functionality", "[multitrack][rendering]") {
    SECTION("Component handles multi-track visibility") {
        // All tracks should be visible by default
        REQUIRE(component->getNumVisibleTracks() == 64); // All tracks visible by default

        // Test individual track visibility
        component->setTrackVisible(0, false);
        REQUIRE_FALSE(component->isTrackVisible(0));
        REQUIRE(component->isTrackVisible(1));

        // Test visibility beyond valid range
        REQUIRE_FALSE(component->isTrackVisible(100));

        // Test batch visibility setting
        component->setAllTracksVisible(false);
        REQUIRE(component->getNumVisibleTracks() == 0);

        component->setAllTracksVisible(true);
        REQUIRE(component->getNumVisibleTracks() == 64);
    }

    SECTION("Multi-track color assignment works correctly") {
        // Test color retrieval for different track indices
        auto color0 = themeManager->getMultiTrackWaveformColor(0);
        auto color8 = themeManager->getMultiTrackWaveformColor(8);
        auto color16 = themeManager->getMultiTrackWaveformColor(16);
        auto color32 = themeManager->getMultiTrackWaveformColor(32);

        // Colors should be different due to variations
        REQUIRE(color0 != color8);
        REQUIRE(color8 != color16);
        REQUIRE(color16 != color32);

        // Base colors should cycle (0 and 8 should have same base)
        auto baseColor0 = themeManager->getWaveformColor(0);
        auto baseColor8 = themeManager->getWaveformColor(8);
        REQUIRE(baseColor0 == baseColor8); // Same base color

        // But multi-track variants should differ
        REQUIRE(color0 != color8); // Different due to brightness/saturation variations
    }
}

TEST_CASE_METHOD(MultiTrackRenderingTestFixture, "Multi-track audio processing integration", "[multitrack][audio]") {
    SECTION("Multi-track engine provides data for rendering") {
        const int numTracks = 16;
        addTestTracks(numTracks);

        // Simulate audio input
        simulateAudioInput(numTracks, 512);

        // Get data from bridge
        auto& bridge = processor.getWaveformDataBridge();
        audio::AudioDataSnapshot snapshot;

        bool hasData = bridge.pullLatestData(snapshot);
        REQUIRE(hasData);
        REQUIRE(snapshot.numChannels == numTracks);
        REQUIRE(snapshot.numSamples > 0);

        // Verify we have actual audio data
        bool hasNonZeroData = false;
        for (size_t ch = 0; ch < snapshot.numChannels; ++ch) {
            for (size_t sample = 0; sample < snapshot.numSamples; ++sample) {
                if (std::abs(snapshot.samples[ch][sample]) > 0.001f) {
                    hasNonZeroData = true;
                    break;
                }
            }
        }
        REQUIRE(hasNonZeroData);
    }
}

TEST_CASE_METHOD(MultiTrackRenderingTestFixture, "Multi-track rendering performance", "[multitrack][performance]") {
    SECTION("Performance with maximum track count") {
        const int maxTracks = 64;
        addTestTracks(maxTracks);

        // Set component size for testing
        component->setBounds(0, 0, 800, 600);

        // Simulate multiple audio blocks
        for (int block = 0; block < 10; ++block) {
            simulateAudioInput(maxTracks, 512);
        }

        // Benchmark rendering performance
        BENCHMARK("64-track rendering") {
            // Create graphics context for testing
            Image testImage(Image::ARGB, 800, 600, true);
            Graphics graphics(testImage);

            component->paint(graphics);
            return graphics;
        };

        // Check performance statistics
        auto stats = component->getPerformanceStats();
        INFO("Average frame time: " << stats.averageMs << "ms");
        INFO("Max frame time: " << stats.maxMs << "ms");

        // Performance requirements from Task 3.2
        REQUIRE(stats.averageMs < 16.67); // Target: 60 FPS (16.67ms per frame)
        REQUIRE(stats.maxMs < 33.33);     // Acceptable: 30 FPS (33.33ms per frame)
    }

    SECTION("Visibility toggle performance") {
        const int numTracks = 32;
        addTestTracks(numTracks);
        component->setBounds(0, 0, 800, 600);

        // Benchmark visibility toggle
        BENCHMARK("Visibility toggle latency") {
            component->setTrackVisible(0, false);
            component->setTrackVisible(0, true);
            return component->isTrackVisible(0);
        };

        // Visibility toggle should be immediate (<1ms)
        auto start = std::chrono::high_resolution_clock::now();
        component->setTrackVisible(15, false);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        REQUIRE(duration.count() < 1000); // < 1ms = 1000 microseconds
    }
}

TEST_CASE_METHOD(MultiTrackRenderingTestFixture, "Backward compatibility", "[multitrack][compatibility]") {
    SECTION("Single track rendering still works") {
        const int singleTrack = 1;
        addTestTracks(singleTrack);

        simulateAudioInput(singleTrack, 512);

        // Should work exactly as before
        auto& bridge = processor.getWaveformDataBridge();
        audio::AudioDataSnapshot snapshot;

        bool hasData = bridge.pullLatestData(snapshot);
        REQUIRE(hasData);
        REQUIRE(snapshot.numChannels == singleTrack);

        // Rendering should work without errors
        component->setBounds(0, 0, 400, 300);
        Image testImage(Image::ARGB, 400, 300, true);
        Graphics graphics(testImage);

        REQUIRE_NOTHROW(component->paint(graphics));
    }

    SECTION("Two channel stereo rendering works") {
        const int stereoTracks = 2;
        addTestTracks(stereoTracks);

        simulateAudioInput(stereoTracks, 256);

        auto& bridge = processor.getWaveformDataBridge();
        audio::AudioDataSnapshot snapshot;

        bool hasData = bridge.pullLatestData(snapshot);
        REQUIRE(hasData);
        REQUIRE(snapshot.numChannels == stereoTracks);

        // Both tracks should be visible by default
        REQUIRE(component->isTrackVisible(0));
        REQUIRE(component->isTrackVisible(1));
    }
}

TEST_CASE_METHOD(MultiTrackRenderingTestFixture, "OpenGL integration compatibility", "[multitrack][opengl]") {
    SECTION("Multi-track rendering works with OpenGL disabled") {
        // This tests the CPU-only path which should always work
        const int numTracks = 8;
        addTestTracks(numTracks);
        component->setBounds(0, 0, 600, 400);

        simulateAudioInput(numTracks, 256);

        // Rendering should work without OpenGL
        Image testImage(Image::ARGB, 600, 400, true);
        Graphics graphics(testImage);

        REQUIRE_NOTHROW(component->paint(graphics));

        // Verify performance is still good without GPU acceleration
        auto stats = component->getPerformanceStats();
        REQUIRE(stats.averageMs < 20.0); // Slightly more lenient for CPU-only
    }
}

TEST_CASE("Multi-track color system validation", "[multitrack][theme]") {
    SECTION("Color accessibility for 64 tracks") {
        auto themeManager = std::make_unique<oscil::theme::ThemeManager>();

        // Test that all 64 colors are accessible
        std::vector<juce::Colour> colors;
        for (int i = 0; i < 64; ++i) {
            colors.push_back(themeManager->getMultiTrackWaveformColor(i));
        }

        // Check color distinctness (no identical colors)
        for (size_t i = 0; i < colors.size(); ++i) {
            for (size_t j = i + 1; j < colors.size(); ++j) {
                // Colors should be different enough to distinguish
                auto colorDiff = std::abs(colors[i].getArgb() - colors[j].getArgb());
                REQUIRE(colorDiff > 0x101010); // Minimum difference threshold
            }
        }

        // Verify base color cycling
        auto color0 = themeManager->getMultiTrackWaveformColor(0);
        auto color8 = themeManager->getMultiTrackWaveformColor(8);
        auto color56 = themeManager->getMultiTrackWaveformColor(56); // 56 % 8 = 0

        // They should share the same base hue but differ in brightness/saturation
        REQUIRE(color0 != color8);
        REQUIRE(color0 != color56);
        REQUIRE(color8 != color56);
    }
}
