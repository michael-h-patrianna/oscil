#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>
#include <array>

namespace oscil::theme {

/**
 * Defines a complete color theme for the Oscil oscilloscope plugin.
 * Contains all colors needed for UI and waveform rendering.
 */
struct ColorTheme {
    // Theme metadata
    juce::String name;
    juce::String description;
    int version{1};

    // Core UI colors
    juce::Colour background{0xff181818};        // Primary background
    juce::Colour surface{0xff2B2B2B};          // Component surfaces
    juce::Colour text{0xffFFFFFF};             // Primary text
    juce::Colour textSecondary{0xffB0B0B0};    // Secondary text
    juce::Colour accent{0xff00AAFF};           // Accent/highlight color
    juce::Colour border{0xff404040};           // Component borders
    juce::Colour grid{0xff404040};             // Grid lines

    // Waveform colors (8 distinct colors for multi-track visualization)
    static constexpr size_t MAX_WAVEFORM_COLORS = 8;
    std::array<juce::Colour, MAX_WAVEFORM_COLORS> waveformColors{{
        juce::Colour::fromFloatRGBA(0.25f, 0.85f, 0.9f, 1.0f),   // Cyan
        juce::Colour::fromFloatRGBA(0.9f, 0.6f, 0.3f, 1.0f),     // Orange
        juce::Colour::fromFloatRGBA(0.5f, 0.8f, 0.4f, 1.0f),     // Green
        juce::Colour::fromFloatRGBA(0.8f, 0.4f, 0.9f, 1.0f),     // Purple
        juce::Colour::fromFloatRGBA(0.9f, 0.9f, 0.2f, 1.0f),     // Yellow
        juce::Colour::fromFloatRGBA(0.9f, 0.3f, 0.3f, 1.0f),     // Red
        juce::Colour::fromFloatRGBA(0.3f, 0.6f, 0.9f, 1.0f),     // Blue
        juce::Colour::fromFloatRGBA(0.9f, 0.7f, 0.9f, 1.0f)      // Pink
    }};

    // Constructor
    ColorTheme() = default;

    /**
     * Creates a ColorTheme with specified name and description.
     */
    ColorTheme(const juce::String& themeName, const juce::String& themeDescription)
        : name(themeName), description(themeDescription) {}

    /**
     * Gets a waveform color by index (cycles through available colors).
     */
    juce::Colour getWaveformColor(int index) const {
        if (index < 0) return waveformColors[0];
        return waveformColors[static_cast<size_t>(index) % MAX_WAVEFORM_COLORS];
    }

    /**
     * Validates color contrast ratios for accessibility.
     * Returns true if theme meets WCAG 2.1 AA standards.
     */
    bool validateAccessibility() const;

    /**
     * Creates a dark professional theme.
     */
    static ColorTheme createDarkProfessional();

    /**
     * Creates a light modern theme.
     */
    static ColorTheme createLightModern();

    /**
     * Serializes theme to JSON for export/import.
     */
    juce::var toJson() const;

    /**
     * Creates theme from JSON data.
     */
    static ColorTheme fromJson(const juce::var& jsonData);

private:
    /**
     * Calculates contrast ratio between two colors.
     * Used for accessibility validation.
     */
    static double calculateContrastRatio(const juce::Colour& color1, const juce::Colour& color2);

    /**
     * Calculates relative luminance of a color.
     */
    static double getRelativeLuminance(const juce::Colour& color);
};

} // namespace oscil::theme
