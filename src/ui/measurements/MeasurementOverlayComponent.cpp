/**
 * @file MeasurementOverlayComponent.cpp
 * @brief Implementation of measurement overlay component
 */

#include "MeasurementOverlayComponent.h"
#include "../../theme/ThemeManager.h"

namespace oscil::ui::measurements {

namespace {
    // Layout constants
    constexpr int COMPONENT_SPACING = 4;
    constexpr int MIN_OVERLAY_WIDTH = 140;
    constexpr int MIN_OVERLAY_HEIGHT = 60;
} // namespace

//==============================================================================
MeasurementOverlayComponent::MeasurementOverlayComponent()
    : MeasurementOverlayComponent(OverlayConfig{}) {
}

MeasurementOverlayComponent::MeasurementOverlayComponent(const OverlayConfig& config)
    : overlayConfig(config) {

    initializeMeasurementComponents();
    setOpaque(false); // Overlay should be transparent
}

//==============================================================================
void MeasurementOverlayComponent::setConfig(const OverlayConfig& config) {
    overlayConfig = config;

    // Update child component configurations
    if (correlationMeter != nullptr) {
        CorrelationMeterComponent::MeterConfig meterConfig;
        meterConfig.showStereoWidth = overlayConfig.showStereoWidthMeter;
        correlationMeter->setConfig(meterConfig);
    }

    // Invalidate layout and repaint
    cachedLayout.isValid = false;
    updateChildVisibility();
    repaint();
}

void MeasurementOverlayComponent::setThemeManager(oscil::theme::ThemeManager* manager) {
    themeManager = manager;

    // Propagate to child components
    if (correlationMeter != nullptr) {
        correlationMeter->setThemeManager(manager);
    }

    repaint();
}

void MeasurementOverlayComponent::setLayoutManager(oscil::ui::layout::LayoutManager* manager) {
    layoutManager = manager;

    // Update position if using adaptive mode
    if (positionMode == PositionMode::Adaptive) {
        cachedLayout.isValid = false;
        resized();
    }
}

void MeasurementOverlayComponent::setProcessingMode(audio::SignalProcessingMode mode) {
    currentMode = mode;

    // Propagate to child components
    if (correlationMeter != nullptr) {
        correlationMeter->setProcessingMode(mode);
    }

    // Update visibility based on relevance
    updateChildVisibility();
}

void MeasurementOverlayComponent::setPositionMode(PositionMode mode) {
    if (positionMode != mode) {
        positionMode = mode;
        cachedLayout.isValid = false;
        resized();
    }
}

//==============================================================================
void MeasurementOverlayComponent::updateCorrelationMetrics(const audio::CorrelationMetrics& metrics) {
    if (correlationMeter != nullptr && correlationMeterVisible) {
        correlationMeter->updateValues(metrics);
    }
}

void MeasurementOverlayComponent::updateStereoWidth(float width) {
    // For now, stereo width is part of correlation metrics
    // In future, this could be separate component
    if (correlationMeter != nullptr && stereoWidthMeterVisible) {
        audio::CorrelationMetrics metrics;
        metrics.stereoWidth = width;
        correlationMeter->updateValues(metrics);
    }
}

//==============================================================================
void MeasurementOverlayComponent::setCorrelationMeterVisible(bool visible) {
    correlationMeterVisible = visible && overlayConfig.showCorrelationMeter;
    updateChildVisibility();
}

void MeasurementOverlayComponent::setStereoWidthMeterVisible(bool visible) {
    stereoWidthMeterVisible = visible && overlayConfig.showStereoWidthMeter;
    updateChildVisibility();
}

void MeasurementOverlayComponent::setAllMeasurementsVisible(bool visible) {
    setCorrelationMeterVisible(visible);
    setStereoWidthMeterVisible(visible);
}

bool MeasurementOverlayComponent::hasMeasurementsVisible() const {
    return (correlationMeterVisible && correlationMeter != nullptr && correlationMeter->isVisible()) ||
           (stereoWidthMeterVisible && correlationMeter != nullptr && correlationMeter->isVisible());
}

//==============================================================================
void MeasurementOverlayComponent::paint(juce::Graphics& graphics) {
    // Early exit if no measurements are visible
    if (!hasMeasurementsVisible() || !isRelevantForCurrentMode()) {
        return;
    }

    // Update layout if needed
    if (!cachedLayout.isValid) {
        updateLayout();
    }

    // Draw overlay background if configured
    if (overlayConfig.overlayOpacity > 0.0F) {
        juce::Colour backgroundColour = juce::Colours::black.withAlpha(overlayConfig.overlayOpacity);

        if (themeManager != nullptr) {
            backgroundColour = themeManager->getCurrentTheme().background.withAlpha(overlayConfig.overlayOpacity);
        }

        graphics.setColour(backgroundColour);
        graphics.fillRoundedRectangle(cachedLayout.overlayBounds.toFloat(), 4.0F);

        // Draw subtle border
        graphics.setColour(backgroundColour.brighter(0.2F));
        graphics.drawRoundedRectangle(cachedLayout.overlayBounds.toFloat(), 4.0F, 1.0F);
    }
}

void MeasurementOverlayComponent::resized() {
    cachedLayout.isValid = false;
    updateLayout();
}

//==============================================================================
juce::Rectangle<int> MeasurementOverlayComponent::getPreferredBounds() const {
    int width = MIN_OVERLAY_WIDTH;
    int height = MIN_OVERLAY_HEIGHT;

    // Calculate based on enabled measurements
    if (correlationMeter != nullptr && correlationMeterVisible) {
        const auto meterBounds = correlationMeter->getPreferredBounds();
        width = std::max(width, meterBounds.getWidth());
        height += meterBounds.getHeight() + COMPONENT_SPACING;
    }

    // Add padding
    width += 2 * overlayConfig.overlayPadding;
    height += 2 * overlayConfig.overlayPadding;

    return {0, 0, width, height};
}

bool MeasurementOverlayComponent::isRelevantForCurrentMode() const {
    if (!overlayConfig.hideWhenNotRelevant) {
        return true;
    }

    // Check if any enabled measurements are relevant for current mode
    if (overlayConfig.showCorrelationMeter && correlationMeterVisible) {
        if (correlationMeter != nullptr && correlationMeter->isRelevantForCurrentMode()) {
            return true;
        }
    }

    return false;
}

//==============================================================================
void MeasurementOverlayComponent::initializeMeasurementComponents() {
    // Create correlation meter
    correlationMeter = std::make_unique<CorrelationMeterComponent>();
    correlationMeter->setThemeManager(themeManager);
    correlationMeter->setProcessingMode(currentMode);
    addAndMakeVisible(*correlationMeter);
}

void MeasurementOverlayComponent::updateLayout() {
    const auto bounds = getLocalBounds();

    if (bounds.isEmpty()) {
        cachedLayout.isValid = false;
        return;
    }

    // Calculate optimal position
    const auto overlayBounds = calculateOptimalPosition();
    cachedLayout.overlayBounds = overlayBounds;

    // Layout child components within overlay bounds
    auto contentBounds = overlayBounds.reduced(overlayConfig.overlayPadding);
    int currentY = contentBounds.getY();

    // Position correlation meter
    if (correlationMeter != nullptr && correlationMeterVisible) {
        const auto meterPreferredBounds = correlationMeter->getPreferredBounds();
        const auto meterBounds = juce::Rectangle<int>{
            contentBounds.getX(),
            currentY,
            contentBounds.getWidth(),
            meterPreferredBounds.getHeight()
        };

        correlationMeter->setBounds(meterBounds);
        cachedLayout.correlationMeterBounds = meterBounds;

        currentY += meterBounds.getHeight() + COMPONENT_SPACING;
    }

    cachedLayout.isValid = true;
}

juce::Rectangle<int> MeasurementOverlayComponent::calculateOptimalPosition() {
    const auto parentBounds = getLocalBounds();
    const auto preferredBounds = getPreferredBounds();

    PositionMode actualMode = positionMode;
    if (actualMode == PositionMode::Adaptive) {
        actualMode = getAdaptivePositionMode();
    }

    // Calculate position based on mode
    juce::Rectangle<int> position;

    switch (actualMode) {
        case PositionMode::TopLeft:
            position = {
                overlayConfig.overlayPadding,
                overlayConfig.overlayPadding,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;

        case PositionMode::TopRight:
            position = {
                parentBounds.getWidth() - preferredBounds.getWidth() - overlayConfig.overlayPadding,
                overlayConfig.overlayPadding,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;

        case PositionMode::BottomLeft:
            position = {
                overlayConfig.overlayPadding,
                parentBounds.getHeight() - preferredBounds.getHeight() - overlayConfig.overlayPadding,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;

        case PositionMode::BottomRight:
            position = {
                parentBounds.getWidth() - preferredBounds.getWidth() - overlayConfig.overlayPadding,
                parentBounds.getHeight() - preferredBounds.getHeight() - overlayConfig.overlayPadding,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;

        case PositionMode::Center:
            position = {
                (parentBounds.getWidth() - preferredBounds.getWidth()) / 2,
                (parentBounds.getHeight() - preferredBounds.getHeight()) / 2,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;

        case PositionMode::Adaptive:
            // Use adaptive position based on layout context
            position = {
                parentBounds.getWidth() - preferredBounds.getWidth() - overlayConfig.overlayPadding,
                overlayConfig.overlayPadding,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;

        default:
            // Fallback to top-right
            position = {
                parentBounds.getWidth() - preferredBounds.getWidth() - overlayConfig.overlayPadding,
                overlayConfig.overlayPadding,
                preferredBounds.getWidth(),
                preferredBounds.getHeight()
            };
            break;
    }

    // Ensure position is within parent bounds
    position = position.constrainedWithin(parentBounds);

    return position;
}

MeasurementOverlayComponent::PositionMode MeasurementOverlayComponent::getAdaptivePositionMode() const {
    // Use layout manager information if available
    if (layoutManager != nullptr) {
        const auto& currentLayout = layoutManager->getCurrentLayout();

        switch (currentLayout.mode) {
            case oscil::ui::layout::LayoutMode::Overlay:
                return PositionMode::TopRight; // Don't interfere with single waveform

            case oscil::ui::layout::LayoutMode::Split2H:
            case oscil::ui::layout::LayoutMode::Split2V:
                return PositionMode::BottomRight; // Corner position for split layouts

            case oscil::ui::layout::LayoutMode::Split4:
            case oscil::ui::layout::LayoutMode::Grid2x2:
            case oscil::ui::layout::LayoutMode::Grid3x3:
            case oscil::ui::layout::LayoutMode::Grid4x4:
            case oscil::ui::layout::LayoutMode::Grid6x6:
            case oscil::ui::layout::LayoutMode::Grid8x8:
                return PositionMode::Center; // Center for grid layouts
        }
    }

    // Default fallback
    return PositionMode::TopRight;
}

void MeasurementOverlayComponent::updateChildVisibility() {
    const bool shouldBeVisible = isRelevantForCurrentMode() && hasMeasurementsVisible();

    if (correlationMeter != nullptr) {
        const bool meterShouldBeVisible = correlationMeterVisible &&
                                         overlayConfig.showCorrelationMeter &&
                                         correlationMeter->isRelevantForCurrentMode();
        correlationMeter->setVisible(meterShouldBeVisible);
    }

    // Update overall component visibility
    setVisible(shouldBeVisible);

    // Trigger layout update if visibility changed
    if (isVisible() != shouldBeVisible) {
        cachedLayout.isValid = false;
        repaint();
    }
}

void MeasurementOverlayComponent::updateAnimations() {
    // Animation implementation would go here for smooth transitions
    // This is a placeholder for future animation features
}

void MeasurementOverlayComponent::startAnimation(const juce::Rectangle<int>& targetBounds) {
    if (!overlayConfig.enableAnimations) {
        setBounds(targetBounds);
        return;
    }

    // Store animation state
    animationState.isAnimating = true;
    animationState.animationProgress = 0.0F;
    animationState.startBounds = getBounds();
    animationState.targetBounds = targetBounds;
    animationState.animationStartTime = juce::Time::getMillisecondCounter();

    // Start animation timer (would be implemented with actual animation system)
    // For now, just set target bounds immediately
    setBounds(targetBounds);
    animationState.isAnimating = false;
}

} // namespace oscil::ui::measurements
