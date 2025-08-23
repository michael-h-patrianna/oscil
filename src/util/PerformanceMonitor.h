/**
 * @file PerformanceMonitor.h
 * @brief High-performance timing and performance monitoring for real-time audio applications
 *
 * This file contains performance monitoring utilities designed for zero-allocation,
 * lock-free operation in real-time audio contexts. Provides frame timing statistics,
 * FPS monitoring, and performance validation with minimal overhead.
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <chrono>
#include <atomic>
#include <array>
#include <cmath>

namespace oscil::util {

/**
 * @class PerformanceMonitor
 * @brief High-performance frame timing and performance monitoring system
 *
 * Provides comprehensive performance monitoring capabilities designed specifically
 * for real-time audio applications where traditional profiling tools introduce
 * unacceptable overhead. Uses lock-free atomic operations and pre-allocated
 * buffers to ensure zero-allocation operation during monitoring.
 *
 * Key features:
 * - Zero-allocation operation during measurement
 * - Lock-free thread-safe statistics collection
 * - Circular buffer for historical timing data
 * - Statistical analysis (mean, min, max, standard deviation)
 * - Frame rate calculation and monitoring
 * - Performance threshold validation
 * - RAII scoped timing utilities
 *
 * The monitor maintains a circular buffer of timing samples and provides
 * real-time statistical analysis without blocking or memory allocation.
 * All operations are designed to have minimal impact on the measured code.
 *
 * Performance characteristics:
 * - Timing overhead: <1μs per measurement
 * - Memory usage: Fixed ~4.8KB for sample buffer
 * - Thread safety: Lock-free for all operations
 * - Statistics calculation: O(n) where n is sample count
 *
 * Use cases:
 * - Audio callback performance monitoring
 * - UI frame rate validation
 * - Performance regression detection
 * - Real-time system optimization
 * - Automated performance testing
 *
 * Thread safety:
 * - All timing operations are lock-free and atomic
 * - Statistics can be safely read from any thread
 * - Multiple threads can record timings concurrently
 * - No blocking operations in critical paths
 *
 * Example usage:
 * @code
 * PerformanceMonitor monitor;
 *
 * // Method 1: Manual timing
 * auto start = monitor.startTiming();
 * performExpensiveOperation();
 * monitor.endTiming(start);
 *
 * // Method 2: RAII scoped timing
 * {
 *     OSCIL_SCOPED_TIMER(monitor);
 *     performExpensiveOperation();
 * } // Timing automatically recorded
 *
 * // Get statistics
 * auto stats = monitor.getStats();
 * if (stats.averageMs > 16.67) {
 *     // Frame time exceeds 60fps target
 * }
 * @endcode
 *
 * @see ScopedTimer for RAII timing utilities
 * @note Designed for minimal overhead in real-time audio contexts
 * @warning Do not use in extremely tight loops - timing overhead may accumulate
 */
class PerformanceMonitor {
public:
    /**
     * @brief Maximum number of timing samples to retain
     *
     * Circular buffer size for historical timing data. Set to 600 samples
     * which provides 10 seconds of history at 60 FPS.
     */
    static constexpr size_t MAX_SAMPLES = 600; // 10 seconds at 60 FPS

    /**
     * @struct FrameStats
     * @brief Comprehensive performance statistics container
     *
     * Contains all computed performance metrics including timing statistics,
     * frame rate information, and performance indicators. All values are
     * computed from the current sample buffer contents.
     */
    struct FrameStats {
        /**
         * @brief Average frame/operation time in milliseconds
         *
         * Arithmetic mean of all timing samples in the current buffer.
         * Provides the typical execution time over the measurement period.
         */
        double averageMs = 0.0;

        /**
         * @brief Minimum recorded time in milliseconds
         *
         * Best-case execution time observed during the measurement period.
         * Useful for identifying optimal performance potential.
         */
        double minMs = 0.0;

        /**
         * @brief Maximum recorded time in milliseconds
         *
         * Worst-case execution time observed during the measurement period.
         * Critical for real-time deadline analysis.
         */
        double maxMs = 0.0;

        /**
         * @brief Standard deviation of timing samples in milliseconds
         *
         * Measure of timing consistency. Lower values indicate more
         * predictable performance, which is crucial for real-time systems.
         */
        double stdDevMs = 0.0;

        /**
         * @brief Total number of frames/operations recorded
         *
         * Cumulative count of all timing measurements since last reset.
         * Used for long-term performance tracking.
         */
        uint64_t totalFrames = 0;

        /**
         * @brief Target frame rate in frames per second
         *
         * Expected frame rate for performance comparison. Typically 60.0
         * for UI operations or varies for audio block processing.
         */
        double targetFps = 60.0;

