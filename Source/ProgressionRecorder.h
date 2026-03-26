#pragma once

#include "ProgressionModel.h"
#include "VoicingModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <set>

class ProgressionRecorder
{
public:
    ProgressionRecorder() = default;

    // Call from GUI thread to begin/end recording
    void startRecording (double bpm, double sampleRate);
    void stopRecording();
    bool isRecording() const { return recording; }

    // Call from processBlock — accumulates MIDI events with beat-relative timestamps
    void processBlock (const juce::MidiBuffer& midi, int numSamples);

    // After stopRecording(), retrieve the raw MIDI sequence (beat-timestamped)
    const juce::MidiMessageSequence& getRawMidi() const { return rawMidi; }

    // Analyze raw MIDI into chord changes
    static std::vector<ProgressionChord> analyzeChordChanges (
        const juce::MidiMessageSequence& midi,
        const VoicingLibrary* voicingLibrary);

    // Quantize chord list to a grid resolution (in beats)
    static std::vector<ProgressionChord> quantize (
        const std::vector<ProgressionChord>& chords,
        double resolution,
        double totalBeats);

    // Inject a MIDI event directly (e.g. for pre-held notes at beat 0)
    void injectEvent (const juce::MidiMessage& msg);

    // Get the total duration of the recording in beats
    double getTotalBeats() const { return totalRecordedBeats; }

private:
    std::atomic<bool> recording { false };
    double bpm_ = 120.0;
    double sampleRate_ = 44100.0;
    double samplePosition = 0.0;
    double totalRecordedBeats = 0.0;

    juce::MidiMessageSequence rawMidi;
};
