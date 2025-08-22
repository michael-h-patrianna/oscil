#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include "theme/ColorTheme.h"
#include "theme/ThemeManager.h"

namespace oscil {

TEST_CASE("ColorTheme - Basic Construction", "[theme]") {
    const auto theme = oscil::theme::ColorTheme::createDarkProfessional();

    REQUIRE(theme.name == "Dark Professional");
    REQUIRE(theme.background != juce::Colour());
    REQUIRE(theme.grid != juce::Colour());
    REQUIRE(theme.text != juce::Colour());
    REQUIRE(theme.waveformColors.size() == 8);
}

TEST_CASE("ColorTheme - Light Theme", "[theme]") {
    const auto theme = oscil::theme::ColorTheme::createLightModern();

    REQUIRE(theme.name == "Light Modern");
    REQUIRE(theme.background != juce::Colour());
    REQUIRE(theme.grid != juce::Colour());
    REQUIRE(theme.text != juce::Colour());
    REQUIRE(theme.waveformColors.size() == 8);
}

TEST_CASE("ColorTheme - Accessibility Validation", "[theme]") {
    const auto darkTheme = oscil::theme::ColorTheme::createDarkProfessional();
    const auto lightTheme = oscil::theme::ColorTheme::createLightModern();

    REQUIRE(darkTheme.validateAccessibility());
    REQUIRE(lightTheme.validateAccessibility());
}

TEST_CASE("ThemeManager - Construction and Defaults", "[theme]") {
    oscil::theme::ThemeManager manager;

    REQUIRE(manager.getCurrentTheme().name == "Dark Professional");

    // Test that we have available themes (debug the issue)
    auto availableThemes = manager.getAvailableThemeNames();
    INFO("Available themes count: " << availableThemes.size());
    for (int i = 0; i < availableThemes.size(); ++i) {
        INFO("Theme " << i << ": '" << availableThemes[i].toStdString() << "'");
    }

    // Check if we can create themes directly to see if they have the right names
    auto darkTheme = oscil::theme::ColorTheme::createDarkProfessional();
    auto lightTheme = oscil::theme::ColorTheme::createLightModern();
    INFO("Dark theme name: '" << darkTheme.name.toStdString() << "'");
    INFO("Light theme name: '" << lightTheme.name.toStdString() << "'");

    REQUIRE(availableThemes.size() >= 1); // At least one theme should be available
}

TEST_CASE("ThemeManager - Theme Switching", "[theme]") {
    oscil::theme::ThemeManager manager;
    bool callbackTriggered = false;

    manager.addThemeChangeListener([&callbackTriggered](const oscil::theme::ColorTheme&) {
        callbackTriggered = true;
    });

    // Test switching to light theme
    bool success = manager.setCurrentTheme("Light Modern");
    INFO("setCurrentTheme result: " << success);
    INFO("Current theme after attempt: " << manager.getCurrentTheme().name.toStdString());

    REQUIRE(success);
    REQUIRE(manager.getCurrentTheme().name == "Light Modern");
    REQUIRE(callbackTriggered);

    // Test invalid theme
    REQUIRE_FALSE(manager.setCurrentTheme("Nonexistent Theme"));
    REQUIRE(manager.getCurrentTheme().name == "Light Modern"); // Should remain unchanged
}

TEST_CASE("ThemeManager - Waveform Color Access", "[theme]") {
    oscil::theme::ThemeManager manager;

    // Test valid channel indices
    for (int i = 0; i < 8; ++i) {
        const auto color = manager.getWaveformColor(i);
        REQUIRE(color != juce::Colour()); // Should not be default/black
    }

    // Test wraparound for higher channel numbers
    const auto color0 = manager.getWaveformColor(0);
    const auto color8 = manager.getWaveformColor(8);
    REQUIRE(color0 == color8); // Should wrap around
}

TEST_CASE("ThemeManager - Performance Requirements", "[theme]") {
    oscil::theme::ThemeManager manager;

    // Test theme switching performance (<50ms requirement)
    const auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        manager.setCurrentTheme(i % 2 == 0 ? "Dark Professional" : "Light Modern");
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime
    ).count();

    // 100 theme switches should complete well under 50ms total
    REQUIRE(duration < 50);
}

TEST_CASE("ThemeManager - Thread Safety", "[theme]") {
    oscil::theme::ThemeManager manager;
    std::atomic<int> completedTasks{0};

    // Test concurrent access to theme colors
    const int numThreads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&manager, &completedTasks]() {
            for (int i = 0; i < 100; ++i) {
                // Mix of read operations
                volatile auto color = manager.getWaveformColor(i % 8);
                volatile auto name = manager.getCurrentTheme().name;
                (void)color; // Suppress unused variable warning
                (void)name;
            }
            completedTasks.fetch_add(100);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE(completedTasks.load() == numThreads * 100);
}

} // namespace oscil