        /**
         * @brief Actual achieved frame rate in frames per second
         *
         * Calculated from recent frame intervals. Compares against
         * targetFps to determine if performance targets are met.
         */
        double actualFps = 0.0;
    };

    /**
     * @brief Default constructor
     *
     * Initializes the performance monitor with empty sample buffers
     * and zero statistics. Ready for immediate use.
     *
     * @post Monitor is ready for timing operations
     * @post All statistics are zeroed
     * @post Sample buffers are empty
     */
    PerformanceMonitor() = default;

    /**
     * @brief Destructor
     *
     * Cleans up any resources. Since the monitor uses only stack
     * allocation and atomics, no special cleanup is required.
     */
    ~PerformanceMonitor() = default;

    /**
     * @brief Starts timing a frame or operation
     *
     * Records the current high-resolution timestamp for later use with
     * endTiming(). The returned time point must be passed to endTiming()
     * to complete the measurement.
     *
     * @return High-resolution timestamp for timing reference
     *
     * @post Timestamp is captured with minimal overhead
     * @note Thread-safe and lock-free operation
     * @note Overhead: typically <1μs on modern systems
     *
     * @see endTiming()
     */
    std::chrono::high_resolution_clock::time_point startTiming();

    /**
     * @brief Completes timing and records the duration
     *
     * Calculates the elapsed time since startTiming() and adds the
     * measurement to the performance statistics. The duration is
     * stored in the circular buffer for statistical analysis.
     *
     * @param startTime Timestamp returned by startTiming()
     *
     * @pre startTime must be a valid timestamp from startTiming()
     * @post Duration is recorded in statistics
     * @post Circular buffer is updated with new sample
     *
     * @note Thread-safe and lock-free operation
     * @note Automatically handles buffer wraparound
     *
     * @see startTiming()
     */
    void endTiming(const std::chrono::high_resolution_clock::time_point& startTime);

    /**
     * @brief Records a frame completion for FPS calculation
     *
     * Marks the completion of a frame or regular operation for frame rate
     * calculation. Should be called once per frame/operation cycle to
     * maintain accurate FPS statistics.
     *
     * @post Frame counter is incremented
     * @post Frame interval is recorded for FPS calculation
     * @post Statistics reflect updated frame rate
     *
     * @note Thread-safe operation
     * @note Call frequency determines FPS accuracy
     */
    void recordFrame();

    /**
     * @brief Retrieves current performance statistics
     *
     * Computes and returns comprehensive performance statistics based on
     * current sample buffer contents. This operation is lock-free and
     * safe to call from any thread without affecting measurements.
     *
     * @return Complete performance statistics structure
     *
     * @note Lock-free and thread-safe operation
     * @note Statistics computed in real-time from current samples
     * @note Safe to call frequently without performance impact
     *
     * Performance: O(n) where n is current sample count
     */
    FrameStats getStats() const;

    /**
     * @brief Resets all performance statistics
     *
     * Clears all timing samples, frame counters, and statistical data.
     * Use when starting a new measurement period or after significant
     * system changes that might affect baseline performance.
     *
     * @post All sample buffers are cleared
     * @post Frame counters are reset to zero
     * @post Next getStats() will show clean slate
     *
     * @note Thread-safe operation
     * @note Does not affect ongoing timing operations
     */
    void reset();

    /**
     * @brief Validates performance against specified criteria
     *
     * Evaluates current performance statistics against specified thresholds
     * to determine if the system is meeting performance requirements.
     * Useful for automated testing and performance validation.
     *
     * @param maxCpuPercent Maximum acceptable CPU usage percentage (default: 1.0%)
     * @param minFps Minimum acceptable frame rate (default: 60.0 FPS)
     * @param maxFrameVarianceMs Maximum acceptable frame time variance (default: 2.0ms)
     *
     * @return true if all performance criteria are met
     * @retval false if any performance threshold is exceeded
     *
     * @note Based on current statistics from sample buffer
     * @note Useful for automated performance regression detection
     *
     * Example criteria:
     * - CPU usage estimated from average frame time
     * - Frame rate from actual FPS calculation
     * - Consistency from standard deviation of frame times
     */
    bool isPerformanceAcceptable(double maxCpuPercent = 1.0,
                                double minFps = 60.0,
                                double maxFrameVarianceMs = 2.0) const;

private:
    //==============================================================================
    // Private Member Variables

