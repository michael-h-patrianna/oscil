#include "ColorTheme.h"
#include <cmath>

namespace oscil::theme {

bool ColorTheme::validateAccessibility() const {
    // Check text contrast ratios against WCAG 2.1 AA standards
    // Normal text: 4.5:1, large text: 3:1

    // Text on background
    if (calculateContrastRatio(text, background) < 4.5) return false;
    if (calculateContrastRatio(textSecondary, background) < 4.5) return false;

    // Text on surface
    if (calculateContrastRatio(text, surface) < 4.5) return false;
    if (calculateContrastRatio(textSecondary, surface) < 4.5) return false;

    // Waveform colors on background (3:1 ratio for graphical elements)
    for (const auto& color : waveformColors) {
        if (calculateContrastRatio(color, background) < 3.0) return false;
    }

    return true;
}

ColorTheme ColorTheme::createDarkProfessional() {
    ColorTheme theme("Dark Professional", "Professional dark theme optimized for studio environments");

    // Dark theme colors
    theme.background = juce::Colour(0xff181818);      // Dark charcoal
    theme.surface = juce::Colour(0xff2B2B2B);         // Lighter charcoal
    theme.text = juce::Colour(0xffFFFFFF);            // White text
    theme.textSecondary = juce::Colour(0xffB0B0B0);   // Light gray
    theme.accent = juce::Colour(0xff00AAFF);          // Bright blue
    theme.border = juce::Colour(0xff404040);          // Medium gray
    theme.grid = juce::Colour(0xff404040);            // Medium gray

    // High-contrast waveform colors for dark background
    theme.waveformColors = {{
        juce::Colour(0xff40D4F0),  // Bright cyan
        juce::Colour(0xffFF9641),  // Orange
        juce::Colour(0xff7FFF7F),  // Bright green
        juce::Colour(0xffDD7FFF),  // Bright purple
        juce::Colour(0xffFFFF40),  // Bright yellow
        juce::Colour(0xffFF6B6B),  // Bright red
        juce::Colour(0xff6BA6FF),  // Bright blue
        juce::Colour(0xffFFB3FF)   // Bright pink
    }};

    return theme;
}

ColorTheme ColorTheme::createLightModern() {
    ColorTheme theme("Light Modern", "Clean light theme for bright environments");

    // Light theme colors with softer, less bright background
    theme.background = juce::Colour(0xffF5F5F5);      // Soft light gray instead of pure white
    theme.surface = juce::Colour(0xffEEEEEE);         // Light gray
    theme.text = juce::Colour(0xff1A1A1A);            // Very dark gray text
    theme.textSecondary = juce::Colour(0xff4A4A4A);   // Dark gray (improved contrast)
    theme.accent = juce::Colour(0xff0066CC);          // Dark blue
    theme.border = juce::Colour(0xffC0C0C0);          // Light gray border
    theme.grid = juce::Colour(0xffD8D8D8);            // Slightly darker grid for softer background

    // Pastel, colorful waveform colors for light background
    theme.waveformColors = {{
        juce::Colour(0xff4A90E2),  // Soft blue
        juce::Colour(0xffF5A623),  // Warm orange
        juce::Colour(0xff7ED321),  // Fresh green
        juce::Colour(0xffBD10E0),  // Vibrant purple
        juce::Colour(0xffE94B3C),  // Coral red
        juce::Colour(0xff50E3C2),  // Mint green
        juce::Colour(0xffB8E986),  // Light lime
        juce::Colour(0xffF8E71C)   // Bright yellow
    }};

    return theme;
}

juce::var ColorTheme::toJson() const {
    auto json = juce::DynamicObject::Ptr(new juce::DynamicObject());

    json->setProperty("name", name);
    json->setProperty("description", description);
    json->setProperty("version", version);

    // Core colors
    json->setProperty("background", background.toString());
    json->setProperty("surface", surface.toString());
    json->setProperty("text", text.toString());
    json->setProperty("textSecondary", textSecondary.toString());
    json->setProperty("accent", accent.toString());
    json->setProperty("border", border.toString());
    json->setProperty("grid", grid.toString());

    // Waveform colors array
    auto waveformArray = juce::Array<juce::var>();
    for (const auto& color : waveformColors) {
        waveformArray.add(color.toString());
    }
    json->setProperty("waveformColors", waveformArray);

    return juce::var(json.get());
}

ColorTheme ColorTheme::fromJson(const juce::var& jsonData) {
    ColorTheme theme;

    if (auto* obj = jsonData.getDynamicObject()) {
        theme.name = obj->getProperty("name").toString();
        theme.description = obj->getProperty("description").toString();
        theme.version = obj->getProperty("version");

        // Core colors with fallbacks
        theme.background = juce::Colour::fromString(obj->getProperty("background").toString());
        theme.surface = juce::Colour::fromString(obj->getProperty("surface").toString());
        theme.text = juce::Colour::fromString(obj->getProperty("text").toString());
        theme.textSecondary = juce::Colour::fromString(obj->getProperty("textSecondary").toString());
        theme.accent = juce::Colour::fromString(obj->getProperty("accent").toString());
        theme.border = juce::Colour::fromString(obj->getProperty("border").toString());
        theme.grid = juce::Colour::fromString(obj->getProperty("grid").toString());

        // Waveform colors
        if (auto* waveformArray = obj->getProperty("waveformColors").getArray()) {
            for (int i = 0; i < juce::jmin(static_cast<int>(MAX_WAVEFORM_COLORS), waveformArray->size()); ++i) {
                theme.waveformColors[static_cast<size_t>(i)] = juce::Colour::fromString(waveformArray->getReference(i).toString());
            }
        }
    }

    return theme;
}

double ColorTheme::calculateContrastRatio(const juce::Colour& color1, const juce::Colour& color2) {
    auto l1 = getRelativeLuminance(color1);
    auto l2 = getRelativeLuminance(color2);

    // Ensure l1 is the lighter color
    if (l1 < l2) std::swap(l1, l2);

    return (l1 + 0.05) / (l2 + 0.05);
}

double ColorTheme::getRelativeLuminance(const juce::Colour& color) {
    auto r = color.getFloatRed();
    auto g = color.getFloatGreen();
    auto b = color.getFloatBlue();

    // Convert to linear RGB
    auto toLinear = [](float c) {
        return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };

    r = toLinear(r);
    g = toLinear(g);
    b = toLinear(b);

    // Calculate relative luminance using ITU-R BT.709 coefficients
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

} // namespace oscil::theme
