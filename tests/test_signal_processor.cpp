#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/audio/SignalProcessor.h"
#include <cmath>
#include <array>

namespace {
    constexpr float PI_F = 3.14159265358979323846f;
}

using namespace audio;
using Catch::Matchers::WithinAbs;

TEST_CASE("SignalProcessor - Basic Construction and Configuration", "[SignalProcessor]") {
    SECTION("Default construction") {
        SignalProcessor processor;
        auto config = processor.getConfig();

        REQUIRE(config.mode == SignalProcessingMode::FullStereo);
        REQUIRE(config.enableCorrelation == true);
        REQUIRE(config.useDoublePrec == true);
    }

    SECTION("Construction with config") {
        ProcessingConfig config(SignalProcessingMode::MidSide);
        config.enableCorrelation = false;

        SignalProcessor processor(config);
        auto retrievedConfig = processor.getConfig();

        REQUIRE(retrievedConfig.mode == SignalProcessingMode::MidSide);
        REQUIRE(retrievedConfig.enableCorrelation == false);
    }

    SECTION("Mode switching") {
        SignalProcessor processor;

        processor.setProcessingMode(SignalProcessingMode::MonoSum);
        REQUIRE(processor.getProcessingMode() == SignalProcessingMode::MonoSum);

        processor.setProcessingMode(SignalProcessingMode::Difference);
        REQUIRE(processor.getProcessingMode() == SignalProcessingMode::Difference);
    }
}

TEST_CASE("SignalProcessor - Processing Mode Output Channel Counts", "[SignalProcessor]") {
    REQUIRE(getOutputChannelCount(SignalProcessingMode::FullStereo) == 2);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::MidSide) == 2);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::MonoSum) == 1);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::LeftOnly) == 1);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::RightOnly) == 1);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::Difference) == 1);
}

TEST_CASE("SignalProcessor - Processing Mode Names", "[SignalProcessor]") {
    REQUIRE(std::string(getProcessingModeName(SignalProcessingMode::FullStereo)) == "Full Stereo");
    REQUIRE(std::string(getProcessingModeName(SignalProcessingMode::MonoSum)) == "Mono Sum");
    REQUIRE(std::string(getProcessingModeName(SignalProcessingMode::MidSide)) == "Mid/Side");
    REQUIRE(std::string(getProcessingModeName(SignalProcessingMode::LeftOnly)) == "Left Only");
    REQUIRE(std::string(getProcessingModeName(SignalProcessingMode::RightOnly)) == "Right Only");
    REQUIRE(std::string(getProcessingModeName(SignalProcessingMode::Difference)) == "Difference");
}

TEST_CASE("SignalProcessor - Full Stereo Mode", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::FullStereo);

    // Test data: sine wave on left, cosine wave on right
    constexpr size_t numSamples = 64;
    std::array<float, numSamples> leftInput;
    std::array<float, numSamples> rightInput;

    for (size_t i = 0; i < numSamples; ++i) {
        float phase = static_cast<float>(i) * 2.0f * PI_F / numSamples;
        leftInput[i] = std::sin(phase);
        rightInput[i] = std::cos(phase);
    }

    SignalProcessor::ProcessedOutput output;
    processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

    REQUIRE(output.numOutputChannels == 2);
    REQUIRE(output.numSamples == numSamples);

    // Verify pass-through behavior
    for (size_t i = 0; i < numSamples; ++i) {
        REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(leftInput[i], 0.0001f));
        REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(rightInput[i], 0.0001f));
    }
}

TEST_CASE("SignalProcessor - Mono Sum Mode", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::MonoSum);

    // Test data
    constexpr size_t numSamples = 64;
    std::array<float, numSamples> leftInput;
    std::array<float, numSamples> rightInput;

    for (size_t i = 0; i < numSamples; ++i) {
        leftInput[i] = 1.0f;  // DC signal
        rightInput[i] = -1.0f; // Inverted DC signal
    }

    SignalProcessor::ProcessedOutput output;
    processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

    REQUIRE(output.numOutputChannels == 1);
    REQUIRE(output.numSamples == numSamples);

    // (1 + (-1)) / 2 = 0
    for (size_t i = 0; i < numSamples; ++i) {
        REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.0f, 0.0001f));
    }
}

