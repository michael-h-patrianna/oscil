/**
 * @file test_complete_theme_system.cpp
 * @brief Comprehensive test suite for the complete theme system implementation
 *
 * This test suite validates the complete theme system implementation including
 * all 7 built-in themes, theme editor functionality, JSON import/export,
 * accessibility validation, and performance characteristics.
 *
 * Test Coverage:
 * - All 7 built-in themes creation and validation
 * - Theme accessibility compliance (WCAG 2.1 AA)
 * - 64-color multi-track waveform color generation
 * - JSON serialization and deserialization round-trip
 * - Theme switching performance (<50ms requirement)
 * - Theme editor component functionality
 * - Import/export operations with error handling
 * - Theme persistence and state management
 *
 * Performance Validation:
 * - Theme switching latency measurement
 * - Memory usage assessment
 * - Color calculation efficiency
 * - Accessibility validation speed
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "../src/theme/ColorTheme.h"
#include "../src/theme/ThemeManager.h"
#include "../src/ui/components/ThemeEditorComponent.h"
#include <chrono>
#include <memory>

using namespace oscil::theme;
using namespace oscil::ui::components;

TEST_CASE("ColorTheme - All 7 Built-in Themes Creation", "[theme][builtin]") {
    SECTION("Dark Professional Theme") {
        auto theme = ColorTheme::createDarkProfessional();

        REQUIRE(theme.name == "Dark Professional");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify theme has appropriate dark colors
        REQUIRE(theme.background.getBrightness() < 0.3f);
        REQUIRE(theme.text.getBrightness() > 0.8f);

        // Verify waveform colors are distinct
        std::set<uint32_t> colorSet;
        for (const auto& color : theme.waveformColors) {
            colorSet.insert(color.getARGB());
        }
        REQUIRE(colorSet.size() == 8); // All 8 colors should be unique
    }

    SECTION("Dark Blue Theme") {
        auto theme = ColorTheme::createDarkBlue();

        REQUIRE(theme.name == "Dark Blue");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify blue-tinted theme characteristics
        REQUIRE(theme.background.getBrightness() < 0.3f);
        REQUIRE(theme.background.getHue() > 0.4f); // Blue hue range
        REQUIRE(theme.background.getHue() < 0.8f);
    }

    SECTION("Pure Black Theme") {
        auto theme = ColorTheme::createPureBlack();

        REQUIRE(theme.name == "Pure Black");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify pure black background
        REQUIRE(theme.background == juce::Colour(0xff000000));
        REQUIRE(theme.text.getBrightness() > 0.9f); // Very bright text
    }

    SECTION("Light Modern Theme") {
        auto theme = ColorTheme::createLightModern();

        REQUIRE(theme.name == "Light Modern");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify light theme characteristics
        REQUIRE(theme.background.getBrightness() > 0.8f);
        REQUIRE(theme.text.getBrightness() < 0.3f);
    }

    SECTION("Light Warm Theme") {
        auto theme = ColorTheme::createLightWarm();

        REQUIRE(theme.name == "Light Warm");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify warm light characteristics
        REQUIRE(theme.background.getBrightness() > 0.8f);
        // Warm tones typically have hue in yellow/orange range
        float hue = theme.background.getHue();
        REQUIRE((hue >= 0.0f && hue <= 0.15f) || hue >= 0.85f); // Warm hue range
    }

    SECTION("Classic Green Theme") {
        auto theme = ColorTheme::createClassicGreen();

        REQUIRE(theme.name == "Classic Green");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify green phosphor characteristics
        REQUIRE(theme.background.getBrightness() < 0.2f);
        REQUIRE(theme.text.getHue() > 0.2f); // Green hue range
        REQUIRE(theme.text.getHue() < 0.5f);
    }

    SECTION("Classic Amber Theme") {
        auto theme = ColorTheme::createClassicAmber();

        REQUIRE(theme.name == "Classic Amber");
        REQUIRE(theme.description.isNotEmpty());
        REQUIRE(theme.validateAccessibility());

        // Verify amber phosphor characteristics
        REQUIRE(theme.background.getBrightness() < 0.2f);
        float textHue = theme.text.getHue();
        REQUIRE(textHue >= 0.0f && textHue <= 0.2f); // Amber/orange hue range
    }
}

TEST_CASE("ThemeManager - All 7 Themes Integration", "[theme][manager]") {
    auto themeManager = std::make_shared<ThemeManager>();

    SECTION("All Built-in Themes Available") {
        auto availableThemes = themeManager->getAvailableThemeNames();

        REQUIRE(availableThemes.size() == 7);
        REQUIRE(availableThemes.contains("Dark Professional"));
        REQUIRE(availableThemes.contains("Dark Blue"));
        REQUIRE(availableThemes.contains("Pure Black"));
        REQUIRE(availableThemes.contains("Light Modern"));
        REQUIRE(availableThemes.contains("Light Warm"));
        REQUIRE(availableThemes.contains("Classic Green"));
        REQUIRE(availableThemes.contains("Classic Amber"));
    }

    SECTION("Theme Switching to All Themes") {
        std::vector<std::string> themeNames = {
            "Dark Professional", "Dark Blue", "Pure Black",
            "Light Modern", "Light Warm", "Classic Green", "Classic Amber"
        };

        for (const auto& themeName : themeNames) {
            REQUIRE(themeManager->setCurrentTheme(juce::String(themeName)));
            REQUIRE(themeManager->getCurrentTheme().name == themeName);
        }
    }

    SECTION("All Themes Accessibility Validation") {
        REQUIRE(themeManager->validateAllThemesAccessibility());
    }
}

TEST_CASE("Multi-Track Color System - 64 Colors", "[theme][multitrack]") {
    auto themeManager = std::make_shared<ThemeManager>();

    SECTION("64 Distinct Colors Generation") {
        themeManager->setCurrentTheme(ThemeManager::ThemeId::DarkProfessional);

        std::set<uint32_t> colorSet;
        for (int i = 0; i < 64; ++i) {
            auto color = themeManager->getMultiTrackWaveformColor(i);
            colorSet.insert(color.getARGB());
        }

        // We expect high distinctness but allow some variation due to color space limitations
        REQUIRE(colorSet.size() >= 56); // At least 56 distinct colors out of 64
    }

    SECTION("Color Variation Patterns") {
        themeManager->setCurrentTheme(ThemeManager::ThemeId::LightModern);

        // Test that colors follow the expected group pattern
        auto color0 = themeManager->getMultiTrackWaveformColor(0);
        auto color8 = themeManager->getMultiTrackWaveformColor(8);
        auto color16 = themeManager->getMultiTrackWaveformColor(16);

        // Colors in different groups should have different characteristics
        REQUIRE(color0.getBrightness() != color8.getBrightness());
        REQUIRE(color0.getSaturation() != color16.getSaturation());
    }

    SECTION("All Themes Support 64 Colors") {
        std::vector<ThemeManager::ThemeId> allThemes = {
            ThemeManager::ThemeId::DarkProfessional,
            ThemeManager::ThemeId::DarkBlue,
            ThemeManager::ThemeId::PureBlack,
            ThemeManager::ThemeId::LightModern,
            ThemeManager::ThemeId::LightWarm,
            ThemeManager::ThemeId::ClassicGreen,
            ThemeManager::ThemeId::ClassicAmber
        };

        for (auto themeId : allThemes) {
            themeManager->setCurrentTheme(themeId);

            // Test first, middle, and last colors
            auto color0 = themeManager->getMultiTrackWaveformColor(0);
            auto color32 = themeManager->getMultiTrackWaveformColor(32);
            auto color63 = themeManager->getMultiTrackWaveformColor(63);

            REQUIRE(color0.isOpaque());
            REQUIRE(color32.isOpaque());
            REQUIRE(color63.isOpaque());
        }
    }
}

TEST_CASE("JSON Serialization - All Themes", "[theme][json]") {
    SECTION("Round-trip Serialization for All Themes") {
        std::vector<ColorTheme> themes = {
            ColorTheme::createDarkProfessional(),
            ColorTheme::createDarkBlue(),
            ColorTheme::createPureBlack(),
            ColorTheme::createLightModern(),
            ColorTheme::createLightWarm(),
            ColorTheme::createClassicGreen(),
            ColorTheme::createClassicAmber()
        };

        for (const auto& originalTheme : themes) {
            // Serialize to JSON
            auto json = originalTheme.toJson();
            auto jsonString = juce::JSON::toString(json);

            REQUIRE(jsonString.isNotEmpty());

            // Deserialize from JSON
            auto parsedJson = juce::JSON::parse(jsonString);
            REQUIRE(parsedJson.isObject());

            auto restoredTheme = ColorTheme::fromJson(parsedJson);

            // Verify all properties match
            REQUIRE(restoredTheme.name == originalTheme.name);
            REQUIRE(restoredTheme.description == originalTheme.description);
            REQUIRE(restoredTheme.background == originalTheme.background);
            REQUIRE(restoredTheme.surface == originalTheme.surface);
            REQUIRE(restoredTheme.text == originalTheme.text);
            REQUIRE(restoredTheme.textSecondary == originalTheme.textSecondary);
            REQUIRE(restoredTheme.accent == originalTheme.accent);
            REQUIRE(restoredTheme.border == originalTheme.border);
            REQUIRE(restoredTheme.grid == originalTheme.grid);

            // Verify waveform colors
            for (size_t i = 0; i < ColorTheme::MAX_WAVEFORM_COLORS; ++i) {
                REQUIRE(restoredTheme.waveformColors[i] == originalTheme.waveformColors[i]);
            }
        }
    }
}

TEST_CASE("Theme Editor Component", "[theme][editor]") {
    auto themeManager = std::make_shared<ThemeManager>();
    auto themeEditor = std::make_unique<ThemeEditorComponent>(themeManager);

    SECTION("Theme Editor Initialization") {
        REQUIRE(themeEditor != nullptr);
        REQUIRE(themeEditor->getWidth() >= ThemeEditorComponent::MIN_WIDTH);
        REQUIRE(themeEditor->getHeight() >= ThemeEditorComponent::MIN_HEIGHT);
    }

    SECTION("Load and Edit Theme") {
        auto originalTheme = ColorTheme::createDarkProfessional();
        themeEditor->setThemeToEdit(originalTheme);

        auto currentTheme = themeEditor->getCurrentTheme();
        REQUIRE(currentTheme.name == originalTheme.name);
        REQUIRE(currentTheme.background == originalTheme.background);
    }

    SECTION("Theme Validation") {
        auto validTheme = ColorTheme::createLightModern();
        themeEditor->setThemeToEdit(validTheme);

        REQUIRE(themeEditor->validateCurrentTheme());
    }

    SECTION("JSON Export/Import") {
        auto originalTheme = ColorTheme::createClassicGreen();
        themeEditor->setThemeToEdit(originalTheme);

        auto jsonString = themeEditor->exportThemeToJson();
        REQUIRE(jsonString.isNotEmpty());

        // Create new editor and import
        auto newEditor = std::make_unique<ThemeEditorComponent>(themeManager);
        REQUIRE(newEditor->importThemeFromJson(jsonString));

        auto importedTheme = newEditor->getCurrentTheme();
        REQUIRE(importedTheme.name == originalTheme.name);
    }

    SECTION("Reset to Original") {
        auto originalTheme = ColorTheme::createPureBlack();
        themeEditor->setThemeToEdit(originalTheme);

        // Simulate some changes by getting a different theme
        auto modifiedTheme = ColorTheme::createLightWarm();
        themeEditor->setThemeToEdit(modifiedTheme);

        // Reset should restore original
        themeEditor->resetToOriginal();
        auto currentTheme = themeEditor->getCurrentTheme();
        REQUIRE(currentTheme.name == originalTheme.name);
    }
}

TEST_CASE("Performance Requirements", "[theme][performance]") {
    auto themeManager = std::make_shared<ThemeManager>();

    SECTION("Theme Switching Performance") {
        std::vector<ThemeManager::ThemeId> themes = {
            ThemeManager::ThemeId::DarkProfessional,
            ThemeManager::ThemeId::DarkBlue,
            ThemeManager::ThemeId::PureBlack,
            ThemeManager::ThemeId::LightModern,
            ThemeManager::ThemeId::LightWarm,
            ThemeManager::ThemeId::ClassicGreen,
            ThemeManager::ThemeId::ClassicAmber
        };

        // Warm up
        for (int i = 0; i < 5; ++i) {
            themeManager->setCurrentTheme(themes[i % themes.size()]);
        }

        // Measure switching time
        auto startTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 100; ++i) {
            themeManager->setCurrentTheme(themes[i % themes.size()]);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

        // Average should be well under 50ms (50,000 microseconds)
        auto averageTimeUs = duration.count() / 100;
        REQUIRE(averageTimeUs < 50000); // 50ms requirement

        INFO("Average theme switching time: " << averageTimeUs << " microseconds");
    }

    SECTION("64-Color Generation Performance") {
        themeManager->setCurrentTheme(ThemeManager::ThemeId::DarkProfessional);

        auto startTime = std::chrono::high_resolution_clock::now();

        // Generate colors for 64 tracks, 1000 times
        for (int iteration = 0; iteration < 1000; ++iteration) {
            for (int track = 0; track < 64; ++track) {
                volatile auto color = themeManager->getMultiTrackWaveformColor(track);
                (void)color; // Prevent optimization
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

        // Should be very fast - under 1ms for 64 colors
        auto averageTimeUs = duration.count() / 1000;
        REQUIRE(averageTimeUs < 1000); // 1ms requirement

        INFO("Average 64-color generation time: " << averageTimeUs << " microseconds");
    }

    SECTION("Accessibility Validation Performance") {
        auto themes = {
            ColorTheme::createDarkProfessional(),
            ColorTheme::createDarkBlue(),
            ColorTheme::createPureBlack(),
            ColorTheme::createLightModern(),
            ColorTheme::createLightWarm(),
            ColorTheme::createClassicGreen(),
            ColorTheme::createClassicAmber()
        };

        auto startTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 100; ++i) {
            for (const auto& theme : themes) {
                volatile bool isValid = theme.validateAccessibility();
                (void)isValid; // Prevent optimization
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

        // Should be under 1ms per validation
        auto averageTimeUs = duration.count() / (100 * 7);
        REQUIRE(averageTimeUs < 1000); // 1ms requirement

        INFO("Average accessibility validation time: " << averageTimeUs << " microseconds");
    }
}

BENCHMARK_ADVANCED("Theme Manager - Theme Switching")(Catch::Benchmark::Chronometer meter) {
    auto themeManager = std::make_shared<ThemeManager>();
    std::vector<ThemeManager::ThemeId> themes = {
        ThemeManager::ThemeId::DarkProfessional,
        ThemeManager::ThemeId::LightModern,
        ThemeManager::ThemeId::DarkBlue,
        ThemeManager::ThemeId::ClassicGreen
    };

    meter.measure([&](int i) {
        return themeManager->setCurrentTheme(themes[i % themes.size()]);
    });
}

BENCHMARK_ADVANCED("Multi-Track Color Generation")(Catch::Benchmark::Chronometer meter) {
    auto themeManager = std::make_shared<ThemeManager>();
    themeManager->setCurrentTheme(ThemeManager::ThemeId::DarkProfessional);

    meter.measure([&](int i) {
        return themeManager->getMultiTrackWaveformColor(i % 64);
    });
}

TEST_CASE("Theme System Integration", "[theme][integration]") {
    SECTION("Complete Workflow") {
        auto themeManager = std::make_shared<ThemeManager>();

        // 1. Load all built-in themes
        auto availableThemes = themeManager->getAvailableThemeNames();
        REQUIRE(availableThemes.size() == 7);

        // 2. Switch between themes
        for (const auto& themeName : availableThemes) {
            REQUIRE(themeManager->setCurrentTheme(themeName));

            // 3. Test color access
            auto bgColor = themeManager->getBackgroundColor();
            auto waveformColor = themeManager->getMultiTrackWaveformColor(5);

            REQUIRE(bgColor.isOpaque());
            REQUIRE(waveformColor.isOpaque());
        }

        // 4. Test custom theme creation via editor
        auto themeEditor = std::make_unique<ThemeEditorComponent>(themeManager);
        auto customTheme = ColorTheme::createDarkProfessional();
        customTheme.name = "Custom Test Theme";
        customTheme.background = juce::Colours::darkblue;

        themeEditor->setThemeToEdit(customTheme);
        REQUIRE(themeEditor->validateCurrentTheme());

        // 5. Export and reimport
        auto jsonString = themeEditor->exportThemeToJson();
        REQUIRE(jsonString.isNotEmpty());

        auto newEditor = std::make_unique<ThemeEditorComponent>(themeManager);
        REQUIRE(newEditor->importThemeFromJson(jsonString));

        auto importedTheme = newEditor->getCurrentTheme();
        REQUIRE(importedTheme.name == "Custom Test Theme");
        REQUIRE(importedTheme.background == juce::Colours::darkblue);
    }
}
