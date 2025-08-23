/**
 * @file test_measurement_system.cpp
 * @brief Unit tests for the measurement system components (Task 4.4)
 *
 * Tests the basic functionality of measurement data structures.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/audio/MeasurementData.h"

TEST_CASE("MeasurementData - Basic Functionality") {
    SECTION("Default construction") {
        audio::MeasurementData data;

        REQUIRE_FALSE(data.correlationValid);
        REQUIRE_FALSE(data.stereoWidthValid);
        REQUIRE_FALSE(data.levelsValid);
        REQUIRE(data.measurementTimestamp == 0);
        REQUIRE(data.measurementId == 0);
    }

    SECTION("Clear functionality") {
        audio::MeasurementData data;

        // Set some values
        data.correlationValid = true;
        data.stereoWidth = 0.5f;
        data.peakLevels[0] = 0.8f;
        data.measurementTimestamp = 123456;

        // Clear and verify reset
        data.clear();

        REQUIRE_FALSE(data.correlationValid);
        REQUIRE_FALSE(data.stereoWidthValid);
        REQUIRE_FALSE(data.levelsValid);
        REQUIRE(data.stereoWidth == 1.0f); // Default stereo width
        REQUIRE(data.peakLevels[0] == 0.0f);
    }
}

TEST_CASE("MeasurementDataBridge - Basic Operations") {
    SECTION("Single measurement") {
        audio::MeasurementDataBridge bridge;

        // Create test measurement data
        audio::MeasurementData testData;
        testData.correlationValid = true;
        testData.correlationMetrics.correlation = 0.7f;
        testData.stereoWidth = 1.2f;
        testData.stereoWidthValid = true;
        testData.peakLevels[0] = 0.5f;
        testData.peakLevels[1] = 0.6f;
        testData.levelsValid = true;
        testData.measurementTimestamp = 789012;
        testData.measurementId = 42;

        // Push data
        bridge.pushMeasurementData(testData);

        // Pull data
        audio::MeasurementData retrieved;
        bool hasData = bridge.pullLatestMeasurements(retrieved);

        REQUIRE(hasData);
        REQUIRE(retrieved.correlationValid);
        REQUIRE_THAT(retrieved.correlationMetrics.correlation, Catch::Matchers::WithinAbs(0.7f, 0.001f));
        REQUIRE_THAT(retrieved.stereoWidth, Catch::Matchers::WithinAbs(1.2f, 0.001f));
        REQUIRE(retrieved.stereoWidthValid);
        REQUIRE(retrieved.levelsValid);
        REQUIRE(retrieved.measurementTimestamp == 789012);
        REQUIRE(retrieved.measurementId == 42);
    }
}

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_core/juce_core.h>

#include "../src/audio/MeasurementData.h"
#include "../src/ui/measurements/CorrelationMeterComponent.h"
#include "../src/ui/measurements/MeasurementOverlayComponent.h"

using namespace oscil::ui::measurements;
using namespace audio;

TEST_CASE("MeasurementData - Basic Functionality") {
    SECTION("Default construction") {
        MeasurementData data;

        REQUIRE_FALSE(data.correlationValid);
        REQUIRE_FALSE(data.stereoWidthValid);
        REQUIRE_FALSE(data.levelsValid);
        REQUIRE(data.measurementTimestamp == 0);
        REQUIRE(data.measurementId == 0);
    }

    SECTION("Clear functionality") {
        MeasurementData data;

        // Set some values
        data.correlationValid = true;
        data.stereoWidth = 0.5f;
        data.peakLevels[0] = 0.8f;
        data.measurementTimestamp = 123456;

        // Clear and verify reset
        data.clear();

        REQUIRE_FALSE(data.correlationValid);
        REQUIRE_FALSE(data.stereoWidthValid);
        REQUIRE_FALSE(data.levelsValid);
        REQUIRE(data.stereoWidth == 1.0f); // Default stereo width
        REQUIRE(data.peakLevels[0] == 0.0f);
    }
}

TEST_CASE("MeasurementDataBridge - Thread Safety") {
    SECTION("Basic push and pull operations") {
        MeasurementDataBridge bridge;

        // Create test measurement data
        MeasurementData testData;
        testData.correlationValid = true;
        testData.correlationMetrics.correlation = 0.7f;
        testData.stereoWidth = 1.2f;
        testData.stereoWidthValid = true;
        testData.peakLevels[0] = 0.5f;
        testData.peakLevels[1] = 0.6f;
        testData.levelsValid = true;
        testData.measurementTimestamp = 789012;
        testData.measurementId = 42;

        // Push data
        bridge.pushMeasurementData(testData);

        // Pull data
        MeasurementData retrieved;
        bool hasData = bridge.pullLatestMeasurements(retrieved);

        REQUIRE(hasData);
        REQUIRE(retrieved.correlationValid == true);
        REQUIRE_THAT(retrieved.correlationMetrics.correlation, Catch::Matchers::WithinAbs(0.7f, 0.001f));
        REQUIRE_THAT(retrieved.stereoWidth, Catch::Matchers::WithinAbs(1.2f, 0.001f));
        REQUIRE(retrieved.stereoWidthValid == true);
        REQUIRE_THAT(retrieved.peakLevels[0], Catch::Matchers::WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(retrieved.peakLevels[1], Catch::Matchers::WithinAbs(0.6f, 0.001f));
        REQUIRE(retrieved.levelsValid == true);
        REQUIRE(retrieved.measurementTimestamp == 789012);
        REQUIRE(retrieved.measurementId == 42);
    }

    SECTION("Multiple measurements") {
        MeasurementDataBridge bridge;

        // Push multiple measurements
        for (int i = 0; i < 5; ++i) {
            MeasurementData data;
            data.correlationMetrics.correlation = static_cast<float>(i) * 0.1f;
            data.correlationValid = true;
            data.measurementId = static_cast<uint32_t>(i);
            bridge.pushMeasurementData(data);
        }

        // Pull all measurements
        std::vector<MeasurementData> measurements = bridge.pullLatestMeasurements();

        REQUIRE(measurements.size() == 5);

        // Verify measurements are in the correct order
        for (size_t i = 0; i < measurements.size(); ++i) {
            REQUIRE(measurements[i].measurementId == i);
            REQUIRE_THAT(measurements[i].correlationMetrics.correlation,
                        Catch::Matchers::WithinAbs(static_cast<float>(i) * 0.1f, 0.001f));
        }
    }
}

TEST_CASE("CorrelationMeterComponent - Construction") {
    SECTION("Default construction") {
        // Test that component can be constructed without errors
        REQUIRE_NOTHROW(CorrelationMeterComponent{});
    }

    SECTION("Construction with custom config") {
        CorrelationMeterComponent::MeterConfig config;
        config.smoothingFactor = 0.9f;
        config.peakHoldTimeMs = 2000.0f;
        config.updateRateHz = 60.0f;

        REQUIRE_NOTHROW(CorrelationMeterComponent{config});
    }
}

TEST_CASE("MeasurementOverlayComponent - Construction") {
    SECTION("Default construction") {
        REQUIRE_NOTHROW(MeasurementOverlayComponent{});
    }

    SECTION("Construction with custom config") {
        MeasurementOverlayComponent::OverlayConfig config;
        config.overlayOpacity = 0.8f;
        config.overlayPadding = 12;
        config.animationDurationMs = 300.0f;

        REQUIRE_NOTHROW(MeasurementOverlayComponent{config});
    }
}

TEST_CASE("Measurement System Integration") {
    SECTION("CorrelationMeterComponent measurement updates") {
        CorrelationMeterComponent meter;

        // Create test correlation metrics
        CorrelationMetrics metrics;
        metrics.correlation = 0.85f;
        metrics.stereoWidth = 1.5f;

        // Update meter with metrics
        REQUIRE_NOTHROW(meter.updateValues(metrics));

        // Component should be relevant for stereo modes
        REQUIRE(meter.isRelevantForCurrentMode());
    }

    SECTION("MeasurementOverlayComponent child management") {
        MeasurementOverlayComponent overlay;

        // Create and add correlation meter
        auto correlationMeter = std::make_unique<CorrelationMeterComponent>();
        auto* meterPtr = correlationMeter.get();

        overlay.setCorrelationMeter(std::move(correlationMeter));

        // Test visibility updates with measurement data
        CorrelationMetrics metrics;
        metrics.correlation = 0.6f;
        metrics.stereoWidth = 1.1f;

        REQUIRE_NOTHROW(overlay.updateCorrelationMeasurements(metrics));
        REQUIRE_NOTHROW(overlay.updateStereoWidthMeasurements(metrics));

        // Verify overlay has measurements visible
        REQUIRE(overlay.hasMeasurementsVisible());
    }
}
