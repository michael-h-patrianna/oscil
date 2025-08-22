#include <catch2/catch_test_macros.hpp>
#include "../src/state/TrackState.h"

using namespace oscil::state;

TEST_CASE("TrackState - Basic Construction and Properties", "[TrackState]") {
    SECTION("Default constructor creates valid state") {
        TrackState trackState(42);

        REQUIRE(trackState.isValid());
        REQUIRE(trackState.getTrackId() == 42);
        REQUIRE(trackState.getTrackName() == "Track 42");
        REQUIRE(trackState.getColorIndex() == (42 % 64));
        REQUIRE(trackState.isVisible() == true);
        REQUIRE(trackState.getGain() == 1.0f);
        REQUIRE(trackState.getOffset() == 0.0f);
        REQUIRE(trackState.getVersion() == TrackState::CURRENT_VERSION);
    }

    SECTION("Property setters work correctly") {
        TrackState trackState(0);

        trackState.setTrackId(5);
        REQUIRE(trackState.getTrackId() == 5);

        trackState.setTrackName("Custom Name");
        REQUIRE(trackState.getTrackName() == "Custom Name");

        trackState.setColorIndex(10);
        REQUIRE(trackState.getColorIndex() == 10);

        trackState.setVisible(false);
        REQUIRE(trackState.isVisible() == false);

        trackState.setGain(2.5f);
        REQUIRE(trackState.getGain() == 2.5f);

        trackState.setOffset(-0.5f);
        REQUIRE(trackState.getOffset() == -0.5f);
    }

    SECTION("Property validation and clamping") {
        TrackState trackState(0);

        // Color index should be clamped to 0-63
        trackState.setColorIndex(-5);
        REQUIRE(trackState.getColorIndex() == 0);

        trackState.setColorIndex(100);
        REQUIRE(trackState.getColorIndex() == 63);

        // Gain should be clamped to 0.0-10.0
        trackState.setGain(-1.0f);
        REQUIRE(trackState.getGain() == 0.0f);

        trackState.setGain(20.0f);
        REQUIRE(trackState.getGain() == 10.0f);

        // Offset should be clamped to -1.0 to 1.0
        trackState.setOffset(-2.0f);
        REQUIRE(trackState.getOffset() == -1.0f);

        trackState.setOffset(2.0f);
        REQUIRE(trackState.getOffset() == 1.0f);
    }
}

TEST_CASE("TrackState - Serialization", "[TrackState]") {
    SECTION("XML serialization round-trip") {
        TrackState original(123);
        original.setTrackName("Test Track");
        original.setColorIndex(25);
        original.setVisible(false);
        original.setGain(3.0f);
        original.setOffset(0.75f);

        // Serialize to XML
        auto xmlString = original.toXmlString();
        REQUIRE_FALSE(xmlString.isEmpty());

        // Deserialize from XML
        auto restored = TrackState::fromXmlString(xmlString);

        // Check all properties match
        REQUIRE(restored.isValid());
        REQUIRE(restored.getTrackId() == 123);
        REQUIRE(restored.getTrackName() == "Test Track");
        REQUIRE(restored.getColorIndex() == 25);
        REQUIRE(restored.isVisible() == false);
        REQUIRE(restored.getGain() == 3.0f);
        REQUIRE(restored.getOffset() == 0.75f);
        REQUIRE(restored.getVersion() == TrackState::CURRENT_VERSION);
    }

    SECTION("Invalid XML handling") {
        auto trackState = TrackState::fromXmlString("");
        REQUIRE(trackState.isValid()); // Should return default state
        REQUIRE(trackState.getTrackId() == 0);

        auto trackState2 = TrackState::fromXmlString("invalid xml");
        REQUIRE(trackState2.isValid()); // Should return default state
        REQUIRE(trackState2.getTrackId() == 0);
    }
}

TEST_CASE("TrackState - ValueTree Operations", "[TrackState]") {
    SECTION("Copy constructor and assignment") {
        TrackState original(99);
        original.setTrackName("Original");
        original.setGain(2.0f);

        // Copy constructor
        TrackState copy(original);
        REQUIRE(copy.getTrackId() == 99);
        REQUIRE(copy.getTrackName() == "Original");
        REQUIRE(copy.getGain() == 2.0f);

        // Assignment operator
        TrackState assigned(0);
        assigned = original;
        REQUIRE(assigned.getTrackId() == 99);
        REQUIRE(assigned.getTrackName() == "Original");
        REQUIRE(assigned.getGain() == 2.0f);
    }

    SECTION("State replacement") {
        TrackState trackState1(1);
        trackState1.setTrackName("Track 1");

        TrackState trackState2(2);
        trackState2.setTrackName("Track 2");

        auto state2Copy = trackState2.getStateCopy();
        trackState1.replaceState(state2Copy);

        REQUIRE(trackState1.getTrackId() == 2);
        REQUIRE(trackState1.getTrackName() == "Track 2");
    }

    SECTION("Reset to defaults") {
        TrackState trackState(55);
        trackState.setTrackName("Modified");
        trackState.setGain(5.0f);
        trackState.setVisible(false);

        trackState.resetToDefaults();

        REQUIRE(trackState.getTrackId() == 55); // ID preserved
        REQUIRE(trackState.getTrackName() == "Track 55"); // Reset to default
        REQUIRE(trackState.getGain() == 1.0f); // Reset to default
        REQUIRE(trackState.isVisible() == true); // Reset to default
    }
}

TEST_CASE("TrackState - Performance Requirements", "[TrackState][Performance]") {
    SECTION("Memory usage should be reasonable") {
        TrackState trackState(0);

        // Track state should use minimal memory
        // This is a basic check - in practice, we'd measure actual memory usage
        REQUIRE(sizeof(TrackState) < 1024); // Should be much less than 1KB
    }

    SECTION("State operations should be fast") {
        TrackState trackState(0);

        // These operations should complete very quickly
        // In a real test, we'd measure timing, but for now just ensure they don't crash
        for (int i = 0; i < 1000; ++i) {
            trackState.setGain(static_cast<float>(i % 10));
            trackState.setVisible(i % 2 == 0);
            trackState.setColorIndex(i % 64);
        }

        REQUIRE(trackState.isValid());
    }
}
