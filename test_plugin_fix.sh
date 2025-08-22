#!/bin/bash

echo "=== TESTING UPDATED PLUGIN WITH FIXED FALLBACK LOGIC ==="

# Test the console output when loading plugin
echo ""
echo "Testing plugin debug output:"
echo "1. The plugin should now properly fallback to software rendering"
echo "2. You should see debug logs showing the fallback process"
echo "3. The paintSoftware() method should be called instead of BGFX path"

echo ""
echo "To test manually:"
echo "1. Open Logic Pro or Ableton Live"
echo "2. Load Oscil plugin on an audio track"
echo "3. Send audio to the track (play some music)"
echo "4. Check the Console app for debug messages from Oscil"
echo "5. Verify that the plugin window shows waveform visualization"

echo ""
echo "Expected debug sequence:"
echo "- 'OscilloscopeComponent: Constructor called'"
echo "- 'OscilloscopeComponent: attempting headless bgfx initialization first'"
echo "- 'BgfxRendererBackend: configuring for headless/offscreen rendering'"
echo "- 'BGFX Init failed'"
echo "- 'OscilloscopeComponent: falling back to software renderer'"
echo "- 'OscilloscopeComponent: software renderer init succeeded'"
echo "- 'paintSoftware called - bounds [width]x[height]'"
echo "- 'paintSoftware channels=2'"
echo "- 'paintSoftware ch0 N=1024 first=[audio_data]'"

echo ""
echo "Key fix: Added isHardwareRenderer() method to distinguish BGFX from software"
echo "Now paintWithRenderer() correctly calls paintSoftware() for software renderers"
echo ""
echo "Plugin installed at: $HOME/Library/Audio/Plug-Ins/VST3/Oscil.vst3"
