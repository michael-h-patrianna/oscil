#pragma once

#include <vector>
#include <cstddef>
#include <algorithm>

namespace oscil::render {

/**
 * High-performance waveform decimation processor.
 * Reduces sample count using min/max algorithm when pixel density is low.
 */
class DecimationProcessor {
public:
    /**
     * Decimation result containing processed samples.
     */
    struct DecimationResult {
        const float* samples = nullptr;
        size_t sampleCount = 0;
        bool wasDecimated = false;
    };

    DecimationProcessor() = default;
    ~DecimationProcessor() = default;

    /**
     * Process samples with automatic decimation based on pixel density.
     * @param inputSamples Input sample array
     * @param inputCount Number of input samples
     * @param targetPixels Target pixel count (width of display area)
     * @param threshold Samples per pixel threshold to trigger decimation (default: 2.0)
     * @return DecimationResult containing processed data
     */
    DecimationResult process(const float* inputSamples,
                           size_t inputCount,
                           int targetPixels,
                           float threshold = 2.0f);

    /**
     * Reset internal buffers. Call when changing buffer sizes.
     */
    void reset();

private:
    // Pre-allocated buffer for decimated samples (avoid allocations)
    std::vector<float> decimatedBuffer;

    /**
     * Perform min/max decimation on samples.
     * @param inputSamples Input sample array
     * @param inputCount Number of input samples
     * @param outputSamples Output buffer
     * @param targetPixels Desired output sample count
     * @return Actual number of output samples
     */
    size_t performDecimation(const float* inputSamples,
                           size_t inputCount,
                           float* outputSamples,
                           int targetPixels);
};

} // namespace oscil::render
