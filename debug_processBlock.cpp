void OscilAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const int numChans = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // DEBUG: Generate test signal instead of using input
    static double phase = 0.0;
    static double sampleRate = 44100.0;
    const double frequency = 440.0; // 440Hz sine wave
    const double amplitude = 0.5;
    const double phaseIncrement = 2.0 * 3.14159265359 * frequency / sampleRate;

    // Generate test signal for all channels
    for (int ch = 0; ch < numChans; ++ch) {
        auto* data = buffer.getWritePointer(ch);
        double localPhase = phase;

        for (int sample = 0; sample < numSamples; ++sample) {
            // Add slight phase offset for stereo separation
            double channelOffset = (ch == 1) ? (3.14159265359 * 0.25) : 0.0;
            data[sample] = (float)(amplitude * sin(localPhase + channelOffset));
            localPhase += phaseIncrement;
        }

        // write into ring buffer
        auto& rb = getRingBuffer(ch);
        rb.push(data, (size_t)numSamples);
    }

    // Update global phase
    phase += phaseIncrement * numSamples;
    while (phase > 2.0 * 3.14159265359) {
        phase -= 2.0 * 3.14159265359;
    }
}
