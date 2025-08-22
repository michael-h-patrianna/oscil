#include "DecimationProcessor.h"
#include <algorithm>
#include <cmath>

namespace oscil::render {

DecimationProcessor::DecimationResult DecimationProcessor::process(const float* inputSamples,
                                                                  size_t inputCount,
                                                                  int targetPixels,
                                                                  float threshold) {
    DecimationResult result;

    if (inputSamples == nullptr || inputCount == 0 || targetPixels <= 0) {
        return result;
    }

    // Calculate samples per pixel ratio
    float samplesPerPixel = static_cast<float>(inputCount) / static_cast<float>(targetPixels);

    // If density is low enough, return original data
    if (samplesPerPixel <= threshold) {
        result.samples = inputSamples;
        result.sampleCount = inputCount;
        result.wasDecimated = false;
        return result;
    }

    // Perform decimation
    // Ensure buffer is large enough (2 samples per pixel for min/max)
    size_t requiredSize = static_cast<size_t>(targetPixels) * 2;
    if (decimatedBuffer.size() < requiredSize) {
        decimatedBuffer.resize(requiredSize);
    }

    size_t outputCount = performDecimation(inputSamples, inputCount,
                                         decimatedBuffer.data(), targetPixels);

    result.samples = decimatedBuffer.data();
    result.sampleCount = outputCount;
    result.wasDecimated = true;
    return result;
}

void DecimationProcessor::reset() {
    decimatedBuffer.clear();
    decimatedBuffer.shrink_to_fit();
}

size_t DecimationProcessor::performDecimation(const float* inputSamples,
                                             size_t inputCount,
                                             float* outputSamples,
                                             int targetPixels) {
    if (targetPixels <= 0) {
        return 0;
    }

    size_t samplesPerPixel = inputCount / static_cast<size_t>(targetPixels);
    if (samplesPerPixel < 2) {
        // Not enough samples to decimate meaningfully, copy directly
        size_t copyCount = std::min(inputCount, static_cast<size_t>(targetPixels));
        std::copy(inputSamples, inputSamples + copyCount, outputSamples);
        return copyCount;
    }

    size_t outputIndex = 0;

    // For each pixel, find min and max in the corresponding sample range
    for (int pixel = 0; pixel < targetPixels && outputIndex < static_cast<size_t>(targetPixels) * 2; ++pixel) {
        size_t startIdx = (static_cast<size_t>(pixel) * inputCount) / static_cast<size_t>(targetPixels);
        size_t endIdx = ((static_cast<size_t>(pixel) + 1) * inputCount) / static_cast<size_t>(targetPixels);

        if (startIdx >= inputCount) break;
        endIdx = std::min(endIdx, inputCount);

        if (startIdx >= endIdx) continue;

        // Find min and max in this range
        float minVal = inputSamples[startIdx];
        float maxVal = inputSamples[startIdx];

        for (size_t i = startIdx + 1; i < endIdx; ++i) {
            float sample = inputSamples[i];
            minVal = std::min(minVal, sample);
            maxVal = std::max(maxVal, sample);
        }

        // Add both min and max to preserve peak information
        if (outputIndex < static_cast<size_t>(targetPixels) * 2) {
            outputSamples[outputIndex++] = minVal;
        }
        if (outputIndex < static_cast<size_t>(targetPixels) * 2) {
            outputSamples[outputIndex++] = maxVal;
        }
    }

    return outputIndex;
}

} // namespace oscil::render
