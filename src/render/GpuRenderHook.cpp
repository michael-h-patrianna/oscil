#include "GpuRenderHook.h"

#if OSCIL_ENABLE_OPENGL && defined(OSCIL_DEBUG_HOOKS)
#include <iostream>

void DebugGpuRenderHook::beginFrame(const juce::Rectangle<float>& bounds, uint64_t frameCounter) {
    beginFrameCount.fetch_add(1, std::memory_order_relaxed);

    // Log occasionally for debugging (every 120 frames ~2 seconds at 60fps)
    if (frameCounter % 120 == 0) {
        std::cout << "[DEBUG GPU HOOK] beginFrame - bounds: "
                  << bounds.getWidth() << "x" << bounds.getHeight()
                  << ", frame: " << frameCounter << std::endl;
    }
}

void DebugGpuRenderHook::drawWaveforms(int waveformCount) {
    drawWaveformsCount.fetch_add(1, std::memory_order_relaxed);

    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 120 == 0) {
        std::cout << "[DEBUG GPU HOOK] drawWaveforms - count: " << waveformCount << std::endl;
    }
}

void DebugGpuRenderHook::applyPostFx(void* renderTarget) {
    applyPostFxCount.fetch_add(1, std::memory_order_relaxed);

    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 120 == 0) {
        std::cout << "[DEBUG GPU HOOK] applyPostFx - target: "
                  << (renderTarget ? "custom" : "default") << std::endl;
    }
}

void DebugGpuRenderHook::endFrame() {
    endFrameCount.fetch_add(1, std::memory_order_relaxed);
}

void DebugGpuRenderHook::resetCounters() {
    beginFrameCount.store(0, std::memory_order_relaxed);
    drawWaveformsCount.store(0, std::memory_order_relaxed);
    applyPostFxCount.store(0, std::memory_order_relaxed);
    endFrameCount.store(0, std::memory_order_relaxed);

    std::cout << "[DEBUG GPU HOOK] Counters reset" << std::endl;
}

#endif // OSCIL_ENABLE_OPENGL && OSCIL_DEBUG_HOOKS
