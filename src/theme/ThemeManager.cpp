#include "ThemeManager.h"
#include <juce_core/juce_core.h>

namespace oscil::theme {

ThemeManager::ThemeManager() : currentTheme(nullptr), currentThemeId(ThemeId::DarkProfessional) {
    initializeBuiltInThemes();
    // Set default theme
    setCurrentTheme(ThemeId::DarkProfessional);
}

bool ThemeManager::setCurrentTheme(ThemeId themeId) {
    juce::String themeName = themeIdToString(themeId);
    return setCurrentTheme(themeName);
}

bool ThemeManager::setCurrentTheme(const juce::String& themeName) {
    auto it = themes.find(themeName);
    if (it == themes.end()) {
        return false;
    }

    currentTheme = it->second.get();
    currentThemeId = stringToThemeId(themeName);

    notifyThemeChanged();
    return true;
}

juce::StringArray ThemeManager::getAvailableThemeNames() const {
    juce::StringArray names;
    for (const auto& pair : themes) {
        names.add(pair.first);
    }
    return names;
}

bool ThemeManager::registerCustomTheme(const ColorTheme& theme) {
    if (theme.name.isEmpty()) {
        return false;
    }

    themes[theme.name] = std::make_unique<ColorTheme>(theme);
    return true;
}

const ColorTheme* ThemeManager::getTheme(const juce::String& themeName) const {
    auto it = themes.find(themeName);
    return it != themes.end() ? it->second.get() : nullptr;
}

juce::Colour ThemeManager::getWaveformColor(int trackIndex) const {
    if (currentTheme == nullptr) {
        // Fallback color if no theme is set
        return juce::Colour(0xff40D4F0);
    }
    return currentTheme->getWaveformColor(trackIndex);
}

bool ThemeManager::validateAllThemesAccessibility() const {
    for (const auto& pair : themes) {
        if (!pair.second->validateAccessibility()) {
            return false;
        }
    }
    return true;
}

juce::String ThemeManager::exportTheme(const juce::String& themeName) const {
    auto* theme = getTheme(themeName);
    if (theme == nullptr) {
        return {};
    }

    return juce::JSON::toString(theme->toJson());
}

bool ThemeManager::importTheme(const juce::String& jsonString, bool overwrite) {
    auto json = juce::JSON::parse(jsonString);
    if (!json.isObject()) {
        return false;
    }

    auto theme = ColorTheme::fromJson(json);
    if (theme.name.isEmpty()) {
        return false;
    }

    // Check if theme already exists
    if (!overwrite && themes.find(theme.name) != themes.end()) {
        return false;
    }

    return registerCustomTheme(theme);
}

void ThemeManager::addThemeChangeListener(ThemeChangeCallback callback) {
    juce::ScopedLock lock(listenerLock);
    themeChangeListeners.push_back(std::move(callback));
}

void ThemeManager::clearThemeChangeListeners() {
    juce::ScopedLock lock(listenerLock);
    themeChangeListeners.clear();
}

void ThemeManager::initializeBuiltInThemes() {
    // Register built-in themes
    auto darkTheme = ColorTheme::createDarkProfessional();
    auto lightTheme = ColorTheme::createLightModern();

    // Store the names before moving the objects
    auto darkName = darkTheme.name;
    auto lightName = lightTheme.name;

    themes[darkName] = std::make_unique<ColorTheme>(std::move(darkTheme));
    themes[lightName] = std::make_unique<ColorTheme>(std::move(lightTheme));
}

void ThemeManager::notifyThemeChanged() {
    if (currentTheme == nullptr) {
        return;
    }

    juce::ScopedLock lock(listenerLock);
    for (auto& callback : themeChangeListeners) {
        callback(*currentTheme);
    }
}

juce::String ThemeManager::themeIdToString(ThemeId id) {
    switch (id) {
        case ThemeId::DarkProfessional:
            return "Dark Professional";
        case ThemeId::LightModern:
            return "Light Modern";
        default:
            return "Dark Professional";
    }
}

ThemeManager::ThemeId ThemeManager::stringToThemeId(const juce::String& name) {
    if (name == "Light Modern") {
        return ThemeId::LightModern;
    }
    return ThemeId::DarkProfessional; // Default fallback
}

} // namespace oscil::theme
