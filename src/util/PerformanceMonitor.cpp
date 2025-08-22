#include "PerformanceMonitor.h"
#include <algorithm>
#include <numeric>

namespace oscil::util {

std::chrono::high_resolution_clock::time_point PerformanceMonitor::startTiming() {
    return std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::endTiming(const std::chrono::high_resolution_clock::time_point& startTime) {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    double durationMs = duration.count() / 1000.0;

    // Store in circular buffer
    size_t index = currentIndex.load(std::memory_order_relaxed);
    frameTimes[index].store(durationMs, std::memory_order_relaxed);

    // Update indices atomically
    currentIndex.store((index + 1) % MAX_SAMPLES, std::memory_order_relaxed);
    sampleCount.store(std::min(sampleCount.load(std::memory_order_relaxed) + 1, MAX_SAMPLES),
                     std::memory_order_relaxed);
}

void PerformanceMonitor::recordFrame() {
    auto now = std::chrono::high_resolution_clock::now();
    lastFrameTime.store(now, std::memory_order_relaxed);
    totalFrames.fetch_add(1, std::memory_order_relaxed);
}

PerformanceMonitor::FrameStats PerformanceMonitor::getStats() const {
    return calculateStats();
}

void PerformanceMonitor::reset() {
    currentIndex.store(0, std::memory_order_relaxed);
    sampleCount.store(0, std::memory_order_relaxed);
    totalFrames.store(0, std::memory_order_relaxed);

    // Clear all samples
    for (auto& sample : frameTimes) {
        sample.store(0.0, std::memory_order_relaxed);
    }
}

bool PerformanceMonitor::isPerformanceAcceptable(double /*maxCpuPercent*/,
                                                double minFps,
                                                double maxFrameVarianceMs) const {
    auto stats = getStats();
    return stats.actualFps >= minFps &&
           stats.stdDevMs <= maxFrameVarianceMs;
    // Note: CPU percentage would need system-level monitoring
}

PerformanceMonitor::FrameStats PerformanceMonitor::calculateStats() const {
    FrameStats stats;
    stats.totalFrames = totalFrames.load(std::memory_order_relaxed);

    size_t count = sampleCount.load(std::memory_order_relaxed);
    if (count == 0) {
        return stats;
    }

    // Collect current samples (lock-free snapshot)
    std::vector<double> samples;
    samples.reserve(count);

    size_t startIndex = (currentIndex.load(std::memory_order_relaxed) + MAX_SAMPLES - count) % MAX_SAMPLES;
    for (size_t i = 0; i < count; ++i) {
        size_t index = (startIndex + i) % MAX_SAMPLES;
        samples.push_back(frameTimes[index].load(std::memory_order_relaxed));
    }

    // Calculate statistics
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    stats.averageMs = sum / count;

    auto minMax = std::minmax_element(samples.begin(), samples.end());
    stats.minMs = *minMax.first;
    stats.maxMs = *minMax.second;

    // Calculate standard deviation
    double squaredSum = 0.0;
    for (double sample : samples) {
        double diff = sample - stats.averageMs;
        squaredSum += diff * diff;
    }
    stats.stdDevMs = std::sqrt(squaredSum / count);

    // Calculate FPS (approximate)
    if (stats.averageMs > 0.0) {
        stats.actualFps = 1000.0 / stats.averageMs;
    }

    return stats;
}

} // namespace oscil::util
