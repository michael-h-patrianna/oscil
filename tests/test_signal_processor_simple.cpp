#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/audio/SignalProcessor.h"
#include <array>

using namespace audio;
using Catch::Matchers::WithinAbs;

TEST_CASE("SignalProcessor - Basic Construction", "[SignalProcessor]") {
    SignalProcessor processor;
    auto config = processor.getConfig();

    REQUIRE(config.mode == SignalProcessingMode::FullStereo);
    REQUIRE(config.enableCorrelation == true);
}

TEST_CASE("SignalProcessor - Mode Configuration", "[SignalProcessor]") {
    SignalProcessor processor;

    processor.setProcessingMode(SignalProcessingMode::MonoSum);
    REQUIRE(processor.getProcessingMode() == SignalProcessingMode::MonoSum);

    processor.setProcessingMode(SignalProcessingMode::MidSide);
    REQUIRE(processor.getProcessingMode() == SignalProcessingMode::MidSide);
}

TEST_CASE("SignalProcessor - Processing Mode Channel Counts", "[SignalProcessor]") {
    REQUIRE(getOutputChannelCount(SignalProcessingMode::FullStereo) == 2);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::MidSide) == 2);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::MonoSum) == 1);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::LeftOnly) == 1);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::RightOnly) == 1);
    REQUIRE(getOutputChannelCount(SignalProcessingMode::Difference) == 1);
}

TEST_CASE("SignalProcessor - Full Stereo Mode", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::FullStereo);

    constexpr size_t numSamples = 8;
    std::array<float, numSamples> leftInput = {0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
    std::array<float, numSamples> rightInput = {0.8F, 0.7F, 0.6F, 0.5F, 0.4F, 0.3F, 0.2F, 0.1F};

    SignalProcessor::ProcessedOutput output;
    processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

    REQUIRE(output.numOutputChannels == 2);
    REQUIRE(output.numSamples == numSamples);

    // Verify pass-through behavior
    for (size_t i = 0; i < numSamples; ++i) {
        REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(leftInput[i], 0.0001F));
        REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(rightInput[i], 0.0001F));
    }
}

TEST_CASE("SignalProcessor - Mono Sum Mode", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::MonoSum);

    constexpr size_t numSamples = 4;
    std::array<float, numSamples> leftInput = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, numSamples> rightInput = {-1.0F, -1.0F, -1.0F, -1.0F};

    SignalProcessor::ProcessedOutput output;
    processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

    REQUIRE(output.numOutputChannels == 1);
    REQUIRE(output.numSamples == numSamples);

    // (1 + (-1)) / 2 = 0
    for (size_t i = 0; i < numSamples; ++i) {
        REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.0F, 0.0001F));
    }
}

TEST_CASE("SignalProcessor - Mid/Side Mode Mathematical Precision", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::MidSide);

    SECTION("Mono content") {
        constexpr size_t numSamples = 4;
        std::array<float, numSamples> leftInput = {0.5F, 0.5F, 0.5F, 0.5F};
        std::array<float, numSamples> rightInput = {0.5F, 0.5F, 0.5F, 0.5F};

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        REQUIRE(output.numOutputChannels == 2);
        REQUIRE(output.numSamples == numSamples);

        for (size_t i = 0; i < numSamples; ++i) {
            // Mid = (L + R) / 2 = (0.5 + 0.5) / 2 = 0.5
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.5F, 0.001F));
            // Side = (L - R) / 2 = (0.5 - 0.5) / 2 = 0
            REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(0.0F, 0.001F));
        }
    }

    SECTION("Stereo content precision") {
        constexpr size_t numSamples = 4;
        std::array<float, numSamples> leftInput = {0.8F, 0.8F, 0.8F, 0.8F};
        std::array<float, numSamples> rightInput = {0.4F, 0.4F, 0.4F, 0.4F};

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        for (size_t i = 0; i < numSamples; ++i) {
            // M = (0.8 + 0.4) / 2 = 0.6
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.6F, 0.001F));
            // S = (0.8 - 0.4) / 2 = 0.2
            REQUIRE_THAT(output.outputChannels[1][i], WithinAbs(0.2F, 0.001F));
        }
    }
}

