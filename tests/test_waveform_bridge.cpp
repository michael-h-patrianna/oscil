#include <catch2/catch_test_macros.hpp>

#include "../src/audio/WaveformDataBridge.h"
#include <thread>
#include <chrono>

using namespace audio;

TEST_CASE("WaveformDataBridge basic functionality", "[WaveformDataBridge]") {
    WaveformDataBridge bridge;

    SECTION("Initial state") {
        REQUIRE(bridge.getTotalFramesPushed() == 0);
        REQUIRE(bridge.getTotalFramesPulled() == 0);

        AudioDataSnapshot snapshot;
        REQUIRE_FALSE(bridge.pullLatestData(snapshot));
    }

    SECTION("Push and pull single frame") {
        // Prepare test data
        constexpr size_t numChannels = 2;
        constexpr size_t numSamples = 64;
        constexpr double sampleRate = 44100.0;

        float leftChannel[numSamples];
        float rightChannel[numSamples];

        // Fill with test signal
        for (size_t i = 0; i < numSamples; ++i) {
            leftChannel[i] = static_cast<float>(i) / numSamples; // Ramp 0-1
            rightChannel[i] = -static_cast<float>(i) / numSamples; // Ramp 0-(-1)
        }

        const float* channelPointers[] = { leftChannel, rightChannel };

        // Push data
        bridge.pushAudioData(channelPointers, numChannels, numSamples, sampleRate);

        REQUIRE(bridge.getTotalFramesPushed() == 1);

        // Pull data
        AudioDataSnapshot snapshot;
        REQUIRE(bridge.pullLatestData(snapshot));

        REQUIRE(bridge.getTotalFramesPulled() == 1);
        REQUIRE(snapshot.numChannels == numChannels);
        REQUIRE(snapshot.numSamples == numSamples);
        REQUIRE(snapshot.sampleRate == sampleRate);

        // Verify sample data
        for (size_t i = 0; i < numSamples; ++i) {
            REQUIRE(snapshot.samples[0][i] == leftChannel[i]);
            REQUIRE(snapshot.samples[1][i] == rightChannel[i]);
        }

        // Second pull should return false (no new data)
        AudioDataSnapshot snapshot2;
        REQUIRE_FALSE(bridge.pullLatestData(snapshot2));
    }

    SECTION("Overwrite behavior") {
        constexpr size_t numChannels = 1;
        constexpr size_t numSamples = 16;
        constexpr double sampleRate = 48000.0;

        float testData1[numSamples];
        float testData2[numSamples];

        // Fill with different patterns
        for (size_t i = 0; i < numSamples; ++i) {
            testData1[i] = 1.0f;
            testData2[i] = -1.0f;
        }

        const float* channels1[] = { testData1 };
        const float* channels2[] = { testData2 };

        // Push first frame
        bridge.pushAudioData(channels1, numChannels, numSamples, sampleRate);

        // Push second frame before pulling (should overwrite)
        bridge.pushAudioData(channels2, numChannels, numSamples, sampleRate);

        // Should only get the second frame
        AudioDataSnapshot snapshot;
        REQUIRE(bridge.pullLatestData(snapshot));

        // Verify we got the second data
        for (size_t i = 0; i < numSamples; ++i) {
            REQUIRE(snapshot.samples[0][i] == -1.0f);
        }

        REQUIRE(bridge.getTotalFramesPushed() == 2); // Two push operations occurred
    }
}

