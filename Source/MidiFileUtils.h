#pragma once

#include "VoicingModel.h"
#include "ProgressionModel.h"
#include "MelodyModel.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace MidiFileUtils
{
    struct ImportResult
    {
        std::vector<juce::MidiMessageSequence> tracks;
        std::vector<juce::String> trackNames;
        double bpm = 120.0;
        int timeSigNum = 4;
        int timeSigDen = 4;
        bool success = false;
    };

    // Read a .mid file — returns tracks with beat-relative timestamps
    ImportResult importMidiFile (const juce::File& file);

    // Convert imported track to Chordy models
    Voicing     midiToVoicing (const juce::MidiMessageSequence& track);
    Progression midiToProgression (const juce::MidiMessageSequence& track,
                                    double bpm, int tsNum, int tsDen);
    Melody      midiToMelody (const juce::MidiMessageSequence& track,
                               double bpm, int tsNum, int tsDen);

    // Export Chordy models to .mid files
    bool exportVoicingToMidi (const Voicing& v, const juce::File& dest);
    bool exportProgressionToMidi (const Progression& p, const juce::File& dest);
    bool exportMelodyToMidi (const Melody& m, const juce::File& dest);
}
