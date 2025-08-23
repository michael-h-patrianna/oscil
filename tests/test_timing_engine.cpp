#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/timing/TimingEngine.h"
#include <array>
#include <thread>

using namespace oscil::timing;
using Catch::Matchers::WithinAbs;

TEST_CASE("TimingEngine - Basic Construction", "[TimingEngine]") {
    TimingEngine engine;

    // Default timing mode should be Free Running
    REQUIRE(engine.getTimingState().currentMode == TimingMode::FreeRunning);

    // Performance stats should be initialized
    const auto& stats = engine.getPerformanceStats();
    REQUIRE(stats.processBlockCalls.load() == 0);
    REQUIRE(stats.triggerDetections.load() == 0);
    REQUIRE(stats.modeChanges.load() == 0);
}

TEST_CASE("TimingEngine - Timing Mode Switching", "[TimingEngine]") {
    TimingEngine engine;

    SECTION("Free Running to Host Sync") {
        engine.setTimingMode(TimingMode::HostSync);
        REQUIRE(engine.getTimingState().currentMode == TimingMode::HostSync);

        const auto& stats = engine.getPerformanceStats();
        REQUIRE(stats.modeChanges.load() == 1);
    }

    SECTION("Multiple mode changes") {
        engine.setTimingMode(TimingMode::Musical);
        engine.setTimingMode(TimingMode::Trigger);
        engine.setTimingMode(TimingMode::TimeBased);

        REQUIRE(engine.getTimingState().currentMode == TimingMode::TimeBased);

        const auto& stats = engine.getPerformanceStats();
        REQUIRE(stats.modeChanges.load() == 3);
    }
}

TEST_CASE("TimingEngine - Configuration Management", "[TimingEngine]") {
    TimingEngine engine;

    SECTION("Trigger Configuration") {
        TriggerConfig config;
        config.type = TriggerType::Level;
        config.edge = TriggerEdge::Rising;
        config.threshold = 0.7F;
        config.hysteresis = 0.15F;

        engine.setTriggerConfig(config);

        const auto& retrievedConfig = engine.getTriggerConfig();
        REQUIRE(retrievedConfig.type == TriggerType::Level);
        REQUIRE(retrievedConfig.edge == TriggerEdge::Rising);
        REQUIRE_THAT(retrievedConfig.threshold, WithinAbs(0.7F, 0.001F));
        REQUIRE_THAT(retrievedConfig.hysteresis, WithinAbs(0.15F, 0.001F));
    }

    SECTION("Musical Configuration") {
        MusicalConfig config;
        config.beatDivision = 8;
        config.customBPM = 140.0;
        config.followTempoChanges = false;

        engine.setMusicalConfig(config);

        const auto& retrievedConfig = engine.getMusicalConfig();
        REQUIRE(retrievedConfig.beatDivision == 8);
        REQUIRE_THAT(retrievedConfig.customBPM, WithinAbs(140.0, 0.001));
        REQUIRE(retrievedConfig.followTempoChanges == false);
    }

    SECTION("Time-Based Configuration") {
        TimeBasedConfig config;
        config.intervalMs = 50.0;

        engine.setTimeBasedConfig(config);

        const auto& retrievedConfig = engine.getTimeBasedConfig();
        REQUIRE_THAT(retrievedConfig.intervalMs, WithinAbs(50.0, 0.001));
    }
}

TEST_CASE("TimingEngine - Processing Blocks", "[TimingEngine]") {
    TimingEngine engine;

    // Prepare for processing at 44.1kHz
    engine.prepareToPlay(44100.0, 512);

    SECTION("Process timing block updates stats") {
        engine.processTimingBlock(nullptr, 512);

        const auto& stats = engine.getPerformanceStats();
        REQUIRE(stats.processBlockCalls.load() == 1);
    }

    SECTION("Free running mode capture logic") {
        // Set to free running mode with 1024 sample interval
        engine.setTimingMode(TimingMode::FreeRunning);

        // Process the first timing block to advance sample count (512 samples)
        engine.processTimingBlock(nullptr, 512);

        // First block should not trigger capture (insufficient samples - only 512 processed)
        bool shouldCapture1 = engine.shouldCaptureAtCurrentTime(nullptr, nullptr, 512);
        REQUIRE(shouldCapture1 == false);

        // Process the second timing block to advance sample count (1024 total)
        engine.processTimingBlock(nullptr, 512);

        // Second block should trigger capture (reaches 1024 samples)
        bool shouldCapture2 = engine.shouldCaptureAtCurrentTime(nullptr, nullptr, 512);
        REQUIRE(shouldCapture2 == true);
    }

    // Clean up
    engine.releaseResources();
}

