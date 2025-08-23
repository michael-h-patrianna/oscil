/**
 * @file MeasurementData.cpp
 * @brief Implementation of measurement data transfer system
 */

#include "MeasurementData.h"
#include <juce_core/juce_core.h>

namespace audio {

//==============================================================================
MeasurementDataBridge::MeasurementDataBridge()
    : writeData(std::make_unique<MeasurementData>())
    , readData(std::make_unique<MeasurementData>()) {
}

void MeasurementDataBridge::pushMeasurementData(const MeasurementData& data) {
    // Copy data to write buffer
    *writeData = data;

    // Mark data as ready (atomic)
    dataReady.store(true, std::memory_order_release);

    // Increment counter
    measurementsPushed.fetch_add(1, std::memory_order_relaxed);
}

bool MeasurementDataBridge::pullLatestMeasurements(MeasurementData& outData) {
    // Check if new data is available
    bool hasNewData = dataReady.exchange(false, std::memory_order_acquire);

    if (hasNewData) {
        // Swap buffers (lock-free)
        writeData.swap(readData);

        // Copy data to output
        outData = *readData;

        // Increment counter
        measurementsPulled.fetch_add(1, std::memory_order_relaxed);
    }

    return hasNewData;
}

void MeasurementDataBridge::resetStats() {
    measurementsPushed.store(0, std::memory_order_relaxed);
    measurementsPulled.store(0, std::memory_order_relaxed);
}

} // namespace audio
