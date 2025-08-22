#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#include <atomic>
#endif

/**
 * Abstract interface for GPU-accelerated post-processing effects.
 *
 * This hook provides integration points for future GPU effects (Task 4.5, 4.6)
 * without requiring changes to the core oscilloscope rendering logic.
 *
 * The hook is invoked only when an OpenGL context is active. When OpenGL is
 * disabled or unavailable, the hook is bypassed entirely for zero overhead.
 *
 * Lifecycle:
 * 1. beginFrame() - Called at start of paint cycle when OpenGL active
 * 2. drawWaveforms() - Called after waveform geometry is prepared
 * 3. applyPostFx() - Called after waveforms are drawn, before overlays
 * 4. endFrame() - Called at end of paint cycle
 *
 * Performance Requirements:
 * - Zero overhead when disabled (<0.1% performance delta)
 * - No heap allocations during hook lifecycle
 * - Hook invocation overhead <0.01ms per frame when enabled
 */
class GpuRenderHook
{
public:
    GpuRenderHook() = default;
    virtual ~GpuRenderHook() = default;

    // Non-copyable, non-movable for safety
    GpuRenderHook(const GpuRenderHook&) = delete;
    GpuRenderHook& operator=(const GpuRenderHook&) = delete;
    GpuRenderHook(GpuRenderHook&&) = delete;
    GpuRenderHook& operator=(GpuRenderHook&&) = delete;

    /**
     * Called at the beginning of the paint cycle when OpenGL context is active.
     * @param bounds The component bounds for this frame
     * @param frameCounter Current frame number for time-based effects
     */
    virtual void beginFrame(const juce::Rectangle<float>& bounds, uint64_t frameCounter) = 0;

    /**
     * Called after waveform geometry is prepared but before drawing.
     * Allows for custom waveform rendering or geometry modification.
     * @param waveformCount Number of waveforms about to be drawn
     */
    virtual void drawWaveforms(int waveformCount) = 0;

    /**
     * Called after waveforms are drawn but before overlays (grid, cursors).
     * This is the main post-processing hook for visual effects.
     * @param renderTarget The current framebuffer/texture (nullptr if using default)
     */
    virtual void applyPostFx(void* renderTarget) = 0;

    /**
     * Called at the end of the paint cycle.
     * Perform any cleanup or final compositing here.
     */
    virtual void endFrame() = 0;

    /**
     * Query whether this hook performs any actual rendering.
     * @return true if hook should be invoked, false to skip entirely
     */
    [[nodiscard]] virtual auto isActive() const -> bool = 0;
};

/**
 * Default no-op implementation of GpuRenderHook.
 * Used when OpenGL is disabled or no custom effects are installed.
 *
 * This implementation should compile to zero overhead when optimized.
 */
class NoOpGpuRenderHook : public GpuRenderHook
{
public:
    void beginFrame(const juce::Rectangle<float>& /*bounds*/, uint64_t /*frameCounter*/) override {}
    void drawWaveforms(int /*waveformCount*/) override {}
    void applyPostFx(void* /*renderTarget*/) override {}
    void endFrame() override {}
    [[nodiscard]] auto isActive() const -> bool override { return false; }
};

#if OSCIL_ENABLE_OPENGL && defined(OSCIL_DEBUG_HOOKS)
/**
 * Debug implementation that counts hook invocations.
 * Used for testing and validation during development.
 */
class DebugGpuRenderHook : public GpuRenderHook
{
public:
    void beginFrame(const juce::Rectangle<float>& bounds, uint64_t frameCounter) override;
    void drawWaveforms(int waveformCount) override;
    void applyPostFx(void* renderTarget) override;
    void endFrame() override;
    [[nodiscard]] auto isActive() const -> bool override { return true; }

    // Debug accessors
    [[nodiscard]] auto getBeginFrameCount() const -> uint64_t { return beginFrameCount.load(); }
    [[nodiscard]] auto getDrawWaveformsCount() const -> uint64_t { return drawWaveformsCount.load(); }
    [[nodiscard]] auto getApplyPostFxCount() const -> uint64_t { return applyPostFxCount.load(); }
    [[nodiscard]] auto getEndFrameCount() const -> uint64_t { return endFrameCount.load(); }

    void resetCounters();

private:
    std::atomic<uint64_t> beginFrameCount{0};
    std::atomic<uint64_t> drawWaveformsCount{0};
    std::atomic<uint64_t> applyPostFxCount{0};
    std::atomic<uint64_t> endFrameCount{0};
};
#endif
