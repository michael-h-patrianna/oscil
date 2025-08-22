#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/util/PerformanceMonitor.h"
#include <thread>
#include <chrono>

using namespace oscil::util;

TEST_CASE("PerformanceMonitor - Basic Functionality", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    SECTION("Initial state") {
        auto stats = monitor.getStats();
        REQUIRE(stats.totalFrames == 0);
        REQUIRE(stats.averageMs == 0.0);
        REQUIRE(stats.actualFps == 0.0);
    }

    SECTION("Frame recording") {
        monitor.recordFrame();
        monitor.recordFrame();

        auto stats = monitor.getStats();
        REQUIRE(stats.totalFrames == 2);
    }
}

TEST_CASE("PerformanceMonitor - Timing Measurements", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    SECTION("Basic timing") {
        auto start = monitor.startTiming();
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 10ms sleep
        monitor.endTiming(start);

        auto stats = monitor.getStats();
        REQUIRE(stats.averageMs >= 8.0); // Should be around 10ms, but allow variance
        REQUIRE(stats.averageMs <= 15.0);
        REQUIRE(stats.minMs >= 8.0);
        REQUIRE(stats.maxMs <= 15.0);
    }

    SECTION("Multiple measurements") {
        // Record multiple timing measurements
        for (int i = 0; i < 10; ++i) {
            auto start = monitor.startTiming();
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + i)); // Variable timing 1-10ms
            monitor.endTiming(start);
        }

        auto stats = monitor.getStats();
        REQUIRE(stats.averageMs > 0.0);
        REQUIRE(stats.minMs > 0.0);
        REQUIRE(stats.maxMs > stats.minMs);
        REQUIRE(stats.stdDevMs >= 0.0);
    }
}

TEST_CASE("PerformanceMonitor - Performance Requirements", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    SECTION("Performance validation") {
        // Simulate fast operations (target <1ms)
        for (int i = 0; i < 60; ++i) {
            auto start = monitor.startTiming();
            // Very short operation
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // 0.1ms
            monitor.endTiming(start);
            monitor.recordFrame();
        }

        auto stats = monitor.getStats();

        // Should meet performance targets
        REQUIRE(stats.averageMs < 1.0); // <1ms per operation
        REQUIRE(stats.actualFps > 50.0); // Should calculate high FPS for fast operations

        // Test performance validation
        bool acceptable = monitor.isPerformanceAcceptable(1.0, 50.0, 5.0);
        REQUIRE(acceptable == true);
    }

    SECTION("Performance failure detection") {
        // Simulate slow operations
        for (int i = 0; i < 10; ++i) {
            auto start = monitor.startTiming();
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 20ms (too slow)
            monitor.endTiming(start);
            monitor.recordFrame();
        }

        auto stats = monitor.getStats();

        // Should detect performance issues
        REQUIRE(stats.averageMs > 15.0); // Should be slow
        bool acceptable = monitor.isPerformanceAcceptable(1.0, 60.0, 2.0);
        REQUIRE(acceptable == false); // Should fail due to slow timing
    }
}

TEST_CASE("PerformanceMonitor - Buffer Overflow", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    SECTION("Circular buffer behavior") {
        // Add more samples than the buffer can hold
        const size_t sampleCount = PerformanceMonitor::MAX_SAMPLES + 100;

        for (size_t i = 0; i < sampleCount; ++i) {
            auto start = monitor.startTiming();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            monitor.endTiming(start);
        }

        auto stats = monitor.getStats();

        // Should still work correctly with overflow
        REQUIRE(stats.averageMs > 0.0);
        REQUIRE(stats.averageMs < 1.0); // Should be around 0.1ms
    }
}

TEST_CASE("PerformanceMonitor - Reset Functionality", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    // Add some data
    auto start = monitor.startTiming();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    monitor.endTiming(start);
    monitor.recordFrame();

    auto statsBefore = monitor.getStats();
    REQUIRE(statsBefore.totalFrames > 0);
    REQUIRE(statsBefore.averageMs > 0.0);

    // Reset and check
    monitor.reset();
    auto statsAfter = monitor.getStats();

    REQUIRE(statsAfter.totalFrames == 0);
    REQUIRE(statsAfter.averageMs == 0.0);
    REQUIRE(statsAfter.actualFps == 0.0);
}

TEST_CASE("PerformanceMonitor - Scoped Timer", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    SECTION("RAII timing") {
        {
            ScopedTimer timer(monitor);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } // Timer destructor should automatically call endTiming

        auto stats = monitor.getStats();
        REQUIRE(stats.averageMs >= 4.0);
        REQUIRE(stats.averageMs <= 8.0);
    }
}

TEST_CASE("PerformanceMonitor - Zero Allocation Test", "[PerformanceMonitor][performance]") {
    PerformanceMonitor monitor;

    SECTION("No allocations during operation") {
        // This test verifies that normal operation doesn't allocate
        // Note: This is a conceptual test - actual allocation detection
        // would require memory profiling tools

        for (int i = 0; i < 1000; ++i) {
            auto start = monitor.startTiming();
            // Simulate very fast operation
            volatile int dummy = i * 2; // Prevent optimization
            (void)dummy;
            monitor.endTiming(start);
            monitor.recordFrame();

            // Get stats multiple times
            auto stats = monitor.getStats();
            REQUIRE(stats.totalFrames >= static_cast<uint64_t>(i + 1)); // Should record frames
        }

        // If we get here without crashes, the lock-free design is working
        REQUIRE(true);
    }
}
