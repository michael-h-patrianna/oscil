/**
 * @file RingBuffer.h
 * @brief Thread-safe lock-free ring buffer implementation for audio processing
 *
 * This file contains a high-performance ring buffer implementation designed
 * for real-time audio applications. The buffer provides wait-free operations
 * for single producer/single consumer scenarios and handles buffer overflow
 * by overwriting the oldest data.
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace dsp {

/**
 * @class RingBuffer
 * @brief Thread-safe lock-free ring buffer for real-time audio processing
 *
 * A high-performance circular buffer implementation optimized for audio
 * applications. Uses atomic operations for thread safety without locks,
 * making it suitable for real-time audio callbacks.
 *
 * Key features:
 * - Lock-free operations suitable for real-time audio threads
 * - Single producer/single consumer (SPSC) thread safety
 * - Automatic overwrite of oldest data when buffer is full
 * - Efficient memory layout with contiguous storage
 * - Support for bulk read/write operations
 *
 * The buffer maintains separate atomic head and tail pointers to track
 * the current write position and the oldest available data. Memory
 * ordering is carefully controlled to ensure correct operation across
 * different CPU architectures.
 *
 * Performance characteristics:
 * - O(1) push and peek operations
 * - Wait-free for single producer/single consumer
 * - Memory efficient with configurable capacity
 * - Cache-friendly access patterns
 *
 * Thread safety:
 * - One thread may write (producer)
 * - One thread may read (consumer)
 * - Multiple readers require external synchronization
 *
 * @tparam T Element type, typically float for audio samples
 *
 * Example usage:
 * @code
 * dsp::RingBuffer<float> buffer(1024);
 *
 * // Producer thread
 * float samples[] = {0.1f, 0.2f, 0.3f};
 * buffer.push(samples, 3);
 *
 * // Consumer thread
 * float output[512];
 * buffer.peekLatest(output, 512);
 * @endcode
 *
 * @warning Not safe for multiple producers or multiple consumers
 * @note Capacity is rounded up to (requested_size + 1) for implementation efficiency
 */
template <typename T>
class RingBuffer {
   public:
    /**
     * @brief Constructs a ring buffer with the specified capacity
     *
     * Allocates storage for the ring buffer with one extra element to
     * distinguish between full and empty states without additional flags.
     *
     * @param capacity Maximum number of elements the buffer can hold
     *
     * @pre capacity > 0
     * @post Buffer is empty and ready for use
     * @post Actual storage size is capacity + 1 elements
     *
     * @note The buffer always allocates one extra element internally
     */
    explicit RingBuffer(size_t capacity) : buffer(capacity + 1), head(0), tail(0) {}

    /**
     * @brief Returns the maximum capacity of the buffer
     *
     * @return Maximum number of elements the buffer can hold
     *
     * @note This is the user-requested capacity, not the internal storage size
     */
    size_t capacity() const {
        return buffer.size() - 1;
    }

    /**
     * @brief Returns the current number of elements in the buffer
     *
     * Calculates the number of valid elements currently stored in the buffer.
     * This operation is atomic and thread-safe for single producer/consumer.
     *
     * @return Number of elements available for reading
     *
     * @note Result is a snapshot and may change immediately after return
     */
    size_t size() const {
        auto h = head.load(std::memory_order_acquire);
        auto t = tail.load(std::memory_order_acquire);
        if (h >= t)
            return h - t;
        else
            return buffer.size() - (t - h);
    }

    /**
     * @brief Pushes a single element into the buffer
     *
     * Adds one element to the buffer, overwriting the oldest element if
     * the buffer is full. This operation is wait-free and suitable for
     * real-time audio threads.
     *
     * @param v Element to add to the buffer
     *
     * @post Element is stored in the buffer
     * @post If buffer was full, oldest element is overwritten
     * @post Head pointer is advanced by one position
     *
     * @note Thread-safe for single producer only
     */
    void pushOne(T v) {
        auto h = head.load(std::memory_order_relaxed);
        buffer[h] = v;
        h = (h + 1) % buffer.size();
        if (h == tail.load(std::memory_order_acquire)) {
            tail.store((tail.load(std::memory_order_relaxed) + 1) % buffer.size(), std::memory_order_release);
        }
        head.store(h, std::memory_order_release);
    }

    /**
     * @brief Pushes multiple elements into the buffer
     *
     * Efficiently adds multiple elements to the buffer by calling pushOne()
     * for each element. Elements are added in order, maintaining temporal
     * relationships.
     *
     * @param data Pointer to array of elements to add
     * @param n Number of elements to add
     *
     * @pre data points to valid array of at least n elements
     * @post All n elements are added to the buffer in order
     * @post Oldest elements may be overwritten if buffer becomes full
     *
     * @note Thread-safe for single producer only
     * @see pushOne()
     */
    void push(const T* data, size_t n) {
        for (size_t i = 0; i < n; ++i) pushOne(data[i]);
    }

    /**
     * @brief Reads the latest N samples without removing them
     *
     * Copies up to N of the most recent samples to the output buffer.
     * If fewer than N samples are available, the output is zero-padded
     * at the beginning to maintain temporal alignment.
     *
     * @param out Output buffer to receive the samples
     * @param N Number of samples to read
     *
     * @pre out points to valid array of at least N elements
     * @post out contains the latest N samples (zero-padded if necessary)
     * @post Buffer contents are unchanged (non-destructive read)
     *
     * @note Thread-safe for single consumer only
     * @note If N > size(), leading elements in out are set to T{}
     *
     * Example:
     * @code
     * RingBuffer<float> buf(5);
     * buf.push(data, 3);  // Add 3 samples
     * float output[5];
     * buf.peekLatest(output, 5);  // output[0:1] = 0, output[2:4] = data
     * @endcode
     */
    void peekLatest(T* out, size_t N) const {
        const size_t current = size();
        const size_t toCopy = std::min(N, current);

        size_t h = head.load(std::memory_order_acquire);
        size_t start = (h + buffer.size() - toCopy) % buffer.size();

        // First fill leading zeros if not enough data
        if (N > current) {
            const size_t zeros = N - current;
            std::fill(out, out + zeros, T{});
            out += zeros;
        }

        // Copy contiguous segments
        if (start + toCopy <= buffer.size()) {
            std::memcpy(out, &buffer[start], toCopy * sizeof(T));
        } else {
            size_t first = buffer.size() - start;
            std::memcpy(out, &buffer[start], first * sizeof(T));
            std::memcpy(out + first, &buffer[0], (toCopy - first) * sizeof(T));
        }
    }

   private:
    /**
     * @brief Internal storage for ring buffer elements
     *
     * Contiguous storage that holds the actual data elements. Size is
     * always one larger than the requested capacity to distinguish
     * between full and empty buffer states.
     *
     * @note Vector provides automatic memory management and alignment
     */
    std::vector<T> buffer;

    /**
     * @brief Atomic write index (head pointer)
     *
     * Points to the next position where new data will be written.
     * Advanced after each successful write operation. Uses atomic
     * operations for thread-safe access.
     *
     * @note Only modified by producer thread
     */
    std::atomic<size_t> head;  // write index

    /**
     * @brief Atomic read index (tail pointer)
     *
     * Points to the oldest valid data in the buffer. Advanced when
     * buffer becomes full and oldest data is overwritten. Uses atomic
     * operations for thread-safe access.
     *
     * @note Modified by producer when buffer wraps around
     */
    std::atomic<size_t> tail;  // oldest index

}; // class RingBuffer

}  // namespace dsp
