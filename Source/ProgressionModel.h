#pragma once

#include "ChordDetector.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

struct ProgressionChord
{
    std::vector<int> intervals;       // semitones from root (self-contained)
    int rootPitchClass = 0;           // 0-11
    ChordQuality quality = ChordQuality::Unknown;
    juce::String alterations;
    juce::String name;                // display name (e.g. "Cm7")
    juce::String linkedVoicingId;     // optional ref to VoicingLibrary (empty = unlinked)
    double startBeat = 0.0;           // position in beats from progression start
    double durationBeats = 4.0;       // duration in beats
    std::vector<int> midiNotes;       // original MIDI notes (absolute, for keyboard display)

    juce::String getDisplayName() const
    {
        if (name.isNotEmpty())
            return name;
        juce::String rootName = ChordDetector::noteNameFromPitchClass (rootPitchClass);
        juce::String qualLabel = ChordDetector::qualitySuffix (quality);
        if (alterations.isNotEmpty())
            qualLabel += alterations;
        return rootName + qualLabel;
    }
};

struct Progression
{
    juce::String id;
    juce::String name;
    int keyPitchClass = 0;            // 0-11, original key
    juce::String mode;                // "Major", "Minor", "Dorian", etc.
    std::vector<ProgressionChord> chords;
    double totalBeats = 0.0;
    double bpm = 120.0;
    int timeSignatureNum = 4;
    int timeSignatureDen = 4;
    juce::MidiMessageSequence rawMidi; // original recording (beat-timestamped)

    bool isValid() const { return ! id.isEmpty() && ! chords.empty(); }
};

class ProgressionLibrary
{
public:
    ProgressionLibrary() = default;

    void addProgression (const Progression& p);
    void removeProgression (const juce::String& id);
    const Progression* getProgression (const juce::String& id) const;
    const std::vector<Progression>& getAllProgressions() const { return progressions; }
    int size() const { return static_cast<int> (progressions.size()); }

    // Transpose a progression's chords by a semitone offset, returning a new Progression
    static Progression transposeProgression (const Progression& p, int semitones);

    // Serialization
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    std::vector<Progression> progressions;

    static juce::ValueTree chordToValueTree (const ProgressionChord& c);
    static ProgressionChord chordFromValueTree (const juce::ValueTree& tree);
    static juce::ValueTree progressionToValueTree (const Progression& p);
    static Progression progressionFromValueTree (const juce::ValueTree& tree);
};
