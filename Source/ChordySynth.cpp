#include "ChordySynth.h"

//==============================================================================
ChordySynth::ChordySynth()
{
    for (int i = 0; i < 12; ++i)
        addVoice (new ChordyVoice (*this));

    addSound (new ChordySound());
}

//==============================================================================
ChordyVoice::ChordyVoice (ChordySynth& owner) : synthRef (owner) {}

bool ChordyVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<ChordySound*> (sound) != nullptr;
}

void ChordyVoice::startNote (int midiNoteNumber, float velocity,
                              juce::SynthesiserSound*, int)
{
    auto freq = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    carrierIncrement = freq / getSampleRate();
    carrier2Increment = freq * 2.0 / getSampleRate();  // octave partial
    modIncrement = freq * modRatio / getSampleRate();
    carrierPhase = 0.0;
    carrier2Phase = 0.0;
    modPhase = 0.0;

    // Velocity curve — sqrt for natural feel, scaled down
    level = std::sqrt (velocity) * 0.5f;

    // Octave partial louder for high notes (brightness), softer for low notes (warmth)
    float pitchNorm = static_cast<float> (midiNoteNumber - 21) / 87.0f;  // 0 at A0, 1 at C8
    octaveLevel = 0.15f + pitchNorm * 0.2f;  // 0.15 low notes, 0.35 high notes

    // Pitch-dependent decay: low notes ring longer, high notes decay faster
    float decayTime = 2.5f - pitchNorm * 1.5f;  // 2.5s at low end, 1.0s at high end

    // Amp envelope — percussive piano: fast attack, long decay, no sustain
    juce::ADSR::Parameters ampParams { 0.002f, decayTime, 0.0f, 0.2f };
    ampEnvelope.setSampleRate (getSampleRate());
    ampEnvelope.setParameters (ampParams);
    ampEnvelope.noteOn();

    // Mod envelope — harmonic brightness decays faster than amplitude
    // Harder velocity = brighter attack (more mod index sustained briefly)
    float modSustain = velocity * 0.08f;
    juce::ADSR::Parameters modParams { 0.001f, decayTime * 0.35f, modSustain, 0.1f };
    modEnvelope.setSampleRate (getSampleRate());
    modEnvelope.setParameters (modParams);
    modEnvelope.noteOn();
}

void ChordyVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
        modEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        modEnvelope.reset();
        clearCurrentNote();
    }
}

void ChordyVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                    int startSample, int numSamples)
{
    if (! ampEnvelope.isActive())
        return;

    auto vol = synthRef.getVolume();

    while (--numSamples >= 0)
    {
        auto modEnvVal = modEnvelope.getNextSample();
        auto modSignal = std::sin (modPhase * juce::MathConstants<double>::twoPi);
        auto modulation = modSignal * static_cast<double> (modEnvVal * maxModIndex);

        // Fundamental with FM modulation
        auto carrier1 = std::sin (carrierPhase * juce::MathConstants<double>::twoPi
                                   + modulation);
        // Clean octave partial (no FM — adds brightness/shimmer)
        auto carrier2 = std::sin (carrier2Phase * juce::MathConstants<double>::twoPi);

        auto ampEnvVal = ampEnvelope.getNextSample();
        auto sample = static_cast<float> (carrier1 + carrier2 * static_cast<double> (octaveLevel))
                      * ampEnvVal * level * vol;

        for (int ch = outputBuffer.getNumChannels(); --ch >= 0;)
            outputBuffer.addSample (ch, startSample, sample);

        carrierPhase += carrierIncrement;
        carrier2Phase += carrier2Increment;
        modPhase += modIncrement;

        if (carrierPhase >= 1.0)
            carrierPhase -= 1.0;
        if (carrier2Phase >= 1.0)
            carrier2Phase -= 1.0;
        if (modPhase >= 1.0)
            modPhase -= 1.0;

        ++startSample;
    }

    if (! ampEnvelope.isActive())
        clearCurrentNote();
}
