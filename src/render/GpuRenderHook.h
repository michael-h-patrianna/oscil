/**
 * @file GpuRenderHook.h
 * @brief Abstract interface for GPU-accelerated post-processing effects
 *
 * This file defines the GpuRenderHook interface which provides extension points
 * for GPU-accelerated visual effects in the oscilloscope rendering pipeline.
 * The hook system allows for future enhancement with post-processing effects
 * while maintaining zero overhead when OpenGL is disabled.
 *
 * The design supports conditional compilation and runtime detection, ensuring
 * the plugin functions correctly on systems without OpenGL support while
 * providing enhanced visual capabilities on supported hardware.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#include <atomic>
#endif

/**
 * @class GpuRenderHook
 * @brief Abstract interface for GPU-accelerated post-processing effects
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
 *
 * Design Patterns:
 * - Interface segregation: Minimal interface for hook implementations
 * - RAII: Frame lifecycle managed through begin/end pairs
 * - Zero-cost abstraction: No overhead when features are unused
 * - Conditional compilation: OpenGL code only compiled when enabled
 *
 * Future Extensions:
 * This interface is designed to support planned enhancements including:
 * - Shader-based waveform rendering (Task 4.5)
 * - Real-time post-processing effects (Task 4.6)
 * - Advanced visualization modes
 * - Performance optimizations through GPU compute
 *
 * Example Implementation:
 * @code
 * class BloomEffect : public GpuRenderHook {
 * public:
 *     void beginFrame(const FrameContext& context) override {
 *         // Set up render targets for bloom
 *     }
 *
 *     void applyPostFx() override {
 *         // Apply bloom shader
 *     }
 * };
 * @endcode
 *
 * @see OpenGLManager for context management
 * @see OscilloscopeComponent for integration point
 */
class GpuRenderHook
{
public:
    /**
     * @brief Default constructor for GPU render hook
     *
     * Initializes the hook with default state. Derived classes should
     * initialize their OpenGL resources in their constructors or during
     * the first beginFrame() call.
     */
    GpuRenderHook() = default;

    /**
     * @brief Virtual destructor for proper cleanup
     *
     * Ensures derived classes can properly clean up OpenGL resources.
     * Derived classes should release all GPU resources in their destructors.
     */
    virtual ~GpuRenderHook() = default;

    // Non-copyable, non-movable for safety
    GpuRenderHook(const GpuRenderHook&) = delete;
    GpuRenderHook& operator=(const GpuRenderHook&) = delete;
    GpuRenderHook(GpuRenderHook&&) = delete;
    GpuRenderHook& operator=(GpuRenderHook&&) = delete;

    /**
     * @brief Called at the beginning of the paint cycle when OpenGL context is active
     *
     * This method is invoked at the start of each frame when GPU acceleration
     * is available. Use this to set up render targets, update uniforms, or
     * prepare any GPU resources needed for the frame.
     *
     * @param bounds The component bounds for this frame in pixels
     * @param frameCounter Current frame number for time-based effects
     *
     * @pre OpenGL context is current and valid
     * @post GPU resources are prepared for frame rendering
     *
     * @note Called on the message thread only
     * @note Should complete in <0.1ms for real-time performance
     * @note May be called with different bounds each frame
     */
    virtual void beginFrame(const juce::Rectangle<float>& bounds, uint64_t frameCounter) = 0;

    /**
     * @brief Called after waveform geometry is prepared but before drawing
     *
     * Allows for custom waveform rendering or geometry modification.
     * This is the point where the CPU has prepared waveform paths and
     * is about to render them. GPU-based waveform rendering can intercept
     * this stage to render waveforms directly on the GPU.
     *
     * @param waveformCount Number of waveforms about to be drawn
     *
     * @pre Waveform geometry has been calculated
     * @pre OpenGL context is current and valid
     * @post Custom waveform rendering completed (if applicable)
     *
     * @note Implementation may choose to render waveforms or delegate to CPU
     * @note Should complete in <0.5ms for typical waveform counts
     */
    virtual void drawWaveforms(int waveformCount) = 0;

    /**
     * @brief Called after waveforms are drawn but before overlays
     *
     * This is the main post-processing hook for visual effects. The
     * waveforms have been rendered and this is the opportunity to apply
     * GPU-based effects like bloom, blur, color grading, etc.
     *
     * @param renderTarget The current framebuffer/texture (nullptr if using default)
     *
     * @pre Waveforms have been rendered to the framebuffer
     * @pre OpenGL context is current and valid
     * @post Post-processing effects have been applied
     *
     * @note This is where most visual effects should be implemented
     * @note Should complete in <1ms for complex effects
     * @note Effects should be additive and preserve original content
     */
    virtual void applyPostFx(void* renderTarget) = 0;

    /**
     * @brief Called at the end of the paint cycle
     *
     * Perform any cleanup or final compositing here. This is the last
     * opportunity to affect the rendered output before it's presented
     * to the user.
     *
     * @pre All rendering for the frame is complete
     * @post GPU resources are cleaned up or prepared for next frame
     *
     * @note Use for cleanup, statistics collection, or final composition
     * @note Should complete in <0.1ms
     */
    virtual void endFrame() = 0;

    /**
     * @brief Query whether this hook performs any actual rendering
     *
     * Allows the rendering system to skip hook invocation entirely when
     * no effects are active. This optimization can save significant
     * performance when GPU effects are disabled.
     *
     * @return true if hook should be invoked, false to skip entirely
     *
     * @note Should be very fast (<0.01ms) as it's called every frame
     * @note Return value may change dynamically based on user settings
     * @note When false, all hook methods are bypassed
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
