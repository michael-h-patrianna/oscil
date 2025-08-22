#!/bin/bash

echo "=== Oscil App Status Check ==="
echo "Current time: $(date)"
echo

# Check if app is running
if pgrep -f "Oscil.app" > /dev/null; then
    echo "✅ Oscil app is running successfully!"
    echo "📱 App window should be visible with oscilloscope interface"
    echo

    # Show process info
    echo "📊 Process Information:"
    ps aux | grep -v grep | grep Oscil | head -1
    echo

    echo "🔧 Current Implementation Status:"
    echo "- ✅ App launches without crashing"
    echo "- ✅ JUCE GUI framework working"
    echo "- ❓ Waveforms may be static (not animated)"
    echo "- ❓ May be using software renderer (not BGFX GPU)"
    echo

    echo "🎯 To see animated waveforms:"
    echo "1. Play audio through the system while app is open"
    echo "2. The oscilloscope should show real-time waveforms"
    echo "3. If lines are flat/static, audio input may not be connected"
    echo

    echo "💡 Next steps to investigate:"
    echo "- Check if audio input is being received"
    echo "- Verify if BGFX or software renderer is being used"
    echo "- Test with different audio sources"

else
    echo "❌ Oscil app is not running"
    echo "Try running: open build/oscil_plugin_artefacts/Debug/Standalone/Oscil.app"
fi
