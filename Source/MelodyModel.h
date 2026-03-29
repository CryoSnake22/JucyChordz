#pragma once

#include "ChordDetector.h"
#include "FolderModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

struct MelodyChordContext
{
    int intervalFromKeyRoot = 0;          // semitones from key root (e.g., ii=2, V=7, I=0)
    ChordQuality quality = ChordQuality::Unknown;
    juce::String alterations;
    double startBeat = 0.0;
    double durationBeats = 4.0;
    juce::String name;                    // display name, e.g. "Dm7"

    juce::String getDisplayName (int keyPitchClass) const
    {
        if (name.isNotEmpty())
            return name;
        int rootPc = (keyPitchClass + intervalFromKeyRoot + 120) % 12;
        juce::String rootName = ChordDetector::noteNameFromPitchClass (rootPc);
        juce::String qualLabel = ChordDetector::qualitySuffix (quality);
        if (alterations.isNotEmpty())
            qualLabel += alterations;
        return rootName + qualLabel;
    }
};

struct MelodyNote
{
    int intervalFromKeyRoot = 0;          // semitones from key root
    double startBeat = 0.0;
    double durationBeats = 0.5;
    int velocity = 100;
};

struct Melody
{
    juce::String id;
    juce::String name;
    int keyPitchClass = 0;                // 0-11, the key it was recorded in
    std::vector<MelodyChordContext> chordContexts;
    std::vector<MelodyNote> notes;
    double totalBeats = 0.0;
    double bpm = 120.0;
    int timeSignatureNum = 4;
    int timeSignatureDen = 4;
    juce::MidiMessageSequence rawMidi;
    double quantizeResolution = 0.0;  // 0=raw, 1.0=beat, 0.5=half, 0.25=quarter
    juce::String folderId;            // folder UUID (empty = root/unfiled)

    bool isValid() const { return ! id.isEmpty() && ! notes.empty(); }

    juce::String getChordContextLabel() const
    {
        if (chordContexts.empty())
            return "";
        juce::StringArray labels;
        for (const auto& cc : chordContexts)
        {
            auto lbl = const_cast<MelodyChordContext&> (cc).getDisplayName (keyPitchClass);
            labels.addIfNotAlreadyThere (lbl);
        }
        return labels.joinIntoString (" > ");
    }

    juce::String getDisplayName() const
    {
        auto ctx = getChordContextLabel();
        if (ctx.isEmpty())
            return name;
        return name + " (" + ctx + ")";
    }
};

class MelodyLibrary
{
public:
    MelodyLibrary() = default;

    void addMelody (const Melody& m);
    void removeMelody (const juce::String& id);
    const Melody* getMelody (const juce::String& id) const;
    const std::vector<Melody>& getAllMelodies() const { return melodies; }
    int size() const { return static_cast<int> (melodies.size()); }

    static Melody transposeMelody (const Melody& m, int semitones);

    // Folder management
    FolderLibrary& getFolders() { return folders; }
    const FolderLibrary& getFolders() const { return folders; }

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    // Post-processing helpers
    static std::vector<MelodyNote> analyzeMelodyNotes (
        const juce::MidiMessageSequence& midi,
        int keyRootMidiNote);

    static std::vector<MelodyNote> quantizeMelodyNotes (
        const std::vector<MelodyNote>& notes,
        double resolution);

private:
    std::vector<Melody> melodies;
    FolderLibrary folders;

    static juce::ValueTree noteToValueTree (const MelodyNote& n);
    static MelodyNote noteFromValueTree (const juce::ValueTree& tree);
    static juce::ValueTree chordContextToValueTree (const MelodyChordContext& cc);
    static MelodyChordContext chordContextFromValueTree (const juce::ValueTree& tree);
    static juce::ValueTree melodyToValueTree (const Melody& m);
    static Melody melodyFromValueTree (const juce::ValueTree& tree);
};
