#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class ChordySound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class ChordySynth;

class ChordyVoice : public juce::SynthesiserVoice
{
public:
    explicit ChordyVoice (ChordySynth& owner);

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int pitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

private:
    ChordySynth& synthRef;

    // Carrier (fundamental) + second partial (octave)
    double carrierPhase = 0.0;
    double carrier2Phase = 0.0;
    double modPhase = 0.0;
    double carrierIncrement = 0.0;
    double carrier2Increment = 0.0;
    double modIncrement = 0.0;

    juce::ADSR ampEnvelope;
    juce::ADSR modEnvelope;

    float level = 0.0f;
    float octaveLevel = 0.0f;  // second partial mix level

    static constexpr double modRatio = 1.0;
    static constexpr float maxModIndex = 3.0f;
};

class ChordySynth : public juce::Synthesiser
{
public:
    ChordySynth();

    void setVolume (float vol) { volume.store (vol, std::memory_order_relaxed); }
    float getVolume() const { return volume.load (std::memory_order_relaxed); }

private:
    friend class ChordyVoice;
    std::atomic<float> volume { 0.7f };
};
