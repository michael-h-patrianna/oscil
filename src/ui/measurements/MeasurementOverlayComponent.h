/**
 * @file MeasurementOverlayComponent.h
 * @brief Overlay component for displaying measurement overlays on oscilloscope
 *
 * This file defines the MeasurementOverlayComponent class which manages
 * measurement displays that overlay on top of the main oscilloscope waveform
 * display. It provides adaptive positioning and visibility management for
 * various measurement components.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CorrelationMeterComponent.h"
#include "../../audio/ProcessingModes.h"
#include "../../ui/layout/LayoutManager.h"
#include <memory>
#include <vector>

// Forward declarations
namespace oscil::theme {
    class ThemeManager;
}

namespace oscil::ui::measurements {

/**
 * @class MeasurementOverlayComponent
 * @brief Manages measurement overlays with adaptive positioning
 *
 * The MeasurementOverlayComponent provides a layer for displaying various
 * measurement components (correlation meters, spectrum analyzers, etc.) on
 * top of the oscilloscope display. It handles:
 *
 * - Adaptive positioning based on layout mode
 * - Visibility management for relevant measurements
 * - Processing mode-specific measurement filtering
 * - Theme integration for consistent styling
 * - Smooth transitions between overlay configurations
 *
 * Key Features:
 * - Non-intrusive overlay positioning that doesn't obscure waveforms
 * - Context-sensitive measurement relevance filtering
 * - Smooth animations for measurement transitions
 * - Multiple measurement component management
 * - Layout-aware positioning system
 *
 * The component automatically shows/hides measurements based on:
 * - Current signal processing mode (e.g., correlation only for stereo modes)
 * - Layout mode (different positions for overlay vs grid layouts)
 * - User preferences for measurement visibility
 * - Available screen space and component size
 *
 * Example Usage:
 * @code
 * auto measurementOverlay = std::make_unique<MeasurementOverlayComponent>();
 * measurementOverlay->setThemeManager(&themeManager);
 * measurementOverlay->setLayoutManager(&layoutManager);
 * measurementOverlay->setProcessingMode(SignalProcessingMode::FullStereo);
 * measurementOverlay->updateMeasurements(correlationMetrics);
 * @endcode
 *
 * @see CorrelationMeterComponent
 * @see oscil::ui::layout::LayoutManager
 * @see audio::SignalProcessingMode
 */
class MeasurementOverlayComponent : public juce::Component {
public:
    /**
     * @brief Configuration for overlay behavior and positioning
     */
    struct OverlayConfig {
        bool showCorrelationMeter = true;          ///< Show correlation meter when relevant
        bool showStereoWidthMeter = true;          ///< Show stereo width measurement
        bool adaptToLayoutMode = true;             ///< Automatically position based on layout
        bool hideWhenNotRelevant = true;           ///< Hide measurements for irrelevant modes
        float overlayOpacity = 0.9F;               ///< Overlay background opacity
        int overlayPadding = 8;                    ///< Padding around overlay content
        bool enableAnimations = true;              ///< Enable smooth transitions
        float animationDurationMs = 200.0F;       ///< Animation duration in milliseconds
    };

    /**
     * @brief Overlay positioning modes
     */
    enum class PositionMode {
        TopLeft,        ///< Top-left corner positioning
        TopRight,       ///< Top-right corner positioning
        BottomLeft,     ///< Bottom-left corner positioning
        BottomRight,    ///< Bottom-right corner positioning
        Center,         ///< Center positioning (for large displays)
        Adaptive        ///< Automatically choose best position based on layout
    };

    /**
     * @brief Constructs measurement overlay with default configuration
     */
    MeasurementOverlayComponent();

    /**
     * @brief Constructs measurement overlay with custom configuration
     *
     * @param config Custom overlay configuration
     */
    explicit MeasurementOverlayComponent(const OverlayConfig& config);

    /**
     * @brief Destructor
     */
    ~MeasurementOverlayComponent() override = default;

    // === Configuration ===

    /**
     * @brief Sets the overlay configuration
     *
     * @param config New overlay configuration
     */
    void setConfig(const OverlayConfig& config);

    /**
     * @brief Gets current overlay configuration
     */
    auto getConfig() const -> const OverlayConfig& { return overlayConfig; }

    /**
     * @brief Sets the theme manager for consistent styling
     *
     * @param manager Pointer to theme manager (nullptr for default styling)
     */
    void setThemeManager(oscil::theme::ThemeManager* manager);

