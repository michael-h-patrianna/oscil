/**
 * @file LayoutManager.cpp
 * @brief Implementation of layout management system for multi-track visualization
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2025
 */

#include "LayoutManager.h"
#include <algorithm>
#include <cmath>

namespace oscil::ui::layout {

//==============================================================================
// LayoutRegion Implementation

void LayoutRegion::addTrack(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= 64) return;

    // Check if track is already assigned
    if (!hasTrack(trackIndex)) {
        assignedTracks.push_back(trackIndex);
        std::sort(assignedTracks.begin(), assignedTracks.end());
    }
}

bool LayoutRegion::removeTrack(int trackIndex) {
    auto it = std::find(assignedTracks.begin(), assignedTracks.end(), trackIndex);
    if (it != assignedTracks.end()) {
        assignedTracks.erase(it);
        return true;
    }
    return false;
}

bool LayoutRegion::hasTrack(int trackIndex) const {
    return std::find(assignedTracks.begin(), assignedTracks.end(), trackIndex) != assignedTracks.end();
}

//==============================================================================
// LayoutConfiguration Implementation

int LayoutConfiguration::findRegionForTrack(int trackIndex) const {
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].hasTrack(trackIndex)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<int> LayoutConfiguration::getTracksForRegion(int regionIndex) const {
    if (regionIndex >= 0 && static_cast<size_t>(regionIndex) < regions.size()) {
        return regions[static_cast<size_t>(regionIndex)].assignedTracks;
    }
    return {};
}

//==============================================================================
// LayoutManager Implementation

LayoutManager::LayoutManager() {
    currentConfig.mode = LayoutMode::Overlay;
    recalculateLayout();
}

void LayoutManager::setLayoutMode(LayoutMode mode, bool animated) {
    if (currentConfig.mode == mode) return;

    // Store current track assignments to preserve them
    auto oldAssignments = std::map<int, int>{};
    for (int trackIndex = 0; trackIndex < 64; ++trackIndex) {
        int regionIndex = findRegionForTrack(trackIndex);
        if (regionIndex >= 0) {
            oldAssignments[trackIndex] = regionIndex;
        }
    }

    currentConfig.mode = mode;
    recalculateLayout();

    // Restore track assignments where possible
    for (const auto& [trackIndex, oldRegionIndex] : oldAssignments) {
        // Try to assign to same region index, or distribute if not available
        int newRegionIndex = oldRegionIndex;
        if (newRegionIndex >= static_cast<int>(currentConfig.regions.size())) {
            newRegionIndex = trackIndex % static_cast<int>(currentConfig.regions.size());
        }
        assignTrackToRegion(trackIndex, newRegionIndex);
    }

    // TODO: Implement animation if requested
    if (animated && transitionsEnabled) {
        isAnimating = true;
        // Animation implementation will be added here
    }
}

void LayoutManager::setComponentBounds(const juce::Rectangle<float>& bounds) {
    if (componentBounds != bounds) {
        componentBounds = bounds;
        layoutNeedsRecalculation = true;
        recalculateLayout();
    }
}

bool LayoutManager::assignTrackToRegion(int trackIndex, int regionIndex) {
    if (trackIndex < 0 || trackIndex >= 64) return false;

    // Remove track from all regions first
    removeTrackFromAllRegions(trackIndex);

    // Assign to new region if valid
    if (regionIndex >= 0 && static_cast<size_t>(regionIndex) < currentConfig.regions.size()) {
        currentConfig.regions[static_cast<size_t>(regionIndex)].addTrack(trackIndex);
        return true;
    }

    return false;
}

bool LayoutManager::removeTrackFromAllRegions(int trackIndex) {
    bool removed = false;
    for (auto& region : currentConfig.regions) {
        if (region.removeTrack(trackIndex)) {
            removed = true;
        }
    }
    return removed;
}

int LayoutManager::findRegionForTrack(int trackIndex) const {
    return currentConfig.findRegionForTrack(trackIndex);
}

std::vector<int> LayoutManager::getTracksForRegion(int regionIndex) const {
    return currentConfig.getTracksForRegion(regionIndex);
}

void LayoutManager::autoDistributeTracks(int numTracks) {
    // Clear all existing assignments
    for (auto& region : currentConfig.regions) {
        region.clearTracks();
    }

    // Distribute tracks evenly across regions
    const int numRegions = static_cast<int>(currentConfig.regions.size());
    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex) {
        int regionIndex = trackIndex % numRegions;
        currentConfig.regions[static_cast<size_t>(regionIndex)].addTrack(trackIndex);
    }
}

const LayoutRegion* LayoutManager::getRegion(int regionIndex) const {
    if (regionIndex >= 0 && static_cast<size_t>(regionIndex) < currentConfig.regions.size()) {
        return &currentConfig.regions[static_cast<size_t>(regionIndex)];
    }
    return nullptr;
}

