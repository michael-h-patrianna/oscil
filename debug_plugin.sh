#!/bin/bash

# Test script to check plugin installation and debug loading

echo "=== Oscil Plugin Debugging Script ==="

# Check plugin installation locations
echo ""
echo "1. Checking plugin installations:"
echo "System location: /Library/Audio/Plug-Ins/VST3/Oscil.vst3"
ls -la "/Library/Audio/Plug-Ins/VST3/Oscil.vst3" 2>/dev/null || echo "  Not found in system location"

echo "User location: $HOME/Library/Audio/Plug-Ins/VST3/Oscil.vst3"
ls -la "$HOME/Library/Audio/Plug-Ins/VST3/Oscil.vst3" 2>/dev/null || echo "  Not found in user location"

# Check if we can run the standalone version for basic testing
echo ""
echo "2. Checking built files:"
echo "VST3 bundle:"
ls -la "./build/oscil_plugin_artefacts/Debug/VST3/Oscil.vst3" 2>/dev/null || echo "  VST3 bundle not found"

# Try to get some debug info from the plugin
echo ""
echo "3. Attempting to run basic validation:"

# Create a minimal DAW-like test if possible
echo "Creating minimal test to validate plugin UI..."

# Check for any crash logs that might give us clues
echo ""
echo "4. Checking for recent crash logs:"
recent_crashes=$(ls -t ~/Library/Logs/DiagnosticReports/*Oscil* 2>/dev/null | head -3)
if [ -n "$recent_crashes" ]; then
    echo "Found recent Oscil crash logs:"
    echo "$recent_crashes"
else
    echo "No recent Oscil crash logs found"
fi

echo ""
echo "=== Analysis ==="
echo "The issue is likely that:"
echo "1. BGFX initialization fails (confirmed in tests)"
echo "2. Software renderer fallback might not be working in DAW context"
echo "3. Component may not be receiving paint() calls"
echo "4. Audio data might not be flowing to the ring buffers"

echo ""
echo "Next steps:"
echo "1. Load plugin in DAW and check console logs"
echo "2. Verify audio input is being processed"
echo "3. Check if paintSoftware() is actually being called"
echo "4. Ensure component has proper bounds/visibility"
