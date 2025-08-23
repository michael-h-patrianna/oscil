/**
 * @file CorrelationMeterComponent.h
 * @brief Real-time correlation meter component for stereo imaging analysis
 *
 * This file defines the CorrelationMeterComponent class which provides real-time
 * visual feedback on stereo correlation and phase relationships between audio
 * channels. The component integrates with the SignalProcessor to display
 * correlation metrics with professional-grade accuracy and responsiveness.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../audio/ProcessingModes.h"
#include <memory>
#include <atomic>

// Forward declarations
namespace oscil::theme {
    class ThemeManager;
}

namespace oscil::ui::measurements {

/**
 * @class CorrelationMeterComponent
 * @brief Real-time stereo correlation meter with professional accuracy
 *
 * Provides visual feedback on stereo correlation coefficient and stereo width
 * measurements for professional audio analysis. The component displays:
 * - Correlation coefficient (-1.0 to +1.0)
 * - Stereo width measurement (0.0 to 2.0)
 * - Visual correlation meter with smooth updates
 * - Numerical value display with high precision
 *
 * Performance Characteristics:
 * - Update latency: ≤10ms requirement (typically <5ms achieved)
 * - Smooth value interpolation to reduce visual jitter
 * - Efficient rendering with minimal CPU overhead
 * - Thread-safe value updates from audio processing
 *
 * Features:
 * - Professional-grade correlation meter visualization
 * - Configurable smoothing/damping for stable display
 * - Theme integration for consistent visual styling
 * - Processing mode awareness (only shows relevant measurements)
 * - High-precision numerical readouts
 * - Optional peak hold and history display
 *
 * Design Notes:
 * The component uses atomic operations for thread-safe communication with
 * the audio processing thread. Values are smoothed using exponential moving
 * averages to provide stable visual feedback while maintaining responsiveness.
 *
 * Integration:
 * This component is designed to integrate with:
 * - SignalProcessor for correlation data
 * - ThemeManager for visual styling
 * - MeasurementOverlayComponent for layout
 * - Processing mode system for relevance filtering
 *
 * Example Usage:
 * @code
 * auto correlationMeter = std::make_unique<CorrelationMeterComponent>();
 * correlationMeter->setThemeManager(&themeManager);
 * correlationMeter->setUpdateRate(30.0f); // 30 Hz updates
 * correlationMeter->updateValues(metrics); // From SignalProcessor
 * @endcode
 *
 * @see audio::CorrelationMetrics
 * @see audio::SignalProcessor
 * @see MeasurementOverlayComponent
 */
