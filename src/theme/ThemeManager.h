#pragma once

#include "ColorTheme.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <unordered_map>

namespace oscil::theme {

/**
 * Manages theme loading, caching, and application throughout the plugin.
 * Provides fast theme switching and color lookup for rendering performance.
 */
class ThemeManager {
public:
    /**
     * Theme identifiers for built-in themes
     */
    enum class ThemeId {
        DarkProfessional,
        DarkBlue,
        PureBlack,
        LightModern,
        LightWarm,
        ClassicGreen,
        ClassicAmber
    };

    /**
     * Constructor initializes with built-in themes
     */
    ThemeManager();

    /**
     * Destructor
     */
    ~ThemeManager() = default;

    // Prevent copying and moving (due to CriticalSection)
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;

    /**
     * Gets the currently active theme
     */
    const ColorTheme& getCurrentTheme() const {
        if (currentTheme == nullptr) {
            // Fallback to first available theme if current is null
            static const auto fallback = ColorTheme::createDarkProfessional();
            return fallback;
        }
        return *currentTheme;
    }

    /**
     * Sets the active theme by ID
     * @param themeId The built-in theme to activate
     * @return true if theme was successfully applied
     */
    bool setCurrentTheme(ThemeId themeId);

    /**
     * Sets the active theme by name
     * @param themeName The name of the theme to activate
     * @return true if theme was found and applied
     */
    bool setCurrentTheme(const juce::String& themeName);

    /**
     * Gets the current theme ID (if it's a built-in theme)
     */
    ThemeId getCurrentThemeId() const { return currentThemeId; }

    /**
     * Gets a list of all available theme names
     */
    juce::StringArray getAvailableThemeNames() const;

    /**
     * Registers a custom theme
     * @param theme The theme to register
     * @return true if theme was successfully registered
     */
    bool registerCustomTheme(const ColorTheme& theme);

    /**
     * Gets a theme by name (returns nullptr if not found)
     */
    const ColorTheme* getTheme(const juce::String& themeName) const;

    /**
     * Fast color lookup methods for performance-critical rendering
     */
    juce::Colour getWaveformColor(int trackIndex) const;

    /**
     * Extended waveform color method for multi-track support (up to 64 tracks)
     * Cycles through base 8 colors with variations for track counts > 8
     */
    juce::Colour getMultiTrackWaveformColor(int trackIndex) const;
    juce::Colour getBackgroundColor() const { return currentTheme->background; }
    juce::Colour getSurfaceColor() const { return currentTheme->surface; }
    juce::Colour getTextColor() const { return currentTheme->text; }
    juce::Colour getGridColor() const { return currentTheme->grid; }
    juce::Colour getBorderColor() const { return currentTheme->border; }
    juce::Colour getAccentColor() const { return currentTheme->accent; }

    /**
     * Validates that all themes meet accessibility requirements
     */
    bool validateAllThemesAccessibility() const;

    /**
     * Exports a theme to JSON string
     */
    juce::String exportTheme(const juce::String& themeName) const;

    /**
     * Imports a theme from JSON string
     * @param jsonString The JSON representation of the theme
     * @param overwrite Whether to overwrite existing theme with same name
     * @return true if import was successful
     */
    bool importTheme(const juce::String& jsonString, bool overwrite = false);

    /**
     * Theme change notification callback type
     */
    using ThemeChangeCallback = std::function<void(const ColorTheme&)>;

    /**
     * Registers a callback for theme changes
     */
    void addThemeChangeListener(ThemeChangeCallback callback);

    /**
     * Removes all theme change listeners
     */
    void clearThemeChangeListeners();

private:
    /**
     * Initializes built-in themes
     */
    void initializeBuiltInThemes();

    /**
     * Notifies all listeners of theme change
     */
    void notifyThemeChanged();

    /**
     * Converts ThemeId enum to string
     */
    static juce::String themeIdToString(ThemeId id);

    /**
     * Converts string to ThemeId enum
     */
    static ThemeId stringToThemeId(const juce::String& name);

    // Theme storage
    std::unordered_map<juce::String, std::unique_ptr<ColorTheme>> themes;

    // Current active theme
    const ColorTheme* currentTheme{nullptr};
    ThemeId currentThemeId{ThemeId::DarkProfessional};

    // Theme change listeners
    std::vector<ThemeChangeCallback> themeChangeListeners;

    // Thread safety for listeners
    juce::CriticalSection listenerLock;
};

} // namespace oscil::theme
