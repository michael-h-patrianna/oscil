#include <catch2/catch_test_macros.hpp>
#include "../src/render/DecimationProcessor.h"
#include "../src/util/PerformanceMonitor.h"
#include <vector>
#include <chrono>
#include <random>
#include <iostream>
#include <numeric>

using namespace oscil::render;
using namespace oscil::util;

TEST_CASE("DecimationProcessor - Basic Functionality", "[DecimationProcessor][performance]") {
    DecimationProcessor processor;

    SECTION("No decimation needed") {
        std::vector<float> samples = {1.0f, 0.5f, -0.5f, -1.0f};

        auto result = processor.process(samples.data(), samples.size(), 10, 2.0f);

        REQUIRE(result.wasDecimated == false);
        REQUIRE(result.sampleCount == samples.size());
        REQUIRE(result.samples == samples.data());
    }

    SECTION("Decimation required") {
        // Create 1000 samples for 100 pixels = 10 samples per pixel > threshold
        std::vector<float> samples(1000);
        std::iota(samples.begin(), samples.end(), 0.0f);

        auto result = processor.process(samples.data(), samples.size(), 100, 2.0f);

        REQUIRE(result.wasDecimated == true);
        REQUIRE(result.sampleCount > 0);
        REQUIRE(result.sampleCount <= 200); // Max 2 samples per pixel (min/max)
    }
}

TEST_CASE("DecimationProcessor - Performance Benchmark", "[DecimationProcessor][performance]") {
    DecimationProcessor processor;
    PerformanceMonitor monitor;

    SECTION("Large dataset performance") {
        // Create large dataset similar to real audio buffer
        const size_t sampleCount = 44100; // 1 second at 44.1kHz
        std::vector<float> samples(sampleCount);

        // Fill with realistic audio data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (auto& sample : samples) {
            sample = dis(gen);
        }

        // Benchmark decimation performance
        const int iterations = 100;
        const int targetPixels = 800; // Typical window width

        for (int i = 0; i < iterations; ++i) {
            auto start = monitor.startTiming();

            auto result = processor.process(samples.data(), samples.size(), targetPixels);

            monitor.endTiming(start);

            // Verify result makes sense
            REQUIRE(result.wasDecimated == true);
            REQUIRE(result.sampleCount > 0);
            REQUIRE(result.sampleCount <= static_cast<size_t>(targetPixels * 2));
        }

        auto stats = monitor.getStats();

        // Performance requirements for Task 2.4
        REQUIRE(stats.averageMs < 1.0); // <1ms per decimation operation
        REQUIRE(stats.maxMs < 5.0); // No outliers > 5ms

        std::cout << "Decimation Performance:" << std::endl;
        std::cout << "  Average: " << stats.averageMs << "ms" << std::endl;
        std::cout << "  Min: " << stats.minMs << "ms" << std::endl;
        std::cout << "  Max: " << stats.maxMs << "ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stdDevMs << "ms" << std::endl;
    }
}

TEST_CASE("Performance - Memory Allocation Test", "[performance]") {
    SECTION("Pre-allocated vs dynamic allocation") {
        const size_t iterations = 1000;
        const size_t bufferSize = 1024;

        PerformanceMonitor dynamicMonitor;
        PerformanceMonitor preallocMonitor;

        // Test dynamic allocation (old approach)
        for (size_t i = 0; i < iterations; ++i) {
            auto start = dynamicMonitor.startTiming();

            std::vector<float> temp(bufferSize, 0.0f); // Allocation every time
            volatile float sum = 0.0f;
            for (size_t j = 0; j < bufferSize; ++j) {
                sum += temp[j];
            }
            (void)sum; // Prevent optimization

            dynamicMonitor.endTiming(start);
        }

        // Test pre-allocated approach (new approach)
        std::vector<float> reusedBuffer(bufferSize, 0.0f);
        for (size_t i = 0; i < iterations; ++i) {
            auto start = preallocMonitor.startTiming();

            std::fill(reusedBuffer.begin(), reusedBuffer.end(), 0.0f); // Reuse buffer
            volatile float sum = 0.0f;
            for (size_t j = 0; j < bufferSize; ++j) {
                sum += reusedBuffer[j];
            }
            (void)sum; // Prevent optimization

            preallocMonitor.endTiming(start);
        }

        auto dynamicStats = dynamicMonitor.getStats();
        auto preallocStats = preallocMonitor.getStats();

        std::cout << "Memory Allocation Performance Comparison:" << std::endl;
        std::cout << "  Dynamic allocation: " << dynamicStats.averageMs << "ms average" << std::endl;
        std::cout << "  Pre-allocated: " << preallocStats.averageMs << "ms average" << std::endl;
        std::cout << "  Improvement: " << (dynamicStats.averageMs / preallocStats.averageMs) << "x faster" << std::endl;

        // Pre-allocated should be faster
        REQUIRE(preallocStats.averageMs < dynamicStats.averageMs);

        // Both should meet performance targets
        REQUIRE(preallocStats.averageMs < 0.1); // Very fast for pre-allocated
        REQUIRE(dynamicStats.averageMs < 1.0);  // Still reasonable for dynamic
    }
}