bool LayoutManager::loadFromState(const juce::ValueTree& layoutState) {
    if (!layoutState.isValid() || layoutState.getType() != juce::Identifier("Layout")) {
        return false;
    }

    // Load layout mode
    juce::String modeString = layoutState.getProperty("mode", "Overlay");
    currentConfig.mode = stringToLayoutMode(modeString);

    // Load spacing and border settings
    currentConfig.regionSpacing = layoutState.getProperty("regionSpacing", 2.0f);
    currentConfig.showRegionBorders = layoutState.getProperty("showRegionBorders", false);

    // Recalculate layout with new mode
    recalculateLayout();

    // Load track assignments
    auto trackAssignments = layoutState.getChildWithName("TrackAssignments");
    if (trackAssignments.isValid()) {
        for (auto child : trackAssignments) {
            int trackIndex = child.getProperty("track", -1);
            int regionIndex = child.getProperty("region", -1);
            if (trackIndex >= 0 && regionIndex >= 0) {
                assignTrackToRegion(trackIndex, regionIndex);
            }
        }
    }

    return true;
}

juce::ValueTree LayoutManager::saveToState() const {
    juce::ValueTree layoutState("Layout");

    // Save basic layout settings
    layoutState.setProperty("mode", layoutModeToString(currentConfig.mode), nullptr);
    layoutState.setProperty("regionSpacing", currentConfig.regionSpacing, nullptr);
    layoutState.setProperty("showRegionBorders", currentConfig.showRegionBorders, nullptr);

    // Save track assignments
    auto trackAssignments = juce::ValueTree("TrackAssignments");
    for (int regionIndex = 0; regionIndex < static_cast<int>(currentConfig.regions.size()); ++regionIndex) {
        for (int trackIndex : currentConfig.regions[static_cast<size_t>(regionIndex)].assignedTracks) {
            auto assignment = juce::ValueTree("Assignment");
            assignment.setProperty("track", trackIndex, nullptr);
            assignment.setProperty("region", regionIndex, nullptr);
            trackAssignments.appendChild(assignment, nullptr);
        }
    }
    layoutState.appendChild(trackAssignments, nullptr);

    return layoutState;
}

//==============================================================================
// Static Utility Methods

int LayoutManager::getNumRegionsForMode(LayoutMode mode) {
    switch (mode) {
        case LayoutMode::Overlay:   return 1;
        case LayoutMode::Split2H:   return 2;
        case LayoutMode::Split2V:   return 2;
        case LayoutMode::Split4:    return 4;
        case LayoutMode::Grid2x2:   return 4;
        case LayoutMode::Grid3x3:   return 9;
        case LayoutMode::Grid4x4:   return 16;
        case LayoutMode::Grid6x6:   return 36;
        case LayoutMode::Grid8x8:   return 64;
        default:                    return 1;
    }
}

juce::String LayoutManager::layoutModeToString(LayoutMode mode) {
    switch (mode) {
        case LayoutMode::Overlay:   return "Overlay";
        case LayoutMode::Split2H:   return "Split2H";
        case LayoutMode::Split2V:   return "Split2V";
        case LayoutMode::Split4:    return "Split4";
        case LayoutMode::Grid2x2:   return "Grid2x2";
        case LayoutMode::Grid3x3:   return "Grid3x3";
        case LayoutMode::Grid4x4:   return "Grid4x4";
        case LayoutMode::Grid6x6:   return "Grid6x6";
        case LayoutMode::Grid8x8:   return "Grid8x8";
        default:                    return "Overlay";
    }
}

LayoutMode LayoutManager::stringToLayoutMode(const juce::String& modeString) {
    if (modeString == "Split2H")    return LayoutMode::Split2H;
    if (modeString == "Split2V")    return LayoutMode::Split2V;
    if (modeString == "Split4")     return LayoutMode::Split4;
    if (modeString == "Grid2x2")    return LayoutMode::Grid2x2;
    if (modeString == "Grid3x3")    return LayoutMode::Grid3x3;
    if (modeString == "Grid4x4")    return LayoutMode::Grid4x4;
    if (modeString == "Grid6x6")    return LayoutMode::Grid6x6;
    if (modeString == "Grid8x8")    return LayoutMode::Grid8x8;
    return LayoutMode::Overlay;  // Default fallback
}

//==============================================================================
// Private Implementation Methods

void LayoutManager::recalculateLayout() {
    if (!layoutNeedsRecalculation || componentBounds.isEmpty()) {
        return;
    }

    std::vector<LayoutRegion> newRegions;

    switch (currentConfig.mode) {
        case LayoutMode::Overlay:
            newRegions = calculateOverlayLayout(componentBounds);
            break;

        case LayoutMode::Split2H:
        case LayoutMode::Split2V:
        case LayoutMode::Split4:
            newRegions = calculateSplitLayout(componentBounds, currentConfig.mode);
            break;

        case LayoutMode::Grid2x2:
        case LayoutMode::Grid3x3:
        case LayoutMode::Grid4x4:
        case LayoutMode::Grid6x6:
        case LayoutMode::Grid8x8:
            newRegions = calculateGridLayout(componentBounds, currentConfig.mode);
            break;
    }

    // Preserve existing track assignments where possible
    preserveTrackAssignments(newRegions);

    currentConfig.regions = std::move(newRegions);
    layoutNeedsRecalculation = false;
}