TEST_CASE("SignalProcessor - Mid/Side Mode Mathematical Precision", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::MidSide);

    SECTION("Mono content - Mid channel should contain signal, Side channel should be silent") {
        constexpr size_t numSamples = 64;
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        // Identical signals on both channels (mono content)
        for (size_t i = 0; i < numSamples; ++i) {
            float value = std::sin(static_cast<float>(i) * 2.0f * M_PI / numSamples);
            leftInput[i] = value;
            rightInput[i] = value;
        }

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        REQUIRE(output.numOutputChannels == 2);
        REQUIRE(output.numSamples == numSamples);

        for (size_t i = 0; i < numSamples; ++i) {
            // Mid = (L + R) / 2 = (signal + signal) / 2 = signal
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(leftInput[i], 0.001f));
            // Side = (L - R) / 2 = (signal - signal) / 2 = 0
            REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(0.0f, 0.001f));
        }
    }

    SECTION("Stereo content - Mathematical precision test") {
        constexpr size_t numSamples = 64;
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        for (size_t i = 0; i < numSamples; ++i) {
            leftInput[i] = 0.8f;   // Different values for stereo content
            rightInput[i] = 0.4f;
        }

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        for (size_t i = 0; i < numSamples; ++i) {
            // M = (0.8 + 0.4) / 2 = 0.6
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.6f, 0.001f));
            // S = (0.8 - 0.4) / 2 = 0.2
            REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(0.2f, 0.001f));
        }
    }

    SECTION("Precision tolerance requirement: ±0.001") {
        // Test with values that would challenge numerical precision
        constexpr size_t numSamples = 8;
        std::array<float, numSamples> leftInput = {1.0f, -1.0f, 0.5f, -0.5f, 0.001f, -0.001f, 0.999f, -0.999f};
        std::array<float, numSamples> rightInput = {-1.0f, 1.0f, -0.5f, 0.5f, -0.001f, 0.001f, -0.999f, 0.999f};

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        for (size_t i = 0; i < numSamples; ++i) {
            float expectedMid = (leftInput[i] + rightInput[i]) * 0.5f;
            float expectedSide = (leftInput[i] - rightInput[i]) * 0.5f;

            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(expectedMid, 0.001f));
            REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(expectedSide, 0.001f));
        }
    }
}

TEST_CASE("SignalProcessor - Left and Right Only Modes", "[SignalProcessor]") {
    SignalProcessor processor;

    constexpr size_t numSamples = 64;
    std::array<float, numSamples> leftInput;
    std::array<float, numSamples> rightInput;

    for (size_t i = 0; i < numSamples; ++i) {
        leftInput[i] = 0.75f;
        rightInput[i] = 0.25f;
    }

    SECTION("Left Only Mode") {
        processor.setProcessingMode(SignalProcessingMode::LeftOnly);

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        REQUIRE(output.numOutputChannels == 1);
        REQUIRE(output.numSamples == numSamples);

        for (size_t i = 0; i < numSamples; ++i) {
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.75f, 0.0001f));
        }
    }

    SECTION("Right Only Mode") {
        processor.setProcessingMode(SignalProcessingMode::RightOnly);

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        REQUIRE(output.numOutputChannels == 1);
        REQUIRE(output.numSamples == numSamples);

        for (size_t i = 0; i < numSamples; ++i) {
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.25f, 0.0001f));
        }
    }
}

TEST_CASE("SignalProcessor - Difference Mode", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::Difference);

    constexpr size_t numSamples = 64;
    std::array<float, numSamples> leftInput;
    std::array<float, numSamples> rightInput;

    for (size_t i = 0; i < numSamples; ++i) {
        leftInput[i] = 1.0f;
        rightInput[i] = 0.3f;
    }

    SignalProcessor::ProcessedOutput output;
    processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

    REQUIRE(output.numOutputChannels == 1);
    REQUIRE(output.numSamples == numSamples);

    // L - R = 1.0 - 0.3 = 0.7
    for (size_t i = 0; i < numSamples; ++i) {
        REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.7f, 0.0001f));
    }
}

TEST_CASE("SignalProcessor - Correlation Coefficient Calculation", "[SignalProcessor]") {
    SignalProcessor processor;

    // Enable correlation with small window for testing
    ProcessingConfig config;
    config.enableCorrelation = true;
    config.correlationWindowSize = 64;
    processor.setConfig(config);

    constexpr size_t numSamples = 64;

    SECTION("Identical signals - correlation should be 1.0") {
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        for (size_t i = 0; i < numSamples; ++i) {
            float value = std::sin(static_cast<float>(i) * 2.0f * M_PI / numSamples);
            leftInput[i] = value;
            rightInput[i] = value;
        }

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        if (output.metricsValid) {
            REQUIRE_THAT(output.metrics.correlation, WithinAbs(1.0f, 0.01f));
        }
    }

    SECTION("Inverted signals - correlation should be -1.0") {
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        for (size_t i = 0; i < numSamples; ++i) {
            float value = std::sin(static_cast<float>(i) * 2.0f * M_PI / numSamples);
            leftInput[i] = value;
            rightInput[i] = -value;
        }

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        if (output.metricsValid) {
            REQUIRE_THAT(output.metrics.correlation, WithinAbs(-1.0f, 0.01f));
        }
    }

    SECTION("Uncorrelated signals - correlation should be ~0.0") {
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        for (size_t i = 0; i < numSamples; ++i) {
            leftInput[i] = std::sin(static_cast<float>(i) * 2.0f * M_PI / numSamples);
            rightInput[i] = std::sin(static_cast<float>(i) * 4.0f * M_PI / numSamples + M_PI/2);
        }

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        if (output.metricsValid) {
            REQUIRE_THAT(output.metrics.correlation, WithinAbs(0.0f, 0.1f));
        }
    }
}

