#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#endif

// Forward declarations
class GpuRenderHook;

/**
 * RAII wrapper for managing JUCE OpenGL context lifecycle.
 * Provides safe attach/detach operations with automatic cleanup.
 *
 * Updated for Task 1.7: Now includes GpuRenderHook management for
 * future GPU effects integration.
 */
class OpenGLManager
#if OSCIL_ENABLE_OPENGL
    : public juce::OpenGLRenderer
#endif
{
public:
    OpenGLManager();
#if OSCIL_ENABLE_OPENGL
    ~OpenGLManager() override;
#else
    ~OpenGLManager();
#endif

    /**
     * Attempts to attach OpenGL context to the specified component.
     * @param component Target component to attach context to
     * @return true if OpenGL is enabled and context attached successfully, false otherwise
     */
    [[nodiscard]] bool attachTo(juce::Component& component);

    /**
     * Detaches OpenGL context from current component if attached.
     * Safe to call multiple times.
     */
    void detach();

    /**
     * @return true if OpenGL is enabled at compile time and context is attached
     */
    [[nodiscard]] bool isOpenGLActive() const;

    /**
     * @return true if OpenGL support is compiled in
     */
    [[nodiscard]] static bool isOpenGLAvailable();

    /**
     * Sets a callback to be invoked when the OpenGL context is created/recreated.
     * @param callback Function to call on context creation
     */
    void setContextCreatedCallback(std::function<void()> callback);

    /**
     * Sets the GPU render hook for future effects processing.
     * @param hook Shared pointer to hook implementation (nullptr for no-op)
     */
    void setGpuRenderHook(std::shared_ptr<GpuRenderHook> hook);

    /**
     * Gets the current GPU render hook.
     * @return Current hook or nullptr if none set
     */
    [[nodiscard]] std::shared_ptr<GpuRenderHook> getGpuRenderHook() const;

#if OSCIL_ENABLE_OPENGL
    // OpenGLRenderer interface
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
#endif

private:
#if OSCIL_ENABLE_OPENGL
    juce::OpenGLContext context;
    std::function<void()> contextCreatedCallback;
    juce::Component* attachedComponent = nullptr;
    std::shared_ptr<GpuRenderHook> gpuRenderHook;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLManager)
};