std::vector<LayoutRegion> LayoutManager::calculateOverlayLayout(const juce::Rectangle<float>& bounds) const {
    std::vector<LayoutRegion> regions;
    regions.emplace_back(bounds);
    return regions;
}

std::vector<LayoutRegion> LayoutManager::calculateSplitLayout(const juce::Rectangle<float>& bounds, LayoutMode mode) const {
    std::vector<LayoutRegion> regions;
    const float spacing = currentConfig.regionSpacing;

    switch (mode) {
        case LayoutMode::Split2H: {
            // Horizontal split (top/bottom)
            const float height = (bounds.getHeight() - spacing) / 2.0f;
            regions.emplace_back(bounds.withHeight(height));
            regions.emplace_back(bounds.withY(bounds.getY() + height + spacing).withHeight(height));
            break;
        }

        case LayoutMode::Split2V: {
            // Vertical split (left/right)
            const float width = (bounds.getWidth() - spacing) / 2.0f;
            regions.emplace_back(bounds.withWidth(width));
            regions.emplace_back(bounds.withX(bounds.getX() + width + spacing).withWidth(width));
            break;
        }

        case LayoutMode::Split4: {
            // Quadrant split
            const float width = (bounds.getWidth() - spacing) / 2.0f;
            const float height = (bounds.getHeight() - spacing) / 2.0f;

            // Top-left
            regions.emplace_back(bounds.withWidth(width).withHeight(height));
            // Top-right
            regions.emplace_back(bounds.withX(bounds.getX() + width + spacing).withWidth(width).withHeight(height));
            // Bottom-left
            regions.emplace_back(bounds.withY(bounds.getY() + height + spacing).withWidth(width).withHeight(height));
            // Bottom-right
            regions.emplace_back(bounds.withX(bounds.getX() + width + spacing).withY(bounds.getY() + height + spacing).withWidth(width).withHeight(height));
            break;
        }

        case LayoutMode::Overlay:
        case LayoutMode::Grid2x2:
        case LayoutMode::Grid3x3:
        case LayoutMode::Grid4x4:
        case LayoutMode::Grid6x6:
        case LayoutMode::Grid8x8:
            // These modes are handled by calculateGridLayout
            break;

        default:
            break;
    }

    return regions;
}

std::vector<LayoutRegion> LayoutManager::calculateGridLayout(const juce::Rectangle<float>& bounds, LayoutMode mode) const {
    std::vector<LayoutRegion> regions;

    int gridSize = 0;
    switch (mode) {
        case LayoutMode::Grid2x2: gridSize = 2; break;
        case LayoutMode::Grid3x3: gridSize = 3; break;
        case LayoutMode::Grid4x4: gridSize = 4; break;
        case LayoutMode::Grid6x6: gridSize = 6; break;
        case LayoutMode::Grid8x8: gridSize = 8; break;
        case LayoutMode::Overlay:
        case LayoutMode::Split2H:
        case LayoutMode::Split2V:
        case LayoutMode::Split4:
            // These modes are not grid layouts
            return regions;
        default:
            return regions;
    }

    const float spacing = currentConfig.regionSpacing;
    const float totalSpacingX = spacing * (gridSize - 1);
    const float totalSpacingY = spacing * (gridSize - 1);
    const float cellWidth = (bounds.getWidth() - totalSpacingX) / gridSize;
    const float cellHeight = (bounds.getHeight() - totalSpacingY) / gridSize;

    for (int row = 0; row < gridSize; ++row) {
        for (int col = 0; col < gridSize; ++col) {
            const float x = bounds.getX() + col * (cellWidth + spacing);
            const float y = bounds.getY() + row * (cellHeight + spacing);
            regions.emplace_back(juce::Rectangle<float>(x, y, cellWidth, cellHeight));
        }
    }

    return regions;
}

void LayoutManager::preserveTrackAssignments(std::vector<LayoutRegion>& newRegions) const {
    // Collect current track assignments
    std::map<int, int> trackToRegion;
    for (int regionIndex = 0; regionIndex < static_cast<int>(currentConfig.regions.size()); ++regionIndex) {
        for (int trackIndex : currentConfig.regions[static_cast<size_t>(regionIndex)].assignedTracks) {
            trackToRegion[trackIndex] = regionIndex;
        }
    }

    // Restore assignments to new regions where possible
    for (const auto& [trackIndex, oldRegionIndex] : trackToRegion) {
        int newRegionIndex = oldRegionIndex;

        // If old region index is beyond new region count, distribute evenly
        if (newRegionIndex >= static_cast<int>(newRegions.size())) {
            newRegionIndex = trackIndex % static_cast<int>(newRegions.size());
        }

        newRegions[static_cast<size_t>(newRegionIndex)].addTrack(trackIndex);
    }
}

} // namespace oscil::ui::layout
