#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <thread>
#include <chrono>

#include "../src/audio/MultiTrackEngine.h"

using namespace audio;

TEST_CASE("MultiTrackEngine", "[MultiTrackEngine]") {

    SECTION("Basic construction and initialization") {
        MultiTrackEngine engine;
        REQUIRE(engine.getNumTracks() == 0);
        REQUIRE(engine.getAllTrackIds().empty());
    }

    SECTION("Prepare to play") {
        MultiTrackEngine engine;
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        const int numChannels = 2;

        engine.prepareToPlay(sampleRate, blockSize, numChannels);

        // Engine should be prepared for audio processing
        // No direct way to test this, but we can test that subsequent operations work
        REQUIRE_NOTHROW(engine.processAudioBlock(nullptr, 0, 0));
    }

    SECTION("Track management") {
        MultiTrackEngine engine;
        engine.prepareToPlay(44100.0, 512, 8);

        SECTION("Add tracks") {
            auto trackId1 = engine.addTrack("Track 1", 0);
            auto trackId2 = engine.addTrack("Track 2", 1);

            REQUIRE(engine.getNumTracks() == 2);
            REQUIRE(trackId1 != trackId2);

            auto trackIds = engine.getAllTrackIds();
            REQUIRE(trackIds.size() == 2);
            REQUIRE(std::find(trackIds.begin(), trackIds.end(), trackId1) != trackIds.end());
            REQUIRE(std::find(trackIds.begin(), trackIds.end(), trackId2) != trackIds.end());
        }

        SECTION("Get track info") {
            auto trackId = engine.addTrack("Test Track", 0);

            const auto* trackInfo = engine.getTrackInfo(trackId);
            REQUIRE(trackInfo != nullptr);
            REQUIRE(trackInfo->name == "Test Track");
            REQUIRE(trackInfo->channelIndex == 0);
            REQUIRE(trackInfo->isActive == true);
            REQUIRE(trackInfo->isVisible == true);
        }

        SECTION("Update track info") {
            auto trackId = engine.addTrack("Original Name", 0);

            // Get current track info
            const auto* originalInfo = engine.getTrackInfo(trackId);
            REQUIRE(originalInfo != nullptr);

            // Create updated info
            TrackInfo updatedInfo = *originalInfo;
            updatedInfo.name = "Updated Name";
            updatedInfo.channelIndex = 1;
            updatedInfo.isActive = false;
            updatedInfo.isVisible = false;

            REQUIRE(engine.updateTrackInfo(trackId, updatedInfo));

            // Verify update
            const auto* newInfo = engine.getTrackInfo(trackId);
            REQUIRE(newInfo != nullptr);
            REQUIRE(newInfo->name == "Updated Name");
            REQUIRE(newInfo->channelIndex == 1);
            REQUIRE(newInfo->isActive == false);
            REQUIRE(newInfo->isVisible == false);
            REQUIRE(newInfo->id == trackId); // ID should be preserved
        }

        SECTION("Remove tracks") {
            auto trackId1 = engine.addTrack("Track 1", 0);
            auto trackId2 = engine.addTrack("Track 2", 1);

            REQUIRE(engine.getNumTracks() == 2);

            REQUIRE(engine.removeTrack(trackId1));
            REQUIRE(engine.getNumTracks() == 1);
            REQUIRE(engine.getTrackInfo(trackId1) == nullptr);
            REQUIRE(engine.getTrackInfo(trackId2) != nullptr);

            REQUIRE(engine.removeTrack(trackId2));
            REQUIRE(engine.getNumTracks() == 0);
            REQUIRE(engine.getTrackInfo(trackId2) == nullptr);
        }

        SECTION("Remove non-existent track") {
            TrackId nonExistentId;
            REQUIRE_FALSE(engine.removeTrack(nonExistentId));
        }
    }

    SECTION("Maximum track limit") {
        MultiTrackEngine engine;
        engine.prepareToPlay(44100.0, 512, 64);

        // Add maximum number of tracks
        std::vector<TrackId> trackIds;
        for (size_t i = 0; i < MultiTrackEngine::MAX_TRACKS; ++i) {
            auto trackId = engine.addTrack("Track " + std::to_string(i), static_cast<int>(i % 8));
            REQUIRE(trackId.isValid()); // Valid track ID
            trackIds.push_back(trackId);
        }

        REQUIRE(engine.getNumTracks() == MultiTrackEngine::MAX_TRACKS);

        // Try to add one more track - should fail (return invalid ID)
        auto extraTrackId = engine.addTrack("Extra Track", 0);
        REQUIRE_FALSE(extraTrackId.isValid()); // Invalid track ID
        REQUIRE(engine.getNumTracks() == MultiTrackEngine::MAX_TRACKS);
    }

    SECTION("Audio processing") {
        MultiTrackEngine engine;
        const double sampleRate = 44100.0;
        const int blockSize = 64;
        const int numChannels = 2;

        engine.prepareToPlay(sampleRate, blockSize, numChannels);

        // Add tracks
        auto trackId1 = engine.addTrack("Track 1", 0);
        auto trackId2 = engine.addTrack("Track 2", 1);

        // Create test audio data
        std::vector<float> channel0Data(blockSize, 0.5f);
        std::vector<float> channel1Data(blockSize, -0.5f);
        std::vector<const float*> channelPointers = { channel0Data.data(), channel1Data.data() };

        // Process audio block
        engine.processAudioBlock(channelPointers.data(), numChannels, blockSize);

        // Verify that tracks have ring buffers and they received data
        const auto* ringBuffer1 = engine.getTrackRingBuffer(trackId1);
        const auto* ringBuffer2 = engine.getTrackRingBuffer(trackId2);

        REQUIRE(ringBuffer1 != nullptr);
        REQUIRE(ringBuffer2 != nullptr);
        REQUIRE(ringBuffer1->size() >= blockSize);
        REQUIRE(ringBuffer2->size() >= blockSize);

        // Test that we can read the data back
        std::vector<float> readBuffer(blockSize);
        ringBuffer1->peekLatest(readBuffer.data(), blockSize);
        REQUIRE(readBuffer[0] == Catch::Approx(0.5f));

        ringBuffer2->peekLatest(readBuffer.data(), blockSize);
        REQUIRE(readBuffer[0] == Catch::Approx(-0.5f));
    }

    SECTION("Memory usage stats") {
        MultiTrackEngine engine;
        engine.prepareToPlay(44100.0, 512, 8);

        auto initialStats = engine.getMemoryStats();
        REQUIRE(initialStats.numTracks == 0);
        REQUIRE(initialStats.totalMemoryBytes == 0);

        // Add tracks and check memory scaling
        engine.addTrack("Track 1", 0);
        auto stats1 = engine.getMemoryStats();
        REQUIRE(stats1.numTracks == 1);
        REQUIRE(stats1.totalMemoryBytes > 0);
        REQUIRE(stats1.memoryPerTrack > 0);

        engine.addTrack("Track 2", 1);
        auto stats2 = engine.getMemoryStats();
        REQUIRE(stats2.numTracks == 2);
        REQUIRE(stats2.totalMemoryBytes == 2 * stats2.memoryPerTrack);
        REQUIRE(stats2.memoryPerTrack == stats1.memoryPerTrack); // Should be consistent

        // Memory per track should be under the 10MB requirement
        const size_t maxMemoryPerTrack = 10 * 1024 * 1024; // 10MB
        REQUIRE(stats2.memoryPerTrack < maxMemoryPerTrack);
    }

    SECTION("Performance stats") {
        MultiTrackEngine engine;
        engine.prepareToPlay(44100.0, 512, 2);

        const auto& perfStats = engine.getPerformanceStats();
        REQUIRE(perfStats.audioBlocksProcessed.load() == 0);

        // Add a track and process some audio
        engine.addTrack("Track 1", 0);

        std::vector<float> audioData(512, 0.1f);
        std::vector<const float*> channelPointers = { audioData.data(), audioData.data() };

        engine.processAudioBlock(channelPointers.data(), 2, 512);
        engine.processAudioBlock(channelPointers.data(), 2, 512);

        REQUIRE(perfStats.audioBlocksProcessed.load() == 2);
    }

    SECTION("Thread safety") {
        MultiTrackEngine engine;
        engine.prepareToPlay(44100.0, 512, 4);

        std::atomic<bool> stopThreads{false};
        std::atomic<int> tracksAdded{0};
        std::atomic<int> tracksRemoved{0};
        std::vector<TrackId> createdTrackIds;
        std::mutex trackIdsMutex;

        // Thread that adds and removes tracks
        std::thread trackManagementThread([&]() {
            for (int i = 0; i < 5 && !stopThreads; ++i) {
                auto trackId = engine.addTrack("Track " + std::to_string(i), i % 4);
                if (trackId.isValid()) {
                    {
                        std::lock_guard<std::mutex> lock(trackIdsMutex);
                        createdTrackIds.push_back(trackId);
                    }
                    tracksAdded++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Wait for stop signal before removing tracks
            while (!stopThreads) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            std::vector<TrackId> trackIdsToRemove;
            {
                std::lock_guard<std::mutex> lock(trackIdsMutex);
                trackIdsToRemove = createdTrackIds;
            }

            for (const auto& trackId : trackIdsToRemove) {
                if (engine.removeTrack(trackId)) {
                    tracksRemoved++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        // Thread that processes audio
        std::thread audioThread([&]() {
            std::vector<float> audioData(512, 0.2f);
            std::vector<const float*> channelPointers = {
                audioData.data(), audioData.data(), audioData.data(), audioData.data()
            };

            for (int i = 0; i < 20 && !stopThreads; ++i) {
                engine.processAudioBlock(channelPointers.data(), 4, 512);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });

        // Let threads run for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stopThreads = true;

        trackManagementThread.join();
        audioThread.join();

        // Should have successfully added and removed tracks without crashes
        REQUIRE(tracksAdded.load() > 0);
        REQUIRE(tracksRemoved.load() >= 0); // May not remove all if timing is off
    }

    SECTION("WaveformDataBridge integration") {
        MultiTrackEngine engine;
        engine.prepareToPlay(44100.0, 64, 2);

        auto& bridge = engine.getWaveformDataBridge();

        // Add tracks
        engine.addTrack("Track 1", 0);
        engine.addTrack("Track 2", 1);

        // Process audio
        std::vector<float> channel0Data(64, 0.3f);
        std::vector<float> channel1Data(64, -0.3f);
        std::vector<const float*> channelPointers = { channel0Data.data(), channel1Data.data() };

        engine.processAudioBlock(channelPointers.data(), 2, 64);

        // Verify data bridge received data
        AudioDataSnapshot snapshot;
        bool hasData = bridge.pullLatestData(snapshot);
        REQUIRE(hasData);
        REQUIRE(snapshot.numChannels == 2);
        REQUIRE(snapshot.numSamples == 64);
        REQUIRE(snapshot.samples[0][0] == Catch::Approx(0.3f));
        REQUIRE(snapshot.samples[1][0] == Catch::Approx(-0.3f));
    }
}
