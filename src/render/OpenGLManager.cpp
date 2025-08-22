#include "OpenGLManager.h"

#if OSCIL_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#endif

OpenGLManager::OpenGLManager() = default;

OpenGLManager::~OpenGLManager() {
    detach();
}

bool OpenGLManager::attachTo(juce::Component& component) {
#if OSCIL_ENABLE_OPENGL
    if (attachedComponent == &component && context.isAttached()) {
        return true; // Already attached to this component
    }

    // Detach from previous component if any
    detach();

    try {
        context.setRenderer(this);
        context.attachTo(component);
        attachedComponent = &component;

        return true;
    } catch (const std::exception& /*e*/) {
        return false;
    } catch (...) {
        return false;
    }
#else
    juce::ignoreUnused(component);
    return false;
#endif
}

void OpenGLManager::detach() {
#if OSCIL_ENABLE_OPENGL
    if (attachedComponent != nullptr) {
        try {
            context.detach();
            attachedComponent = nullptr;
        } catch (...) {
            // Ignore errors during detach
        }
    }
#endif
}

bool OpenGLManager::isOpenGLActive() const {
#if OSCIL_ENABLE_OPENGL
    return attachedComponent != nullptr && context.isActive();
#else
    return false;
#endif
}

bool OpenGLManager::isOpenGLAvailable() {
#if OSCIL_ENABLE_OPENGL
    return true;
#else
    return false;
#endif
}

void OpenGLManager::setContextCreatedCallback(std::function<void()> callback) {
#if OSCIL_ENABLE_OPENGL
    contextCreatedCallback = std::move(callback);
#else
    juce::ignoreUnused(callback);
#endif
}

#if OSCIL_ENABLE_OPENGL
void OpenGLManager::newOpenGLContextCreated() {
    // Log basic OpenGL info
    juce::gl::loadFunctions();

    if (contextCreatedCallback) {
        contextCreatedCallback();
    }
}

void OpenGLManager::renderOpenGL() {
    // No custom OpenGL rendering in this scaffold - JUCE handles compositing
    // This method is called by JUCE during the render cycle
}

void OpenGLManager::openGLContextClosing() {
    // Cleanup if needed
}
#endif
