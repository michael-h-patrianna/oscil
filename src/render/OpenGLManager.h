/**
 * @file OpenGLManager.h
 * @brief RAII wrapper for JUCE OpenGL context management and GPU render hooks
 *
 * This file contains the OpenGLManager class that provides safe lifecycle
 * management for JUCE OpenGL contexts with optional GPU render hook integration
 * for future visual effects processing.
 *
 * @author Oscil Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#endif

// Forward declarations
class GpuRenderHook;

/**
 * @class OpenGLManager
 * @brief RAII wrapper for managing JUCE OpenGL context lifecycle and GPU hooks
 *
 * Provides safe attach/detach operations for OpenGL contexts with automatic
 * cleanup and optional GPU render hook integration for future effects processing.
 * The class is designed to gracefully handle both OpenGL-enabled and disabled
 * builds through conditional compilation.
 *
 * Key features:
 * - RAII-based OpenGL context lifecycle management
 * - Safe attach/detach operations with error handling
 * - GPU render hook integration for future effects
 * - Conditional compilation support for OpenGL-free builds
 * - Automatic context recreation handling
 * - Thread-safe context state management
 *
 * The manager serves as a bridge between JUCE's OpenGL system and Oscil's
 * custom GPU effects pipeline. When OpenGL is disabled at compile time,
 * the class becomes a lightweight no-op wrapper that maintains API compatibility.
 *
 * Usage scenarios:
 * 1. **Performance Enhancement**: GPU-accelerated waveform rendering
 * 2. **Visual Effects**: Post-processing effects like persistence trails and glow
 * 3. **Future Extensibility**: User-customizable fragment shaders
 * 4. **Graceful Fallback**: Automatic CPU rendering when GPU is unavailable
 *
 * Thread safety:
 * - OpenGL operations are confined to the message thread
 * - Context state queries are thread-safe
 * - Hook management operations are thread-safe
 *
 * Example usage:
 * @code
 * OpenGLManager glManager;
 *
 * // Attempt to enable OpenGL acceleration
 * if (glManager.attachTo(myComponent)) {
 *     // OpenGL successfully attached
 *     auto hook = std::make_shared<MyGpuRenderHook>();
 *     glManager.setGpuRenderHook(hook);
 * } else {
 *     // Fall back to CPU rendering
 * }
 * @endcode
 *
 * @see GpuRenderHook
 * @see OscilloscopeComponent
 * @note When OSCIL_ENABLE_OPENGL is false, most methods become no-ops
 */
class OpenGLManager
#if OSCIL_ENABLE_OPENGL
    : public juce::OpenGLRenderer
