#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace dsp {

template <typename T>
class RingBuffer {
   public:
    explicit RingBuffer(size_t capacity) : buffer(capacity + 1), head(0), tail(0) {}

    size_t capacity() const {
        return buffer.size() - 1;
    }

    // Current number of elements stored
    size_t size() const {
        auto h = head.load(std::memory_order_acquire);
        auto t = tail.load(std::memory_order_acquire);
        if (h >= t)
            return h - t;
        else
            return buffer.size() - (t - h);
    }

    // Push a single element (overwrite oldest if full)
    void pushOne(T v) {
        auto h = head.load(std::memory_order_relaxed);
        buffer[h] = v;
        h = (h + 1) % buffer.size();
        if (h == tail.load(std::memory_order_acquire)) {
            tail.store((tail.load(std::memory_order_relaxed) + 1) % buffer.size(), std::memory_order_release);
        }
        head.store(h, std::memory_order_release);
    }

    // Push many elements
    void push(const T* data, size_t n) {
        for (size_t i = 0; i < n; ++i) pushOne(data[i]);
    }

    // Read latest N samples without popping; if N > size, fill prefix with zeros
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
    std::vector<T> buffer;
    std::atomic<size_t> head;  // write index
    std::atomic<size_t> tail;  // oldest index
};

}  // namespace dsp
