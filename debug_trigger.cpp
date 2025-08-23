#include "src/timing/TimingEngine.h"
#include <iostream>

using namespace oscil::timing;

int main() {
    TimingEngine engine;
    engine.prepareToPlay(44100.0, 8);
    engine.setTimingMode(TimingMode::Trigger);

    TriggerConfig config;
    config.type = TriggerType::Level;
    config.edge = TriggerEdge::Rising;
    config.threshold = 0.5F;
    config.hysteresis = 0.1F;
    config.enabled = true;
    config.holdOffSamples = 0;
    engine.setTriggerConfig(config);

    std::cout << "Config: threshold=" << config.threshold << " hysteresis=" << config.hysteresis << std::endl;
    std::cout << "Expected trigger condition: lastSample <= " << (config.threshold - config.hysteresis)
              << " AND sample >= " << config.threshold << std::endl;

    // Initialize with a low value
    std::array<float, 1> initAudio = {0.2F};
    std::array<const float*, 1> initChannels = {initAudio.data()};
    engine.processTimingBlock(nullptr, 1);
    bool result1 = engine.shouldCaptureAtCurrentTime(nullptr, initChannels.data(), 1);
    std::cout << "Initial call with [0.2] result: " << result1 << std::endl;

    // Test with trigger data
    std::array<float, 8> testAudio = {0.2F, 0.3F, 0.4F, 0.6F, 0.7F, 0.8F, 0.7F, 0.6F};
    std::array<const float*, 1> channels = {testAudio.data()};

    std::cout << "Test data: [";
    for (size_t i = 0; i < testAudio.size(); ++i) {
        std::cout << testAudio[i];
        if (i < testAudio.size() - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    engine.processTimingBlock(nullptr, 8);
    bool result2 = engine.shouldCaptureAtCurrentTime(nullptr, channels.data(), 8);
    std::cout << "Trigger test result: " << result2 << std::endl;

    auto stats = engine.getPerformanceStats();
    std::cout << "Trigger detections: " << stats.triggerDetections.load() << std::endl;

    return 0;
}
