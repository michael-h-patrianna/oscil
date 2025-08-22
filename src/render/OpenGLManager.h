#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#endif

/**
 * RAII wrapper for managing JUCE OpenGL context lifecycle.
 * Provides safe attach/detach operations with automatic cleanup.
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
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLManager)
};