TEST_CASE("SignalProcessor - Performance and Threading Requirements", "[SignalProcessor]") {
    SignalProcessor processor;

    SECTION("Processing latency measurement") {
        constexpr size_t numSamples = 128; // Typical audio buffer size
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        // Generate test signal
        for (size_t i = 0; i < numSamples; ++i) {
            leftInput[i] = std::sin(static_cast<float>(i) * 2.0f * M_PI / numSamples);
            rightInput[i] = std::cos(static_cast<float>(i) * 2.0f * M_PI / numSamples);
        }

        // Process multiple blocks to get average
        SignalProcessor::ProcessedOutput output;

        auto start = std::chrono::high_resolution_clock::now();
        for (int block = 0; block < 100; ++block) {
            processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        float avgTimePerBlockMs = (duration.count() / 100.0f) / 1000.0f;

        // Processing latency target: ≤2ms per block
        REQUIRE(avgTimePerBlockMs <= 2.0f);

        const auto& stats = processor.getPerformanceStats();
        REQUIRE(stats.blocksProcessed.load() >= 100);
    }

    SECTION("Mode switching without interruption") {
        constexpr size_t numSamples = 64;
        std::array<float, numSamples> leftInput;
        std::array<float, numSamples> rightInput;

        for (size_t i = 0; i < numSamples; ++i) {
            leftInput[i] = 0.5f;
            rightInput[i] = -0.5f;
        }

        SignalProcessor::ProcessedOutput output;

        // Process in Full Stereo mode
        processor.setProcessingMode(SignalProcessingMode::FullStereo);
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        REQUIRE(output.numOutputChannels == 2);

        // Switch to Mid/Side mode and process immediately
        processor.setProcessingMode(SignalProcessingMode::MidSide);
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        REQUIRE(output.numOutputChannels == 2);
        REQUIRE_THAT(output.outputChannels[0][0], WithinAbs(0.0f, 0.001f)); // Mid
        REQUIRE_THAT(output.outputChannels[1][0], WithinAbs(0.5f, 0.001f)); // Side

        // Switch to Mono Sum and verify
        processor.setProcessingMode(SignalProcessingMode::MonoSum);
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        REQUIRE(output.numOutputChannels == 1);
        REQUIRE_THAT(output.outputChannels[0][0], WithinAbs(0.0f, 0.001f)); // (0.5 + (-0.5))/2
    }

    SECTION("Statistics tracking") {
        SignalProcessor statsProcessor;
        const auto& initialStats = statsProcessor.getPerformanceStats();

        REQUIRE(initialStats.blocksProcessed.load() == 0);
        REQUIRE(initialStats.totalSamplesProcessed.load() == 0);
        REQUIRE(initialStats.modeChanges.load() == 0);

        // Process some blocks
        constexpr size_t numSamples = 64;
        std::array<float, numSamples> leftInput{};
        std::array<float, numSamples> rightInput{};

        SignalProcessor::ProcessedOutput output;
        statsProcessor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        statsProcessor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        // Change mode
        statsProcessor.setProcessingMode(SignalProcessingMode::MidSide);

        const auto& finalStats = statsProcessor.getPerformanceStats();
        REQUIRE(finalStats.blocksProcessed.load() == 2);
        REQUIRE(finalStats.totalSamplesProcessed.load() == numSamples * 2);
        REQUIRE(finalStats.modeChanges.load() == 1);

        // Reset stats
        statsProcessor.resetStats();
        const auto& resetStats = statsProcessor.getPerformanceStats();
        REQUIRE(resetStats.blocksProcessed.load() == 0);
        REQUIRE(resetStats.totalSamplesProcessed.load() == 0);
        REQUIRE(resetStats.modeChanges.load() == 0);
    }
}