#endif
{
public:
    /**
     * @brief Constructs the OpenGL manager
     *
     * Initializes the manager in a detached state. OpenGL context creation
     * is deferred until attachTo() is called with a valid component.
     *
     * @post Manager is in detached state
     * @post No OpenGL context is active
     * @post No GPU render hooks are installed
     */
    OpenGLManager();

#if OSCIL_ENABLE_OPENGL
    /**
     * @brief Destructor (OpenGL enabled build)
     *
     * Automatically detaches any active OpenGL context and cleans up
     * all associated resources. Overrides juce::OpenGLRenderer destructor.
     *
     * @post OpenGL context is safely detached
     * @post All GPU resources are released
     * @post Render hooks are properly destroyed
     */
    ~OpenGLManager() override;
#else
    /**
     * @brief Destructor (OpenGL disabled build)
     *
     * No-op destructor for builds compiled without OpenGL support.
     * Maintains API compatibility while avoiding OpenGL dependencies.
     */
    ~OpenGLManager();
#endif

    /**
     * @brief Attempts to attach OpenGL context to the specified component
     *
     * Creates and attaches a JUCE OpenGL context to the target component,
     * enabling GPU-accelerated rendering. Gracefully handles cases where
     * OpenGL is unavailable or context creation fails.
     *
     * @param component Target component to receive the OpenGL context
     *
     * @return true if OpenGL is enabled and context attached successfully
     * @retval false if OpenGL is disabled, unavailable, or context creation failed
     *
     * @pre component must be a valid, displayable JUCE component
     * @post If successful, component has active OpenGL context
     * @post If successful, render callbacks will be invoked during repaints
     *
     * @note Safe to call multiple times - will detach existing context first
     * @note Automatically installs this manager as the OpenGL renderer
     * @see detach()
     */
    [[nodiscard]] bool attachTo(juce::Component& component);

    /**
     * @brief Detaches OpenGL context from current component
     *
     * Safely removes the OpenGL context from any attached component and
     * releases all associated GPU resources. Safe to call multiple times
     * or when no context is attached.
     *
     * @post No OpenGL context is attached
     * @post Component reverts to CPU-based rendering
     * @post GPU resources are properly released
     *
     * @note Automatically called by destructor
     * @note Safe to call from any thread
     * @see attachTo()
     */
    void detach();

    /**
     * @brief Checks if OpenGL context is currently active
     *
     * @return true if OpenGL is compiled in AND context is successfully attached
     * @retval false if OpenGL is disabled, context failed, or not attached
     *
     * @note Returns actual operational state, not just compilation flags
     * @note Thread-safe query operation
     */
    [[nodiscard]] bool isOpenGLActive() const;

    /**
     * @brief Checks if OpenGL support is available at compile time
     *
     * @return true if OSCIL_ENABLE_OPENGL was defined during compilation
     * @retval false if OpenGL support was disabled at compile time
     *
     * @note This is a compile-time constant check
     * @note Use isOpenGLActive() to check runtime state
     */
    [[nodiscard]] static bool isOpenGLAvailable();

    /**
     * @brief Sets callback for OpenGL context creation events
     *
     * Registers a callback function that will be invoked whenever the OpenGL
     * context is created or recreated. Useful for initializing GPU resources
     * that need to be recreated when the context changes.
     *
     * @param callback Function to call on context creation (nullptr to clear)
     *
     * @post Callback will be invoked on next context creation
     * @post Previous callback is replaced
     *
     * @note Callback is invoked from the message thread
     * @note Context may be recreated due to display changes or driver updates
     *
     * Example:
     * @code
     * manager.setContextCreatedCallback([this]() {
     *     // Initialize shaders, textures, etc.
     *     initializeGpuResources();
     * });
     * @endcode
     */
    void setContextCreatedCallback(std::function<void()> callback);

    /**
     * @brief Sets the GPU render hook for effects processing
     *
     * Installs a GPU render hook that will be invoked during OpenGL rendering
     * operations. The hook enables custom GPU effects like persistence trails,
     * glow effects, and user-defined fragment shaders.
     *
     * @param hook Shared pointer to hook implementation (nullptr for no-op)
     *
     * @post Hook will be invoked during OpenGL render cycles
     * @post Previous hook is replaced (if any)
     * @post Hook lifetime is managed by shared_ptr
     *
     * @note Hook is only active when OpenGL context is attached
     * @note No-op if OpenGL is disabled at compile time
     * @see GpuRenderHook
     */
    void setGpuRenderHook(std::shared_ptr<GpuRenderHook> hook);

    /**
     * @brief Gets the currently installed GPU render hook
     *
     * @return Current hook instance or nullptr if none installed
     *
     * @note Return value may be nullptr if no hook is set
     * @note Returned shared_ptr maintains reference to hook
     */
    [[nodiscard]] std::shared_ptr<GpuRenderHook> getGpuRenderHook() const;

#if OSCIL_ENABLE_OPENGL
    //==============================================================================
    // JUCE OpenGLRenderer Interface Implementation

    /**
     * @brief Called when a new OpenGL context is created
     *
     * JUCE callback invoked when the OpenGL context is created or recreated.
     * Handles context initialization and invokes user-defined callbacks.
     *
     * @post OpenGL context is ready for use
     * @post Context creation callback is invoked (if set)
     * @post GPU render hook is notified of new context
     *
     * @note Called automatically by JUCE - do not call directly
     * @note May be called multiple times during component lifetime
     */
    void newOpenGLContextCreated() override;

    /**
     * @brief Called to perform OpenGL rendering
     *
     * JUCE callback invoked during component repaints when OpenGL is active.
     * Coordinates with GPU render hooks to perform custom rendering operations.
     *
     * @note Called automatically by JUCE during repaints
     * @note Runs on the message thread within OpenGL context
     * @note Do not call directly - use component.repaint() instead
     */
    void renderOpenGL() override;

    /**
     * @brief Called when OpenGL context is about to be destroyed
     *
     * JUCE callback invoked before the OpenGL context is destroyed.
     * Provides opportunity to clean up GPU resources before context loss.
     *
     * @post GPU resources should be released
     * @post Context will be invalid after this call returns
     *
     * @note Called automatically by JUCE - do not call directly
     * @note Last chance to access OpenGL state before destruction
     */
    void openGLContextClosing() override;
#endif

private:
    //==============================================================================
    // Private Member Variables

#if OSCIL_ENABLE_OPENGL
    /**
     * @brief The JUCE OpenGL context instance
     *
     * Core OpenGL context that manages the GPU rendering state.
     * Automatically handles context creation, destruction, and state management.
     */
    juce::OpenGLContext context;

    /**
     * @brief User-defined callback for context creation events
     *
     * Optional callback function invoked when the OpenGL context is created
     * or recreated. Allows user code to initialize GPU resources.
     */
    std::function<void()> contextCreatedCallback;

    /**
     * @brief Pointer to the component with attached OpenGL context
     *
     * Tracks which component currently has the OpenGL context attached.
     * Used for safe detachment and state management.
     *
     * @note nullptr when no context is attached
     */
    juce::Component* attachedComponent = nullptr;

    /**
     * @brief Shared pointer to active GPU render hook
     *
     * Manages the lifecycle of the GPU render hook used for custom
     * effects processing during OpenGL rendering operations.
     *
     * @note nullptr when no hook is installed
     */
    std::shared_ptr<GpuRenderHook> gpuRenderHook;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLManager)
};
