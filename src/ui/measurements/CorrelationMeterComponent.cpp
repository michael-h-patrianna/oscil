/**
 * @file CorrelationMeterComponent.cpp
 * @brief Implementation of real-time correlation meter component
 */

#include "CorrelationMeterComponent.h"
#include "../../theme/ThemeManager.h"
#include <algorithm>
#include <cmath>

namespace oscil::ui::measurements {

namespace {
    // Rendering constants
    constexpr int PREFERRED_WIDTH = 120;
    constexpr int PREFERRED_HEIGHT = 80;
    constexpr int METER_HEIGHT = 12;
    constexpr int VALUE_HEIGHT = 16;
    constexpr int SPACING = 4;
}

//==============================================================================
CorrelationMeterComponent::CorrelationMeterComponent()
    : CorrelationMeterComponent(MeterConfig{}) {
}

CorrelationMeterComponent::CorrelationMeterComponent(const MeterConfig& config)
    : meterConfig(config) {

    // Initialize display values to neutral
    displayCorrelation.store(0.0F);
    displayStereoWidth.store(1.0F); // Neutral stereo width

    // Start timer for smooth updates
    if (meterConfig.updateRateHz > 0.0F) {
        const int intervalMs = static_cast<int>(1000.0F / meterConfig.updateRateHz);
        startTimer(intervalMs);
    }
}

CorrelationMeterComponent::~CorrelationMeterComponent() {
    stopTimer();
}

//==============================================================================
void CorrelationMeterComponent::setConfig(const MeterConfig& config) {
    meterConfig = config;

    // Update timer rate
    stopTimer();
    if (meterConfig.updateRateHz > 0.0F) {
        const int intervalMs = static_cast<int>(1000.0F / meterConfig.updateRateHz);
        startTimer(intervalMs);
    }

    // Invalidate layout
    cachedLayout.isValid = false;
    repaint();
}

void CorrelationMeterComponent::setThemeManager(oscil::theme::ThemeManager* manager) {
    themeManager = manager;
    repaint();
}

void CorrelationMeterComponent::setProcessingMode(audio::SignalProcessingMode mode) {
    currentMode = mode;
    repaint();
}

//==============================================================================
void CorrelationMeterComponent::updateValues(const audio::CorrelationMetrics& metrics) {
    // Store raw values atomically
    rawCorrelation.store(metrics.correlation);
    rawStereoWidth.store(metrics.stereoWidth);

    // Update timestamp for freshness tracking
    lastUpdateTimestamp.store(juce::Time::getMillisecondCounter());
}

//==============================================================================
void CorrelationMeterComponent::paint(juce::Graphics& graphics) {
    // Early exit if not relevant for current mode
    if (!isRelevantForCurrentMode()) {
        graphics.fillAll(juce::Colours::transparentBlack);
        return;
    }

    // Update layout if needed
    if (!cachedLayout.isValid) {
        updateLayout();
    }

    // Get current theme colors
    juce::Colour backgroundColour = juce::Colours::darkgrey;
    juce::Colour textColour = juce::Colours::white;

    if (themeManager != nullptr) {
        // Use theme colors when available
        backgroundColour = themeManager->getCurrentTheme().background;
        textColour = themeManager->getCurrentTheme().text;
    }

    // Clear background
    graphics.fillAll(backgroundColour);

    // Get current display values
    const float correlation = displayCorrelation.load();
    const float stereoWidth = displayStereoWidth.load();

    // Draw correlation meter
    drawCorrelationMeter(graphics, cachedLayout.correlationMeterBounds, correlation);

    // Draw stereo width indicator if enabled
    if (meterConfig.showStereoWidth) {
        drawStereoWidthIndicator(graphics, cachedLayout.stereoWidthBounds, stereoWidth);
    }

    // Draw numerical values if enabled
    if (meterConfig.showNumerical) {
        drawNumericalValues(graphics);
    }
}

void CorrelationMeterComponent::resized() {
    cachedLayout.isValid = false;
}

//==============================================================================
void CorrelationMeterComponent::timerCallback() {
    // Smooth interpolation between current and target values
    const float currentCorr = displayCorrelation.load();
    const float currentWidth = displayStereoWidth.load();

    const float targetCorr = rawCorrelation.load();
    const float targetWidth = rawStereoWidth.load();

    // Apply smoothing
    const float newCorr = smoothValue(currentCorr, targetCorr, meterConfig.smoothingFactor);
    const float newWidth = smoothValue(currentWidth, targetWidth, meterConfig.smoothingFactor);

    // Update display values if they changed significantly
    constexpr float threshold = 0.001F;
    if (std::abs(newCorr - currentCorr) > threshold ||
        std::abs(newWidth - currentWidth) > threshold) {

        displayCorrelation.store(newCorr);
        displayStereoWidth.store(newWidth);

        // Update peak hold if enabled
        if (meterConfig.enablePeakHold) {
            if (std::abs(newCorr) > std::abs(peakCorrelation)) {
                peakCorrelation = newCorr;
                peakHoldTimer = static_cast<uint32_t>(meterConfig.peakHoldTimeMs);
            } else if (peakHoldTimer > 0) {
                const uint32_t timerInterval = static_cast<uint32_t>(1000.0F / meterConfig.updateRateHz);
                peakHoldTimer = (peakHoldTimer > timerInterval) ? peakHoldTimer - timerInterval : 0;
            }

            if (newWidth > peakStereoWidth) {
                peakStereoWidth = newWidth;
            }
        }

        repaint();
    }
}

//==============================================================================
juce::Rectangle<int> CorrelationMeterComponent::getPreferredBounds() const {
    return {0, 0, PREFERRED_WIDTH, PREFERRED_HEIGHT};
}

bool CorrelationMeterComponent::isRelevantForCurrentMode() const {
    // Correlation is only meaningful for stereo modes
    switch (currentMode) {
        case audio::SignalProcessingMode::FullStereo:
        case audio::SignalProcessingMode::MidSide:
        case audio::SignalProcessingMode::Difference:
            return true;
        case audio::SignalProcessingMode::MonoSum:
        case audio::SignalProcessingMode::LeftOnly:
        case audio::SignalProcessingMode::RightOnly:
            return false;
    }
    return false;
}

//==============================================================================
void CorrelationMeterComponent::updateLayout() {
    const auto bounds = getLocalBounds().toFloat();

    if (bounds.isEmpty()) {
        cachedLayout.isValid = false;
        return;
    }

    // Calculate layout based on enabled features
    float currentY = SPACING;
    const float width = bounds.getWidth() - (2 * SPACING);

    // Correlation meter
    cachedLayout.correlationMeterBounds = {
        static_cast<float>(SPACING), currentY, width, static_cast<float>(METER_HEIGHT)
    };
    currentY += METER_HEIGHT + SPACING;

    // Correlation value
    if (meterConfig.showNumerical) {
        cachedLayout.correlationValueBounds = {
            static_cast<float>(SPACING), currentY, width, static_cast<float>(VALUE_HEIGHT)
        };
        currentY += VALUE_HEIGHT + SPACING;
    }

    // Stereo width meter (if enabled)
    if (meterConfig.showStereoWidth) {
        cachedLayout.stereoWidthBounds = {
            static_cast<float>(SPACING), currentY, width, static_cast<float>(METER_HEIGHT)
        };
        currentY += METER_HEIGHT + SPACING;

        // Stereo width value
        if (meterConfig.showNumerical) {
            cachedLayout.stereoWidthValueBounds = {
                static_cast<float>(SPACING), currentY, width, static_cast<float>(VALUE_HEIGHT)
            };
        }
    }

    cachedLayout.isValid = true;
}

void CorrelationMeterComponent::drawCorrelationMeter(juce::Graphics& graphics,
                                                    const juce::Rectangle<float>& bounds,
                                                    float value) {
    // Draw meter background
    graphics.setColour(juce::Colours::black);
    graphics.fillRoundedRectangle(bounds, 2.0F);

    // Draw meter border
    graphics.setColour(juce::Colours::grey);
    graphics.drawRoundedRectangle(bounds, 2.0F, 1.0F);

    // Draw center line (zero correlation)
    const float centerX = bounds.getCentreX();
    graphics.setColour(juce::Colours::white.withAlpha(0.5F));
    graphics.drawVerticalLine(static_cast<int>(centerX), bounds.getY() + 1, bounds.getBottom() - 1);

    // Draw correlation bar
    const juce::Colour meterColor = getCorrelationColor(value);
    graphics.setColour(meterColor);

    // Calculate bar position and width
    const float meterWidth = bounds.getWidth() - 4; // Account for padding
    const float meterLeft = bounds.getX() + 2;
    const float meterCenter = meterLeft + (meterWidth * 0.5F);

    if (value >= 0.0F) {
        // Positive correlation: bar from center to right
        const float barWidth = (meterWidth * 0.5F) * value;
        const juce::Rectangle<float> barBounds{
            meterCenter, bounds.getY() + 2, barWidth, bounds.getHeight() - 4
        };
        graphics.fillRoundedRectangle(barBounds, 1.0F);
    } else {
        // Negative correlation: bar from center to left
        const float barWidth = (meterWidth * 0.5F) * (-value);
        const juce::Rectangle<float> barBounds{
            meterCenter - barWidth, bounds.getY() + 2, barWidth, bounds.getHeight() - 4
        };
        graphics.fillRoundedRectangle(barBounds, 1.0F);
    }

    // Draw peak hold indicator if enabled
    if (meterConfig.enablePeakHold && peakHoldTimer > 0) {
        graphics.setColour(meterColor.brighter());
        const float peakPos = meterCenter + (meterWidth * 0.5F * peakCorrelation);
        graphics.drawVerticalLine(static_cast<int>(peakPos), bounds.getY() + 1, bounds.getBottom() - 1);
    }
}

void CorrelationMeterComponent::drawStereoWidthIndicator(juce::Graphics& graphics,
                                                        const juce::Rectangle<float>& bounds,
                                                        float value) {
    // Draw meter background
    graphics.setColour(juce::Colours::black);
    graphics.fillRoundedRectangle(bounds, 2.0F);

    // Draw meter border
    graphics.setColour(juce::Colours::grey);
    graphics.drawRoundedRectangle(bounds, 2.0F, 1.0F);

    // Draw width bar (0.0 to 2.0 range)
    const juce::Colour widthColor = getStereoWidthColor(value);
    graphics.setColour(widthColor);

    const float normalizedValue = std::clamp(value / 2.0F, 0.0F, 1.0F);
    const float barWidth = (bounds.getWidth() - 4) * normalizedValue;

    const juce::Rectangle<float> barBounds{
        bounds.getX() + 2, bounds.getY() + 2, barWidth, bounds.getHeight() - 4
    };
    graphics.fillRoundedRectangle(barBounds, 1.0F);
}

void CorrelationMeterComponent::drawNumericalValues(juce::Graphics& graphics) {
    juce::Colour textColour = juce::Colours::white;
    if (themeManager != nullptr) {
        textColour = themeManager->getCurrentTheme().text;
    }

    graphics.setColour(textColour);
    graphics.setFont(12.0F);

    // Draw correlation value
    const float correlation = displayCorrelation.load();
    const juce::String corrText = juce::String(correlation, 3);
    graphics.drawText(corrText, cachedLayout.correlationValueBounds,
                     juce::Justification::centred, true);

    // Draw stereo width value if enabled
    if (meterConfig.showStereoWidth) {
        const float stereoWidth = displayStereoWidth.load();
        const juce::String widthText = juce::String(stereoWidth, 2);
        graphics.drawText(widthText, cachedLayout.stereoWidthValueBounds,
                         juce::Justification::centred, true);
    }
}

juce::Colour CorrelationMeterComponent::getCorrelationColor(float correlation) const {
    // Color coding based on correlation value
    const float absCorr = std::abs(correlation);

    if (absCorr < 0.3F) {
        // Low correlation: neutral/warning color
        return juce::Colours::yellow;
    } else if (absCorr < 0.7F) {
        // Medium correlation: good color
        return juce::Colours::green;
    } else {
        // High correlation: excellent color (or problematic if negative)
        return correlation > 0 ? juce::Colours::cyan : juce::Colours::red;
    }
}

juce::Colour CorrelationMeterComponent::getStereoWidthColor(float width) const {
    // Color coding based on stereo width
    if (width < 0.5F) {
        return juce::Colours::red;      // Too narrow
    } else if (width < 1.5F) {
        return juce::Colours::green;    // Good width
    } else {
        return juce::Colours::orange;   // Very wide
    }
}

float CorrelationMeterComponent::smoothValue(float currentValue, float targetValue, float smoothingFactor) {
    const float alpha = 1.0F - smoothingFactor;
    return (alpha * targetValue) + (smoothingFactor * currentValue);
}

} // namespace oscil::ui::measurements