    /**
     * @brief Sets the layout manager for adaptive positioning
     *
     * @param manager Pointer to layout manager (nullptr to disable adaptation)
     */
    void setLayoutManager(oscil::ui::layout::LayoutManager* manager);

    /**
     * @brief Sets the current signal processing mode
     *
     * Used to determine which measurements are relevant and should be displayed.
     *
     * @param mode Current signal processing mode
     */
    void setProcessingMode(audio::SignalProcessingMode mode);

    /**
     * @brief Sets the overlay position mode
     *
     * @param mode Position mode for overlay placement
     */
    void setPositionMode(PositionMode mode);

    // === Measurement Updates ===

    /**
     * @brief Updates correlation measurements for display
     *
     * @param metrics Latest correlation metrics from SignalProcessor
     */
    void updateCorrelationMetrics(const audio::CorrelationMetrics& metrics);

    /**
     * @brief Updates stereo width measurements
     *
     * @param width Current stereo width value (0.0 to 2.0)
     */
    void updateStereoWidth(float width);

    // === Visibility Management ===

    /**
     * @brief Sets visibility for correlation meter
     *
     * @param visible Whether correlation meter should be visible
     */
    void setCorrelationMeterVisible(bool visible);

    /**
     * @brief Sets visibility for stereo width meter
     *
     * @param visible Whether stereo width meter should be visible
     */
    void setStereoWidthMeterVisible(bool visible);

    /**
     * @brief Sets visibility for all measurements
     *
     * @param visible Whether all measurements should be visible
     */
    void setAllMeasurementsVisible(bool visible);

    /**
     * @brief Checks if any measurements are currently visible
     *
     * @return true if at least one measurement is visible
     */
    auto hasMeasurementsVisible() const -> bool;

    // === Component Overrides ===

    /**
     * @brief Renders the measurement overlay
     *
     * @param graphics JUCE Graphics context for drawing
     */
    void paint(juce::Graphics& graphics) override;

    /**
     * @brief Handles component resize events
     */
    void resized() override;

    // === Utility ===

    /**
     * @brief Gets the preferred size for optimal overlay display
     *
     * @return Recommended size based on enabled measurements
     */
    auto getPreferredBounds() const -> juce::Rectangle<int>;

    /**
     * @brief Checks if overlay should be visible for current processing mode
     *
     * @return true if current mode supports any enabled measurements
     */
    auto isRelevantForCurrentMode() const -> bool;

private:
    // === Configuration ===
    OverlayConfig overlayConfig;
    PositionMode positionMode = PositionMode::Adaptive;

    // === Dependencies ===
    oscil::theme::ThemeManager* themeManager = nullptr;
    oscil::ui::layout::LayoutManager* layoutManager = nullptr;
    audio::SignalProcessingMode currentMode = audio::SignalProcessingMode::FullStereo;

    // === Measurement Components ===
    std::unique_ptr<CorrelationMeterComponent> correlationMeter;

    // === Layout and Visibility ===
    struct OverlayLayout {
        juce::Rectangle<int> correlationMeterBounds;
        juce::Rectangle<int> stereoWidthBounds;
        juce::Rectangle<int> overlayBounds;
        bool isValid = false;
    } cachedLayout;

    bool correlationMeterVisible = true;
    bool stereoWidthMeterVisible = true;

    // === Animation State ===
    struct AnimationState {
        bool isAnimating = false;
        float animationProgress = 1.0F;
        juce::Rectangle<int> startBounds;
        juce::Rectangle<int> targetBounds;
        uint32_t animationStartTime = 0;
    } animationState;

    // === Private Methods ===

    /**
     * @brief Initializes measurement components
     */
    void initializeMeasurementComponents();

    /**
     * @brief Updates cached layout calculations
     */
    void updateLayout();

    /**
     * @brief Calculates optimal overlay position based on current mode
     *
     * @return Optimal overlay bounds
     */
    auto calculateOptimalPosition() -> juce::Rectangle<int>;

    /**
     * @brief Gets adaptive position based on layout mode
     *
     * @return Recommended position mode for current layout
     */
    auto getAdaptivePositionMode() const -> PositionMode;

    /**
     * @brief Updates visibility of child components based on configuration
     */
    void updateChildVisibility();

    /**
     * @brief Handles smooth transitions between overlay states
     */
    void updateAnimations();

    /**
     * @brief Starts animation to new overlay configuration
     *
     * @param targetBounds Target bounds for animation
     */
    void startAnimation(const juce::Rectangle<int>& targetBounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeasurementOverlayComponent)
};

} // namespace oscil::ui::measurements
