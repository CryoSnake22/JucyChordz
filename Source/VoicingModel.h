#pragma once

#include "ChordDetector.h"
#include "FolderModel.h"
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

struct Voicing
{
    juce::String id;
    juce::String name;
    ChordQuality quality = ChordQuality::Unknown;
    juce::String alterations;    // user-defined extensions like "#9#11b5"
    int rootPitchClass = 0;      // 0-11 (C=0), user-confirmed root
    std::vector<int> intervals;   // semitones from root, always starts with 0
    std::vector<int> velocities;  // per-note velocities (parallel to intervals)
    int octaveReference = 60;     // MIDI note of root when recorded
    juce::String folderId;         // folder UUID (empty = root/unfiled)
    juce::int64 createdAt = 0;     // unix timestamp (ms) when created

    bool isValid() const { return ! intervals.empty() && ! id.isEmpty(); }

    // Build a display label: "So What voicing" or quality+alterations like "m7#11"
    juce::String getQualityLabel() const
    {
        juce::String label = ChordDetector::qualitySuffix (quality);
        if (alterations.isNotEmpty())
            label += alterations;
        return label.isEmpty() ? "N/A" : label;
    }
};

class VoicingLibrary
{
public:
    VoicingLibrary() = default;

    // Library operations
    void addVoicing (const Voicing& v);
    void removeVoicing (const juce::String& id);
    const Voicing* getVoicing (const juce::String& id) const;
    const std::vector<Voicing>& getAllVoicings() const { return voicings; }
    std::vector<Voicing> getVoicingsByQuality (ChordQuality q) const;
    int size() const { return static_cast<int> (voicings.size()); }

    // Create a voicing from raw MIDI notes (auto-detects quality)
    static Voicing createFromNotes (const std::vector<int>& midiNotes,
                                    const juce::String& name,
                                    const std::vector<int>& noteVelocities = {});

    // Transpose a voicing to a specific root note, returning absolute MIDI notes
    static std::vector<int> transposeToKey (const Voicing& v, int rootMidiNote);

    // Apply inversion: move the N lowest notes up an octave (result sorted)
    static std::vector<int> applyInversion (const std::vector<int>& notes, int inversion);

    // Apply drop: Nth voice from top goes down an octave (result sorted)
    static std::vector<int> applyDrop (const std::vector<int>& notes, int dropN);

    // Match played MIDI notes against the library by interval pattern.
    // Returns the matching voicing and the detected root note name, or nullptr if no match.
    const Voicing* findByNotes (const std::vector<int>& midiNotes, juce::String& outDisplayName) const;

    // Folder management
    FolderLibrary& getFolders() { return folders; }
    const FolderLibrary& getFolders() const { return folders; }

    // Serialization
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    std::vector<Voicing> voicings;
    FolderLibrary folders;

    static juce::ValueTree voicingToValueTree (const Voicing& v);
    static Voicing voicingFromValueTree (const juce::ValueTree& tree);
};
