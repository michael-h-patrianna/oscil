/**
 * @file test_layout_manager.cpp
 * @brief Comprehensive unit tests for the LayoutManager system
 *
 * Tests all layout modes, track assignment functionality, state persistence,
 * and performance characteristics of the layout management system.
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2025
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/ui/layout/LayoutManager.h"
#include <juce_core/juce_core.h>
#include <chrono>

using namespace oscil::ui::layout;

TEST_CASE("LayoutManager - Basic Construction", "[layout][basic]") {
    LayoutManager manager;

    SECTION("Default state") {
        REQUIRE(manager.getLayoutMode() == LayoutMode::Overlay);
        REQUIRE(manager.getNumRegions() == 1);
        REQUIRE_FALSE(manager.isTransitioning());
        REQUIRE(manager.areTransitionsEnabled() == true);
        REQUIRE(manager.getTransitionDuration() == 100);
    }
}

TEST_CASE("LayoutManager - Layout Mode Switching", "[layout][modes]") {
    LayoutManager manager;
    manager.setComponentBounds({0, 0, 800, 600});

    SECTION("All layout modes have correct region counts") {
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Overlay) == 1);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Split2H) == 2);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Split2V) == 2);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Split4) == 4);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Grid2x2) == 4);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Grid3x3) == 9);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Grid4x4) == 16);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Grid6x6) == 36);
        REQUIRE(LayoutManager::getNumRegionsForMode(LayoutMode::Grid8x8) == 64);
    }

    SECTION("Switch to Split2H mode") {
        manager.setLayoutMode(LayoutMode::Split2H, false);
        REQUIRE(manager.getLayoutMode() == LayoutMode::Split2H);
        REQUIRE(manager.getNumRegions() == 2);

        // Check region bounds for horizontal split
        const auto* region0 = manager.getRegion(0);
        const auto* region1 = manager.getRegion(1);
        REQUIRE(region0 != nullptr);
        REQUIRE(region1 != nullptr);

        // Top region should be in upper half
        REQUIRE(region0->bounds.getY() == Catch::Approx(0.0f));
        REQUIRE(region0->bounds.getHeight() == Catch::Approx(299.0f).margin(1.0f)); // (600-2)/2

        // Bottom region should be in lower half
        REQUIRE(region1->bounds.getY() == Catch::Approx(301.0f).margin(1.0f)); // 299 + 2 spacing
        REQUIRE(region1->bounds.getHeight() == Catch::Approx(299.0f).margin(1.0f));
    }

    SECTION("Switch to Grid3x3 mode") {
        manager.setLayoutMode(LayoutMode::Grid3x3, false);
        REQUIRE(manager.getLayoutMode() == LayoutMode::Grid3x3);
        REQUIRE(manager.getNumRegions() == 9);

        // Check that all regions have reasonable bounds
        for (int i = 0; i < 9; ++i) {
            const auto* region = manager.getRegion(i);
            REQUIRE(region != nullptr);
            REQUIRE(region->bounds.getWidth() > 0);
            REQUIRE(region->bounds.getHeight() > 0);
            REQUIRE(region->bounds.getX() >= 0);
            REQUIRE(region->bounds.getY() >= 0);
        }
    }
}

TEST_CASE("LayoutManager - Track Assignment", "[layout][tracks]") {
    LayoutManager manager;
    manager.setComponentBounds({0, 0, 800, 600});
    manager.setLayoutMode(LayoutMode::Grid2x2, false);

    SECTION("Basic track assignment") {
        REQUIRE(manager.assignTrackToRegion(0, 0) == true);
        REQUIRE(manager.assignTrackToRegion(1, 1) == true);
        REQUIRE(manager.assignTrackToRegion(2, 2) == true);
        REQUIRE(manager.assignTrackToRegion(3, 3) == true);

        REQUIRE(manager.findRegionForTrack(0) == 0);
        REQUIRE(manager.findRegionForTrack(1) == 1);
        REQUIRE(manager.findRegionForTrack(2) == 2);
        REQUIRE(manager.findRegionForTrack(3) == 3);
    }

    SECTION("Invalid track assignments") {
        REQUIRE(manager.assignTrackToRegion(-1, 0) == false);  // Invalid track
        REQUIRE(manager.assignTrackToRegion(64, 0) == false);  // Track out of range
        REQUIRE(manager.assignTrackToRegion(0, -1) == false);  // Invalid region
        REQUIRE(manager.assignTrackToRegion(0, 10) == false);  // Region out of range
    }

    SECTION("Track reassignment") {
        manager.assignTrackToRegion(0, 0);
        REQUIRE(manager.findRegionForTrack(0) == 0);

        // Reassign to different region
        manager.assignTrackToRegion(0, 1);
        REQUIRE(manager.findRegionForTrack(0) == 1);

        // Should no longer be in region 0
        auto tracksInRegion0 = manager.getTracksForRegion(0);
        REQUIRE(std::find(tracksInRegion0.begin(), tracksInRegion0.end(), 0) == tracksInRegion0.end());
    }

    SECTION("Auto-distribution of tracks") {
        manager.autoDistributeTracks(8);

        // Verify tracks are distributed across 4 regions (2x2 grid)
        for (int regionIndex = 0; regionIndex < 4; ++regionIndex) {
            auto tracks = manager.getTracksForRegion(regionIndex);
            REQUIRE(tracks.size() == 2);  // 8 tracks / 4 regions = 2 tracks per region
        }

        // Verify all tracks 0-7 are assigned
        for (int trackIndex = 0; trackIndex < 8; ++trackIndex) {
            REQUIRE(manager.findRegionForTrack(trackIndex) >= 0);
        }
    }
}

TEST_CASE("LayoutManager - Layout Mode Preservation", "[layout][preservation]") {
    LayoutManager manager;
    manager.setComponentBounds({0, 0, 800, 600});

    SECTION("Track assignments preserved when switching compatible layouts") {
        manager.setLayoutMode(LayoutMode::Grid2x2, false);
        manager.assignTrackToRegion(0, 0);
        manager.assignTrackToRegion(1, 1);
        manager.assignTrackToRegion(2, 2);
        manager.assignTrackToRegion(3, 3);

        // Switch to Split4 (same number of regions)
        manager.setLayoutMode(LayoutMode::Split4, false);
        REQUIRE(manager.findRegionForTrack(0) == 0);
        REQUIRE(manager.findRegionForTrack(1) == 1);
        REQUIRE(manager.findRegionForTrack(2) == 2);
        REQUIRE(manager.findRegionForTrack(3) == 3);
    }

    SECTION("Track assignments redistributed when switching to fewer regions") {
        manager.setLayoutMode(LayoutMode::Grid3x3, false);
        for (int i = 0; i < 9; ++i) {
            manager.assignTrackToRegion(i, i);
        }

        // Switch to Split2H (2 regions)
        manager.setLayoutMode(LayoutMode::Split2H, false);
        REQUIRE(manager.getNumRegions() == 2);

        // All tracks should still be assigned somewhere
        for (int i = 0; i < 9; ++i) {
            int regionIndex = manager.findRegionForTrack(i);
            REQUIRE(regionIndex >= 0);
            REQUIRE(regionIndex < 2);
        }
    }
}

TEST_CASE("LayoutManager - State Persistence", "[layout][persistence]") {
    LayoutManager manager;
    manager.setComponentBounds({0, 0, 800, 600});

    SECTION("Save and load layout state") {
        // Configure layout
        manager.setLayoutMode(LayoutMode::Grid2x2, false);
        manager.assignTrackToRegion(0, 0);
        manager.assignTrackToRegion(1, 1);
        manager.assignTrackToRegion(2, 2);
        manager.assignTrackToRegion(3, 3);

        // Save state
        auto savedState = manager.saveToState();
        REQUIRE(savedState.isValid());
        REQUIRE(savedState.getType() == juce::Identifier("Layout"));

        // Create new manager and load state
        LayoutManager newManager;
        newManager.setComponentBounds({0, 0, 800, 600});
        REQUIRE(newManager.loadFromState(savedState) == true);

        // Verify state was restored
        REQUIRE(newManager.getLayoutMode() == LayoutMode::Grid2x2);
        REQUIRE(newManager.findRegionForTrack(0) == 0);
        REQUIRE(newManager.findRegionForTrack(1) == 1);
        REQUIRE(newManager.findRegionForTrack(2) == 2);
        REQUIRE(newManager.findRegionForTrack(3) == 3);
    }

    SECTION("Invalid state handling") {
        juce::ValueTree invalidState("InvalidType");
        REQUIRE(manager.loadFromState(invalidState) == false);

        juce::ValueTree emptyState;
        REQUIRE(manager.loadFromState(emptyState) == false);
    }
}

TEST_CASE("LayoutManager - String Conversion", "[layout][strings]") {
    SECTION("Layout mode to string conversion") {
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Overlay) == "Overlay");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Split2H) == "Split2H");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Split2V) == "Split2V");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Split4) == "Split4");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Grid2x2) == "Grid2x2");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Grid3x3) == "Grid3x3");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Grid4x4) == "Grid4x4");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Grid6x6) == "Grid6x6");
        REQUIRE(LayoutManager::layoutModeToString(LayoutMode::Grid8x8) == "Grid8x8");
    }

    SECTION("String to layout mode conversion") {
        REQUIRE(LayoutManager::stringToLayoutMode("Overlay") == LayoutMode::Overlay);
        REQUIRE(LayoutManager::stringToLayoutMode("Split2H") == LayoutMode::Split2H);
        REQUIRE(LayoutManager::stringToLayoutMode("Split2V") == LayoutMode::Split2V);
        REQUIRE(LayoutManager::stringToLayoutMode("Split4") == LayoutMode::Split4);
        REQUIRE(LayoutManager::stringToLayoutMode("Grid2x2") == LayoutMode::Grid2x2);
        REQUIRE(LayoutManager::stringToLayoutMode("Grid3x3") == LayoutMode::Grid3x3);
        REQUIRE(LayoutManager::stringToLayoutMode("Grid4x4") == LayoutMode::Grid4x4);
        REQUIRE(LayoutManager::stringToLayoutMode("Grid6x6") == LayoutMode::Grid6x6);
        REQUIRE(LayoutManager::stringToLayoutMode("Grid8x8") == LayoutMode::Grid8x8);

        // Invalid string should return Overlay
        REQUIRE(LayoutManager::stringToLayoutMode("InvalidMode") == LayoutMode::Overlay);
        REQUIRE(LayoutManager::stringToLayoutMode("") == LayoutMode::Overlay);
    }
}

TEST_CASE("LayoutManager - Performance Characteristics", "[layout][performance]") {
    LayoutManager manager;
    manager.setComponentBounds({0, 0, 1920, 1080}); // High resolution

    SECTION("Layout switching performance") {
        auto start = std::chrono::high_resolution_clock::now();

        // Switch through all layout modes
        manager.setLayoutMode(LayoutMode::Split2H, false);
        manager.setLayoutMode(LayoutMode::Split2V, false);
        manager.setLayoutMode(LayoutMode::Split4, false);
        manager.setLayoutMode(LayoutMode::Grid2x2, false);
        manager.setLayoutMode(LayoutMode::Grid3x3, false);
        manager.setLayoutMode(LayoutMode::Grid4x4, false);
        manager.setLayoutMode(LayoutMode::Grid6x6, false);
        manager.setLayoutMode(LayoutMode::Grid8x8, false);
        manager.setLayoutMode(LayoutMode::Overlay, false);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should complete all switches in reasonable time
        REQUIRE(duration.count() < 10); // Less than 10ms for all switches
    }

    SECTION("Track assignment performance") {
        manager.setLayoutMode(LayoutMode::Grid8x8, false);

        auto start = std::chrono::high_resolution_clock::now();

        // Assign 64 tracks
        for (int i = 0; i < 64; ++i) {
            manager.assignTrackToRegion(i, i % 64);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Should complete all assignments in reasonable time
        REQUIRE(duration.count() < 1000); // Less than 1ms for 64 track assignments
    }

    SECTION("State save/load performance") {
        manager.setLayoutMode(LayoutMode::Grid8x8, false);
        manager.autoDistributeTracks(64);

        auto start = std::chrono::high_resolution_clock::now();

        auto state = manager.saveToState();

        auto saveEnd = std::chrono::high_resolution_clock::now();

        LayoutManager newManager;
        newManager.setComponentBounds({0, 0, 1920, 1080});
        newManager.loadFromState(state);

        auto loadEnd = std::chrono::high_resolution_clock::now();

        auto saveDuration = std::chrono::duration_cast<std::chrono::microseconds>(saveEnd - start);
        auto loadDuration = std::chrono::duration_cast<std::chrono::microseconds>(loadEnd - saveEnd);

        // Both operations should be fast
        REQUIRE(saveDuration.count() < 1000); // Less than 1ms to save
        REQUIRE(loadDuration.count() < 1000); // Less than 1ms to load
    }
}

TEST_CASE("LayoutManager - Edge Cases", "[layout][edge]") {
    LayoutManager manager;

    SECTION("Zero component bounds") {
        manager.setComponentBounds({0, 0, 0, 0});
        manager.setLayoutMode(LayoutMode::Grid2x2, false);

        // Should handle gracefully without crashing
        REQUIRE(manager.getNumRegions() == 4);

        for (int i = 0; i < 4; ++i) {
            const auto* region = manager.getRegion(i);
            REQUIRE(region != nullptr);
            // Bounds may be empty but should exist
        }
    }

    SECTION("Very small component bounds") {
        manager.setComponentBounds({0, 0, 1, 1});
        manager.setLayoutMode(LayoutMode::Grid8x8, false);

        // Should handle gracefully
        REQUIRE(manager.getNumRegions() == 64);
    }

    SECTION("Maximum track assignments") {
        manager.setComponentBounds({0, 0, 800, 600});
        manager.setLayoutMode(LayoutMode::Grid8x8, false);

        // Assign all 64 possible tracks
        for (int i = 0; i < 64; ++i) {
            REQUIRE(manager.assignTrackToRegion(i, i) == true);
        }

        // Verify all assignments
        for (int i = 0; i < 64; ++i) {
            REQUIRE(manager.findRegionForTrack(i) == i);
        }
    }
}