TEST_CASE("SignalProcessor - Left and Right Only Modes", "[SignalProcessor]") {
    SignalProcessor processor;

    constexpr size_t numSamples = 4;
    std::array<float, numSamples> leftInput = {0.75F, 0.75F, 0.75F, 0.75F};
    std::array<float, numSamples> rightInput = {0.25F, 0.25F, 0.25F, 0.25F};

    SECTION("Left Only Mode") {
        processor.setProcessingMode(SignalProcessingMode::LeftOnly);

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        REQUIRE(output.numOutputChannels == 1);
        REQUIRE(output.numSamples == numSamples);

        for (size_t i = 0; i < numSamples; ++i) {
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.75F, 0.0001F));
        }
    }

    SECTION("Right Only Mode") {
        processor.setProcessingMode(SignalProcessingMode::RightOnly);

        SignalProcessor::ProcessedOutput output;
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

        REQUIRE(output.numOutputChannels == 1);
        REQUIRE(output.numSamples == numSamples);

        for (size_t i = 0; i < numSamples; ++i) {
            REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.25F, 0.0001F));
        }
    }
}

TEST_CASE("SignalProcessor - Difference Mode", "[SignalProcessor]") {
    SignalProcessor processor;
    processor.setProcessingMode(SignalProcessingMode::Difference);

    constexpr size_t numSamples = 4;
    std::array<float, numSamples> leftInput = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, numSamples> rightInput = {0.3F, 0.3F, 0.3F, 0.3F};

    SignalProcessor::ProcessedOutput output;
    processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);

    REQUIRE(output.numOutputChannels == 1);
    REQUIRE(output.numSamples == numSamples);

    // L - R = 1.0 - 0.3 = 0.7
    for (size_t i = 0; i < numSamples; ++i) {
        REQUIRE_THAT(output.outputChannels[0][i], WithinAbs(0.7F, 0.0001F));
    }
}

TEST_CASE("SignalProcessor - Performance Requirements", "[SignalProcessor]") {
    SignalProcessor processor;

    SECTION("Mode switching without interruption") {
        constexpr size_t numSamples = 8;
        std::array<float, numSamples> leftInput = {0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.5F};
        std::array<float, numSamples> rightInput = {-0.5F, -0.5F, -0.5F, -0.5F, -0.5F, -0.5F, -0.5F, -0.5F};

        SignalProcessor::ProcessedOutput output;

        // Process in Full Stereo mode
        processor.setProcessingMode(SignalProcessingMode::FullStereo);
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        REQUIRE(output.numOutputChannels == 2);

        // Switch to Mid/Side mode and process immediately
        processor.setProcessingMode(SignalProcessingMode::MidSide);
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        REQUIRE(output.numOutputChannels == 2);
        REQUIRE_THAT(output.outputChannels[0][0], WithinAbs(0.0F, 0.001F)); // Mid
        REQUIRE_THAT(output.outputChannels[1][0], WithinAbs(0.5F, 0.001F)); // Side

        // Switch to Mono Sum and verify
        processor.setProcessingMode(SignalProcessingMode::MonoSum);
        processor.processBlock(leftInput.data(), rightInput.data(), numSamples, output);
        REQUIRE(output.numOutputChannels == 1);
        REQUIRE_THAT(output.outputChannels[0][0], WithinAbs(0.0F, 0.001F)); // (0.5 + (-0.5))/2
    }

    SECTION("Statistics tracking") {
        SignalProcessor statsProcessor;
        const auto& initialStats = statsProcessor.getPerformanceStats();

        REQUIRE(initialStats.blocksProcessed.load() == 0);
        REQUIRE(initialStats.totalSamplesProcessed.load() == 0);
        REQUIRE(initialStats.modeChanges.load() == 0);

        // Process some blocks
        constexpr size_t numSamples = 8;
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