    /**
     * @brief Lock-free circular buffer for frame timing samples
     *
     * Array of atomic double values storing timing measurements in milliseconds.
     * Uses lock-free atomic operations to allow concurrent access without
     * blocking in real-time contexts.
     *
     * @note Each element is atomic for thread safety
     * @note Fixed size to avoid dynamic allocation
     */
    std::array<std::atomic<double>, MAX_SAMPLES> frameTimes{};

    /**
     * @brief Current write position in circular buffer
     *
     * Atomic index tracking where the next timing sample will be written.
     * Automatically wraps around when reaching MAX_SAMPLES.
     */
    std::atomic<size_t> currentIndex{0};

    /**
     * @brief Number of valid samples in the buffer
     *
     * Tracks how many timing samples have been recorded. Saturates at
     * MAX_SAMPLES once the buffer is full.
     */
    std::atomic<size_t> sampleCount{0};

    /**
     * @brief Total number of frames recorded since last reset
     *
     * Cumulative counter for all timing operations performed.
     * Used for long-term performance tracking and statistics.
     */
    std::atomic<uint64_t> totalFrames{0};

    /**
     * @brief Timestamp of the last recorded frame
     *
     * Used for frame rate calculation by measuring intervals between
     * recordFrame() calls. Atomic to ensure thread-safe access.
     */
    std::atomic<std::chrono::high_resolution_clock::time_point> lastFrameTime{};

    /**
     * @brief Calculates comprehensive statistics from current samples
     *
     * Internal method that computes mean, min, max, standard deviation,
     * and frame rate from the current contents of the sample buffer.
     * Called by getStats() to generate the FrameStats structure.
     *
     * @return Complete statistical analysis of current samples
     *
     * @note Lock-free operation safe for concurrent access
     * @note Performance: O(n) where n is current sample count
     */
    FrameStats calculateStats() const;
};

/**
 * @class ScopedTimer
 * @brief RAII timing helper for automatic performance measurement
 *
 * Provides automatic timing measurement using RAII (Resource Acquisition
 * Is Initialization) pattern. Timing starts at construction and ends at
 * destruction, ensuring measurements are always completed even if
 * exceptions occur.
 *
 * This is the preferred method for timing code blocks as it eliminates
 * the possibility of forgetting to call endTiming() and provides
 * exception safety.
 *
 * Example usage:
 * @code
 * PerformanceMonitor monitor;
 *
 * {
 *     ScopedTimer timer(monitor);
 *     performExpensiveOperation();
 * } // Timing automatically recorded here
 *
 * // Or using the convenience macro:
 * {
 *     OSCIL_SCOPED_TIMER(monitor);
 *     performExpensiveOperation();
 * } // Timing automatically recorded here
 * @endcode
 *
 * @see PerformanceMonitor
 * @see OSCIL_SCOPED_TIMER macro
 */
class ScopedTimer {
public:
    /**
     * @brief Constructs timer and begins measurement
     *
     * Immediately starts timing by calling monitor.startTiming().
     * The measurement will be completed when the timer is destroyed.
     *
     * @param monitor Reference to the performance monitor to record timing
     *
     * @post Timing measurement has begun
     * @note Overhead is minimal (<1μs typically)
     */
    explicit ScopedTimer(PerformanceMonitor& monitor)
        : monitor_(monitor), startTime_(monitor.startTiming()) {}

    /**
     * @brief Destructor completes timing measurement
     *
     * Automatically calls monitor.endTiming() with the stored start time,
     * ensuring the measurement is recorded even if exceptions occur.
     *
     * @post Timing measurement is recorded in monitor statistics
     */
    ~ScopedTimer() {
        monitor_.endTiming(startTime_);
    }

private:
    /**
     * @brief Reference to the performance monitor
     *
     * Stores reference to the monitor that will receive the timing data
     * when the scoped timer is destroyed.
     */
    PerformanceMonitor& monitor_;

    /**
     * @brief Timestamp when timing began
     *
     * High-resolution timestamp captured during construction.
     * Used to calculate elapsed time in the destructor.
     */
    std::chrono::high_resolution_clock::time_point startTime_;
};

/**
 * @def OSCIL_SCOPED_TIMER
 * @brief Convenience macro for creating a scoped timer
 *
 * Creates an unnamed ScopedTimer instance that will automatically
 * time the current scope. More convenient than creating a named
 * ScopedTimer instance.
 *
 * @param monitor The PerformanceMonitor instance to record timing
 *
 * Example:
 * @code
 * PerformanceMonitor monitor;
 *
 * void expensiveFunction() {
 *     OSCIL_SCOPED_TIMER(monitor);
 *     // Function body is automatically timed
 * }
 * @endcode
 */
#define OSCIL_SCOPED_TIMER(monitor) oscil::util::ScopedTimer _timer(monitor)

} // namespace oscil::util