TEST_CASE("WaveformDataBridge thread safety", "[WaveformDataBridge][threading]") {
    WaveformDataBridge bridge;

    constexpr size_t numChannels = 2;
    constexpr size_t numSamples = 128;
    constexpr double sampleRate = 44100.0;
    constexpr int numFrames = 100;

    std::atomic<bool> audioThreadRunning{true};
    std::atomic<int> framesPushedByAudio{0};
    std::atomic<int> framesPulledByUI{0};

    // Audio thread simulation
    std::thread audioThread([&]() {
        float testData[numChannels][numSamples];
        const float* channelPointers[numChannels];

        for (size_t ch = 0; ch < numChannels; ++ch) {
            channelPointers[ch] = testData[ch];
        }

        for (int frame = 0; frame < numFrames && audioThreadRunning.load(); ++frame) {
            // Fill with frame-specific data
            for (size_t ch = 0; ch < numChannels; ++ch) {
                for (size_t i = 0; i < numSamples; ++i) {
                    testData[ch][i] = static_cast<float>(frame) + static_cast<float>(ch);
                }
            }

            bridge.pushAudioData(channelPointers, numChannels, numSamples, sampleRate);
            framesPushedByAudio.fetch_add(1);

            // Simulate audio callback timing
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // UI thread simulation
    std::thread uiThread([&]() {
        AudioDataSnapshot snapshot;

        while (audioThreadRunning.load() || framesPulledByUI.load() < numFrames) {
            if (bridge.pullLatestData(snapshot)) {
                framesPulledByUI.fetch_add(1);

                // Verify data integrity
                REQUIRE(snapshot.numChannels == numChannels);
                REQUIRE(snapshot.numSamples == numSamples);
                REQUIRE(snapshot.sampleRate == sampleRate);
            }

            // Simulate UI refresh timing (slower than audio)
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    });

    // Wait for audio thread to complete
    audioThread.join();
    audioThreadRunning.store(false);

    // Wait for UI thread to complete
    uiThread.join();

    // Verify thread safety - no data corruption or crashes
    REQUIRE(framesPushedByAudio.load() == numFrames);
    REQUIRE(framesPulledByUI.load() <= framesPushedByAudio.load()); // UI may miss some frames
    REQUIRE(framesPulledByUI.load() > 0); // But should get some data
}

TEST_CASE("WaveformDataBridge edge cases", "[WaveformDataBridge]") {
    WaveformDataBridge bridge;

    SECTION("Null channel data") {
        bridge.pushAudioData(nullptr, 2, 64, 44100.0);

        AudioDataSnapshot snapshot;
        REQUIRE_FALSE(bridge.pullLatestData(snapshot));
        REQUIRE(bridge.getTotalFramesPushed() == 0);
    }

    SECTION("Zero channels") {
        float dummyData[64];
        const float* channels[] = { dummyData };

        bridge.pushAudioData(channels, 0, 64, 44100.0);

        AudioDataSnapshot snapshot;
        // Should still work but with zero channels
        if (bridge.pullLatestData(snapshot)) {
            REQUIRE(snapshot.numChannels == 0);
        }
    }

    SECTION("Maximum channels and samples") {
        constexpr size_t maxChannels = AudioDataSnapshot::MAX_CHANNELS;
        constexpr size_t maxSamples = AudioDataSnapshot::MAX_SAMPLES;

        std::vector<std::vector<float>> channelData(maxChannels + 10); // Exceed max
        std::vector<const float*> channelPointers;

        for (size_t ch = 0; ch < maxChannels + 10; ++ch) {
            channelData[ch].resize(maxSamples + 100); // Exceed max samples too
            for (size_t i = 0; i < maxSamples + 100; ++i) {
                channelData[ch][i] = static_cast<float>(ch * 1000 + i);
            }
            channelPointers.push_back(channelData[ch].data());
        }

        bridge.pushAudioData(channelPointers.data(), maxChannels + 10, maxSamples + 100, 96000.0);

        AudioDataSnapshot snapshot;
        REQUIRE(bridge.pullLatestData(snapshot));

        // Should be clamped to maximums
        REQUIRE(snapshot.numChannels == maxChannels);
        REQUIRE(snapshot.numSamples == maxSamples);

        // Verify sample data integrity within limits
        for (size_t ch = 0; ch < maxChannels; ++ch) {
            for (size_t i = 0; i < maxSamples; ++i) {
                float expected = static_cast<float>(ch * 1000 + i);
                REQUIRE(snapshot.samples[ch][i] == expected);
            }
        }
    }
}

TEST_CASE("WaveformDataBridge performance", "[WaveformDataBridge][performance]") {
    WaveformDataBridge bridge;

    constexpr size_t numChannels = 8;
    constexpr size_t numSamples = 512;
    constexpr double sampleRate = 192000.0;
    constexpr int numIterations = 1000;

    // Prepare test data
    std::vector<std::vector<float>> channelData(numChannels);
    std::vector<const float*> channelPointers;

    for (size_t ch = 0; ch < numChannels; ++ch) {
        channelData[ch].resize(numSamples);
        for (size_t i = 0; i < numSamples; ++i) {
            channelData[ch][i] = static_cast<float>(std::sin(2.0 * M_PI * i / numSamples));
        }
        channelPointers.push_back(channelData[ch].data());
    }

    // Measure push performance
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numIterations; ++i) {
        bridge.pushAudioData(channelPointers.data(), numChannels, numSamples, sampleRate);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto pushDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    // Should be very fast - target <100µs per operation for real-time safety
    double avgPushTime = static_cast<double>(pushDuration.count()) / numIterations;
    INFO("Average push time: " << avgPushTime << " µs");
    REQUIRE(avgPushTime < 100.0); // Should be much faster than this

    // Measure pull performance
    AudioDataSnapshot snapshot;
    startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numIterations; ++i) {
        bridge.pullLatestData(snapshot);
    }

    endTime = std::chrono::high_resolution_clock::now();
    auto pullDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    double avgPullTime = static_cast<double>(pullDuration.count()) / numIterations;
    INFO("Average pull time: " << avgPullTime << " µs");
    REQUIRE(avgPullTime < 1000.0); // UI thread has more time budget
}