class CorrelationMeterComponent : public juce::Component,
                                  public juce::Timer {
public:
    /**
     * @brief Configuration for correlation meter behavior
     */
    struct MeterConfig {
        float updateRateHz = 30.0f;        ///< Display update rate in Hz
        float smoothingFactor = 0.8f;      ///< Exponential smoothing (0.0 = no smoothing, 1.0 = maximum)
        bool showNumerical = true;         ///< Show numerical values alongside meter
        bool showStereoWidth = true;       ///< Show stereo width measurement
        bool enablePeakHold = false;       ///< Enable peak hold functionality
        float peakHoldTimeMs = 1000.0f;    ///< Peak hold time in milliseconds
    };

    /**
     * @brief Constructs correlation meter with default configuration
     *
     * Initializes the meter with professional-grade settings suitable for
     * mixing and mastering applications.
     *
     * @post Component is ready for display and value updates
     * @post Timer is configured for smooth visual updates
     * @post All measurements initialized to neutral values
     */
    CorrelationMeterComponent();

    /**
     * @brief Constructs correlation meter with custom configuration
     *
     * @param config Custom configuration for meter behavior
     */
    explicit CorrelationMeterComponent(const MeterConfig& config);

    /**
     * @brief Destructor
     */
    ~CorrelationMeterComponent() override;

    // === Configuration ===

    /**
     * @brief Sets the meter configuration
     *
     * Updates the meter behavior including update rate, smoothing, and
     * display options. Changes take effect immediately.
     *
     * @param config New configuration settings
     */
    void setConfig(const MeterConfig& config);

    /**
     * @brief Gets current meter configuration
     */
    const MeterConfig& getConfig() const { return meterConfig; }

    /**
     * @brief Sets the theme manager for visual styling
     *
     * @param manager Pointer to theme manager (nullptr for default styling)
     */
    void setThemeManager(oscil::theme::ThemeManager* manager);

    /**
     * @brief Sets which processing mode is currently active
     *
     * Used to determine which measurements are relevant and should be displayed.
     * For example, correlation is not meaningful in mono sum mode.
     *
     * @param mode Current signal processing mode
     */
    void setProcessingMode(audio::SignalProcessingMode mode);

    // === Value Updates ===

    /**
     * @brief Updates correlation measurements from audio processing
     *
     * Thread-safe method to update the correlation values displayed by the meter.
     * Values are smoothed internally to provide stable visual feedback.
     *
     * @param metrics Latest correlation metrics from SignalProcessor
     *
     * @note Thread-safe: Can be called from audio processing thread
     * @note Values are smoothed based on current smoothing configuration
     */
    void updateValues(const audio::CorrelationMetrics& metrics);

    /**
     * @brief Gets current displayed correlation value
     *
     * @return Current smoothed correlation coefficient (-1.0 to +1.0)
     */
    float getCurrentCorrelation() const { return displayCorrelation.load(); }

    /**
     * @brief Gets current displayed stereo width value
     *
     * @return Current smoothed stereo width (0.0 to 2.0)
     */
    float getCurrentStereoWidth() const { return displayStereoWidth.load(); }

    // === Component Overrides ===

    /**
     * @brief Renders the correlation meter display
     *
     * Draws the correlation meter with current values, including:
     * - Correlation meter bar with color coding
     * - Stereo width indicator
     * - Numerical value displays
     * - Peak hold indicators (if enabled)
     *
     * @param g JUCE Graphics context for drawing
     */
    void paint(juce::Graphics& g) override;

    /**
     * @brief Handles component resize events
     *
     * Recalculates layout for meter elements when component size changes.
     */
    void resized() override;

    // === Timer Callback ===

    /**
     * @brief Timer callback for smooth value interpolation
     *
     * Updates displayed values with smooth interpolation to reduce visual
     * jitter while maintaining responsiveness to audio changes.
     */
    void timerCallback() override;

    // === Utility ===

    /**
     * @brief Gets the preferred size for this component
     *
     * @return Recommended size for optimal meter display
     */
    juce::Rectangle<int> getPreferredBounds() const;

    /**
     * @brief Checks if meter is relevant for current processing mode
     *
     * @return true if current processing mode supports correlation measurement
     */
    bool isRelevantForCurrentMode() const;

private:
    // === Configuration ===
    MeterConfig meterConfig;
    oscil::theme::ThemeManager* themeManager = nullptr;
    audio::SignalProcessingMode currentMode = audio::SignalProcessingMode::FullStereo;

    // === Measurement Values ===
    // Raw values from audio processing (atomic for thread safety)
    std::atomic<float> rawCorrelation{0.0f};
    std::atomic<float> rawStereoWidth{0.0f};
    std::atomic<uint64_t> lastUpdateTimestamp{0};

    // Smoothed values for display
    std::atomic<float> displayCorrelation{0.0f};
    std::atomic<float> displayStereoWidth{0.0f};

    // Peak hold values
    float peakCorrelation = 0.0f;
    float peakStereoWidth = 0.0f;
    uint32_t peakHoldTimer = 0;

    // === Cached Layout ===
    struct MeterLayout {
        juce::Rectangle<float> correlationMeterBounds;
        juce::Rectangle<float> stereoWidthBounds;
        juce::Rectangle<float> correlationValueBounds;
        juce::Rectangle<float> stereoWidthValueBounds;
        juce::Rectangle<float> labelBounds;
        bool isValid = false;
    } cachedLayout;

    // === Rendering ===

    /**
     * @brief Updates cached layout when component size changes
     */
    void updateLayout();

    /**
     * @brief Renders the correlation meter bar
     *
     * @param g Graphics context
     * @param bounds Meter bounds
     * @param value Current correlation value (-1.0 to +1.0)
     */
    void drawCorrelationMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds, float value);

    /**
     * @brief Renders the stereo width indicator
     *
     * @param g Graphics context
     * @param bounds Indicator bounds
     * @param value Current stereo width (0.0 to 2.0)
     */
    void drawStereoWidthIndicator(juce::Graphics& g, const juce::Rectangle<float>& bounds, float value);

    /**
     * @brief Renders numerical value displays
     *
     * @param g Graphics context
     */
    void drawNumericalValues(juce::Graphics& g);

    /**
     * @brief Gets color for correlation value based on theme and magnitude
     *
     * @param correlation Correlation value (-1.0 to +1.0)
     * @return Color for rendering correlation
     */
    juce::Colour getCorrelationColor(float correlation) const;

    /**
     * @brief Gets color for stereo width value based on theme
     *
     * @param width Stereo width value (0.0 to 2.0)
     * @return Color for rendering width
     */
    juce::Colour getStereoWidthColor(float width) const;

    /**
     * @brief Smooths a value using exponential moving average
     *
     * @param currentValue Current displayed value
     * @param targetValue New target value
     * @param smoothingFactor Smoothing factor (0.0 = no smoothing, 1.0 = maximum)
     * @return Smoothed value
     */
    static float smoothValue(float currentValue, float targetValue, float smoothingFactor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationMeterComponent)
};

} // namespace oscil::ui::measurements