TEST_CASE("TimingEngine - Trigger Detection", "[TimingEngine]") {
    TimingEngine engine;
    engine.prepareToPlay(44100.0, 8);
    engine.setTimingMode(TimingMode::Trigger);

    SECTION("Rising edge detection") {
        TriggerConfig config;
        config.type = TriggerType::Level;
        config.edge = TriggerEdge::Rising;
        config.threshold = 0.5F;
        config.hysteresis = 0.1F;
        config.enabled = true;  // Explicitly enable
        config.holdOffSamples = 1;  // Minimum valid hold-off
        engine.setTriggerConfig(config);

        // Test step-by-step processing

        // Step 1: Initialize with low value
        std::array<float, 1> lowAudio = {0.2F};
        std::array<const float*, 1> lowChannels = {lowAudio.data()};
        engine.processTimingBlock(nullptr, 1);
        bool result1 = engine.shouldCaptureAtCurrentTime(nullptr, lowChannels.data(), 1);
        REQUIRE(result1 == false);  // Should not trigger on initialization

        // Step 2: Now provide high value that should trigger
        std::array<float, 1> highAudio = {0.6F};
        std::array<const float*, 1> highChannels = {highAudio.data()};
        engine.processTimingBlock(nullptr, 1);
        bool result2 = engine.shouldCaptureAtCurrentTime(nullptr, highChannels.data(), 1);

        // Debug: If this fails, let's see why
        if (!result2) {
            auto state = engine.getTimingState();
            auto triggerConfig = engine.getTriggerConfig();
            // Force trigger to ensure mechanism works
            engine.forceTrigger();
            auto stats = engine.getPerformanceStats();
            // Add context for debugging
            INFO("TimingMode: " << static_cast<int>(state.currentMode));
            INFO("TriggerEnabled: " << triggerConfig.enabled);
            INFO("TriggerType: " << static_cast<int>(triggerConfig.type));
            INFO("Threshold: " << triggerConfig.threshold);
            INFO("Manual trigger stats: " << stats.triggerDetections.load());
        }

        REQUIRE(result2 == true);

        const auto& stats = engine.getPerformanceStats();
        REQUIRE(stats.triggerDetections.load() >= 1);
    }

    SECTION("Falling edge detection") {
        TriggerConfig config;
        config.type = TriggerType::Level;
        config.edge = TriggerEdge::Falling;
        config.threshold = 0.5F;
        config.hysteresis = 0.1F;
        config.enabled = true;  // Explicitly enable
        config.holdOffSamples = 1;  // Minimum valid hold-off
        engine.setTriggerConfig(config);

        // Initialize with a high value first to establish baseline above threshold
        std::array<float, 1> initAudio = {0.8F};
        std::array<const float*, 1> initChannels = {initAudio.data()};
        engine.processTimingBlock(nullptr, 1);
        engine.shouldCaptureAtCurrentTime(nullptr, initChannels.data(), 1);

        // Create test audio: crosses below threshold - hysteresis
        std::array<float, 8> testAudio = {0.8F, 0.7F, 0.6F, 0.4F, 0.3F, 0.2F, 0.3F, 0.4F};
        std::array<const float*, 1> channels = {testAudio.data()};

        engine.processTimingBlock(nullptr, 8);
        bool shouldCapture = engine.shouldCaptureAtCurrentTime(nullptr, channels.data(), 8);
        REQUIRE(shouldCapture == true);

        const auto& stats = engine.getPerformanceStats();
        REQUIRE(stats.triggerDetections.load() >= 1);
    }

    engine.releaseResources();
}

TEST_CASE("TimingEngine - Thread Safety", "[TimingEngine]") {
    TimingEngine engine;
    engine.prepareToPlay(44100.0, 512);

    // Test concurrent mode switching and processing
    std::atomic<bool> running{true};
    std::atomic<int> modeSwitches{0};

    // Thread 1: Switch modes rapidly
    std::thread modeThread([&]() {
        const std::array<TimingMode, 4> modes = {
            TimingMode::FreeRunning,
            TimingMode::HostSync,
            TimingMode::TimeBased,
            TimingMode::Musical
        };

        size_t modeIndex = 0;
        while (running.load()) {
            engine.setTimingMode(modes[modeIndex % modes.size()]);
            modeSwitches.fetch_add(1);
            modeIndex++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 2: Process timing blocks continuously
    std::thread processThread([&]() {
        while (running.load()) {
            engine.processTimingBlock(nullptr, 512);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    running.store(false);

    modeThread.join();
    processThread.join();

    // Verify engine is still functional
    REQUIRE(modeSwitches.load() > 0);
    const auto& stats = engine.getPerformanceStats();
    REQUIRE(stats.processBlockCalls.load() > 0);

    engine.releaseResources();
}

TEST_CASE("TimingEngine - Performance Stats Reset", "[TimingEngine]") {
    TimingEngine engine;
    engine.prepareToPlay(44100.0, 512);

    // Generate some activity
    engine.setTimingMode(TimingMode::Musical);
    engine.processTimingBlock(nullptr, 512);
    engine.processTimingBlock(nullptr, 512);

    // Verify stats have values
    const auto& statsBefore = engine.getPerformanceStats();
    REQUIRE(statsBefore.processBlockCalls.load() > 0);
    REQUIRE(statsBefore.modeChanges.load() > 0);

    // Reset stats
    engine.resetStatistics();

    // Verify stats are reset
    const auto& statsAfter = engine.getPerformanceStats();
    REQUIRE(statsAfter.processBlockCalls.load() == 0);
    REQUIRE(statsAfter.modeChanges.load() == 0);
    REQUIRE(statsAfter.triggerDetections.load() == 0);

    engine.releaseResources();
}
