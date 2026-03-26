#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <set>

enum class ChordQuality
{
    Major,
    Minor,
    Diminished,
    Augmented,
    Maj6,
    Min6,
    Dom7,
    Maj7,
    Min7,
    MinMaj7,
    Dim7,
    HalfDim7,
    Dom7b5,
    Dom7sharp5,
    Dom7b9,
    Dom7sharp9,
    Dom9,
    Maj9,
    Min9,
    MinMaj9,
    Dom11,
    Min11,
    Maj7sharp11,
    Dom13,
    Maj13,
    Min13,
    Maj69,
    Min69,
    Add9,
    MinAdd9,
    Sus2,
    Sus4,
    Unknown
};

struct ChordResult
{
    int rootPitchClass = -1;        // 0-11 (C=0, C#=1, ..., B=11), -1 if none
    ChordQuality quality = ChordQuality::Unknown;
    int bassNote = -1;              // lowest MIDI note number
    juce::String displayName;       // e.g. "Cmaj7", "Eb/G"

    bool isValid() const { return rootPitchClass >= 0 && quality != ChordQuality::Unknown; }
};

class ChordDetector
{
public:
    static ChordResult detect (const std::vector<int>& midiNotes);

    static juce::String noteNameFromPitchClass (int pitchClass);
    static juce::String qualitySuffix (ChordQuality q);

    // Returns the interval template for a chord quality (e.g., Dom7 -> {0,4,7,10})
    static std::vector<int> getChordTones (ChordQuality q);

private:
    struct ChordTemplate
    {
        std::set<int> intervals;
        ChordQuality quality;
    };

    static const std::vector<ChordTemplate>& getTemplates();
};
