#include <catch2/catch_test_macros.hpp>
#include "theme/ColorTheme.h"
#include "theme/ThemeManager.h"

namespace oscil {

TEST_CASE("Debug Theme Manager Issues", "[debug]") {
    // Test theme creation directly first
    auto darkTheme = oscil::theme::ColorTheme::createDarkProfessional();
    auto lightTheme = oscil::theme::ColorTheme::createLightModern();

    INFO("Dark theme name: '" << darkTheme.name.toStdString() << "'");
    INFO("Light theme name: '" << lightTheme.name.toStdString() << "'");

    REQUIRE(darkTheme.name == "Dark Professional");
    REQUIRE(lightTheme.name == "Light Modern");
    REQUIRE(darkTheme.name != lightTheme.name);

    // Test ThemeManager construction
    oscil::theme::ThemeManager manager;

    auto availableThemes = manager.getAvailableThemeNames();
    INFO("Available themes count: " << availableThemes.size());
    for (int i = 0; i < availableThemes.size(); ++i) {
        INFO("Available theme " << i << ": '" << availableThemes[i].toStdString() << "'");
    }

    INFO("Current theme: '" << manager.getCurrentTheme().name.toStdString() << "'");

    // Test if we can get each theme by name
    auto* darkPtr = manager.getTheme("Dark Professional");
    auto* lightPtr = manager.getTheme("Light Modern");

    INFO("Dark theme pointer: " << (darkPtr ? "found" : "null"));
    INFO("Light theme pointer: " << (lightPtr ? "found" : "null"));

    if (darkPtr) {
        INFO("Found dark theme name: '" << darkPtr->name.toStdString() << "'");
    }
    if (lightPtr) {
        INFO("Found light theme name: '" << lightPtr->name.toStdString() << "'");
    }

    REQUIRE(availableThemes.size() >= 1);
}

} // namespace oscil
