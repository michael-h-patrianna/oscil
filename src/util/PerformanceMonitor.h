#pragma once

#include <chrono>
#include <atomic>
#include <array>
#include <cmath>

namespace oscil::util {

/**
 * High-performance frame timing and performance monitoring.
 * Designed for zero-allocation, lock-free operation in real-time contexts.
 */
class PerformanceMonitor {
public:
    static constexpr size_t MAX_SAMPLES = 600; // 10 seconds at 60 FPS

    struct FrameStats {
        double averageMs = 0.0;
        double minMs = 0.0;
        double maxMs = 0.0;
        double stdDevMs = 0.0;
        uint64_t totalFrames = 0;
        double targetFps = 60.0;
        double actualFps = 0.0;
    };

    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;

    /**
     * Start timing a frame or operation.
     * Returns a handle to be used with endTiming().
     */
    std::chrono::high_resolution_clock::time_point startTiming();

    /**
     * End timing and record the duration.
     * @param startTime Time point returned by startTiming()
     */
    void endTiming(const std::chrono::high_resolution_clock::time_point& startTime);

    /**
     * Record a frame completion (for FPS calculation).
     */
    void recordFrame();

    /**
     * Get current performance statistics.
     * This is lock-free and safe to call from any thread.
     */
    FrameStats getStats() const;

    /**
     * Reset all statistics.
     */
    void reset();

    /**
     * Check if performance requirements are being met.
     * @param maxCpuPercent Maximum acceptable CPU percentage
     * @param minFps Minimum acceptable FPS
     * @param maxFrameVarianceMs Maximum acceptable frame time variance
     */
    bool isPerformanceAcceptable(double maxCpuPercent = 1.0,
                                double minFps = 60.0,
                                double maxFrameVarianceMs = 2.0) const;

private:
    // Lock-free circular buffer for frame timings
    std::array<std::atomic<double>, MAX_SAMPLES> frameTimes{};
    std::atomic<size_t> currentIndex{0};
    std::atomic<size_t> sampleCount{0};
    std::atomic<uint64_t> totalFrames{0};

    // Frame rate calculation
    std::atomic<std::chrono::high_resolution_clock::time_point> lastFrameTime{};

    /**
     * Calculate statistics from current samples.
     * Called internally by getStats().
     */
    FrameStats calculateStats() const;
};

/**
 * RAII timing helper for scoped performance measurement.
 */
class ScopedTimer {
public:
    explicit ScopedTimer(PerformanceMonitor& monitor)
        : monitor_(monitor), startTime_(monitor.startTiming()) {}

    ~ScopedTimer() {
        monitor_.endTiming(startTime_);
    }

private:
    PerformanceMonitor& monitor_;
    std::chrono::high_resolution_clock::time_point startTime_;
};

// Convenience macro for scoped timing
#define OSCIL_SCOPED_TIMER(monitor) oscil::util::ScopedTimer _timer(monitor)

} // namespace oscil::util
