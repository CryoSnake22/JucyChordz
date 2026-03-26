#include "ProgressionModel.h"
#include <algorithm>

//==============================================================================
// ProgressionLibrary
//==============================================================================

void ProgressionLibrary::addProgression (const Progression& p)
{
    progressions.push_back (p);
}

void ProgressionLibrary::removeProgression (const juce::String& id)
{
    progressions.erase (
        std::remove_if (progressions.begin(), progressions.end(),
                        [&id] (const Progression& p) { return p.id == id; }),
        progressions.end());
}

const Progression* ProgressionLibrary::getProgression (const juce::String& id) const
{
    for (const auto& p : progressions)
        if (p.id == id)
            return &p;
    return nullptr;
}

Progression ProgressionLibrary::transposeProgression (const Progression& p, int semitones)
{
    Progression result = p;
    result.keyPitchClass = (p.keyPitchClass + semitones + 12) % 12;

    for (auto& chord : result.chords)
    {
        chord.rootPitchClass = (chord.rootPitchClass + semitones + 12) % 12;

        // Shift MIDI notes (preserves voice leading exactly)
        for (auto& note : chord.midiNotes)
            note += semitones;

        // Rebuild display name from transposed root + quality
        juce::String rootName = ChordDetector::noteNameFromPitchClass (chord.rootPitchClass);
        juce::String qualLabel = ChordDetector::qualitySuffix (chord.quality);
        if (chord.alterations.isNotEmpty())
            qualLabel += chord.alterations;
        chord.name = rootName + qualLabel;

        // Add slash bass note if lowest note differs from root (inversion)
        if (! chord.midiNotes.empty())
        {
            int bassPitchClass = chord.midiNotes[0] % 12;
            if (bassPitchClass < 0) bassPitchClass += 12;
            if (bassPitchClass != chord.rootPitchClass)
                chord.name += "/" + ChordDetector::noteNameFromPitchClass (bassPitchClass);
        }
    }

    return result;
}

//==============================================================================
// Serialization
//==============================================================================

static const juce::Identifier ID_ProgressionLibrary ("ProgressionLibrary");
static const juce::Identifier ID_Progression ("Progression");
static const juce::Identifier ID_ProgressionChord ("ProgressionChord");

static const juce::Identifier ID_id ("id");
static const juce::Identifier ID_name ("name");
static const juce::Identifier ID_keyPitchClass ("keyPitchClass");
static const juce::Identifier ID_mode ("mode");
static const juce::Identifier ID_totalBeats ("totalBeats");
static const juce::Identifier ID_bpm ("bpm");
static const juce::Identifier ID_timeSigNum ("timeSigNum");
static const juce::Identifier ID_timeSigDen ("timeSigDen");

static const juce::Identifier ID_intervals ("intervals");
static const juce::Identifier ID_rootPitchClass ("rootPitchClass");
static const juce::Identifier ID_quality ("quality");
static const juce::Identifier ID_alterations ("alterations");
static const juce::Identifier ID_chordName ("chordName");
static const juce::Identifier ID_linkedVoicingId ("linkedVoicingId");
static const juce::Identifier ID_startBeat ("startBeat");
static const juce::Identifier ID_durationBeats ("durationBeats");
static const juce::Identifier ID_midiNotes ("midiNotes");
static const juce::Identifier ID_rawMidi ("rawMidi");

static juce::String intsToString (const std::vector<int>& v)
{
    juce::StringArray parts;
    for (int i : v)
        parts.add (juce::String (i));
    return parts.joinIntoString (",");
}

static std::vector<int> stringToInts (const juce::String& s)
{
    std::vector<int> result;
    auto parts = juce::StringArray::fromTokens (s, ",", "");
    for (const auto& part : parts)
    {
        auto trimmed = part.trim();
        if (trimmed.isNotEmpty())
            result.push_back (trimmed.getIntValue());
    }
    return result;
}

// Serialize MidiMessageSequence to a compact string: "time:byte1:byte2:byte3;..."
static juce::String midiSequenceToString (const juce::MidiMessageSequence& seq)
{
    juce::StringArray events;
    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* evt = seq.getEventPointer (i);
        auto& msg = evt->message;
        juce::String entry = juce::String (msg.getTimeStamp(), 6);
        auto* data = msg.getRawData();
        for (int b = 0; b < msg.getRawDataSize(); ++b)
            entry += ":" + juce::String (data[b]);
        events.add (entry);
    }
    return events.joinIntoString (";");
}

static juce::MidiMessageSequence stringToMidiSequence (const juce::String& s)
{
    juce::MidiMessageSequence seq;
    if (s.isEmpty())
        return seq;

    auto events = juce::StringArray::fromTokens (s, ";", "");
    for (const auto& entry : events)
    {
        auto parts = juce::StringArray::fromTokens (entry, ":", "");
        if (parts.size() < 2)
            continue;

        double timestamp = parts[0].getDoubleValue();
        juce::uint8 bytes[8];
        int numBytes = juce::jmin (parts.size() - 1, 8);
        for (int b = 0; b < numBytes; ++b)
            bytes[b] = static_cast<juce::uint8> (parts[b + 1].getIntValue());

        seq.addEvent (juce::MidiMessage (bytes, numBytes, timestamp));
    }
    seq.updateMatchedPairs();
    return seq;
}

//==============================================================================

juce::ValueTree ProgressionLibrary::chordToValueTree (const ProgressionChord& c)
{
    juce::ValueTree tree (ID_ProgressionChord);
    tree.setProperty (ID_intervals, intsToString (c.intervals), nullptr);
    tree.setProperty (ID_rootPitchClass, c.rootPitchClass, nullptr);
    tree.setProperty (ID_quality, static_cast<int> (c.quality), nullptr);
    tree.setProperty (ID_alterations, c.alterations, nullptr);
    tree.setProperty (ID_chordName, c.name, nullptr);
    tree.setProperty (ID_linkedVoicingId, c.linkedVoicingId, nullptr);
    tree.setProperty (ID_startBeat, c.startBeat, nullptr);
    tree.setProperty (ID_durationBeats, c.durationBeats, nullptr);
    tree.setProperty (ID_midiNotes, intsToString (c.midiNotes), nullptr);
    return tree;
}

ProgressionChord ProgressionLibrary::chordFromValueTree (const juce::ValueTree& tree)
{
    ProgressionChord c;
    c.intervals = stringToInts (tree.getProperty (ID_intervals).toString());
    c.rootPitchClass = tree.getProperty (ID_rootPitchClass, 0);
    c.quality = static_cast<ChordQuality> (static_cast<int> (tree.getProperty (ID_quality)));
    c.alterations = tree.getProperty (ID_alterations).toString();
    c.name = tree.getProperty (ID_chordName).toString();
    c.linkedVoicingId = tree.getProperty (ID_linkedVoicingId).toString();
    c.startBeat = tree.getProperty (ID_startBeat, 0.0);
    c.durationBeats = tree.getProperty (ID_durationBeats, 4.0);
    c.midiNotes = stringToInts (tree.getProperty (ID_midiNotes).toString());
    return c;
}

juce::ValueTree ProgressionLibrary::progressionToValueTree (const Progression& p)
{
    juce::ValueTree tree (ID_Progression);
    tree.setProperty (ID_id, p.id, nullptr);
    tree.setProperty (ID_name, p.name, nullptr);
    tree.setProperty (ID_keyPitchClass, p.keyPitchClass, nullptr);
    tree.setProperty (ID_mode, p.mode, nullptr);
    tree.setProperty (ID_totalBeats, p.totalBeats, nullptr);
    tree.setProperty (ID_bpm, p.bpm, nullptr);
    tree.setProperty (ID_timeSigNum, p.timeSignatureNum, nullptr);
    tree.setProperty (ID_timeSigDen, p.timeSignatureDen, nullptr);
    tree.setProperty (ID_rawMidi, midiSequenceToString (p.rawMidi), nullptr);

    for (const auto& chord : p.chords)
        tree.appendChild (chordToValueTree (chord), nullptr);

    return tree;
}

Progression ProgressionLibrary::progressionFromValueTree (const juce::ValueTree& tree)
{
    Progression p;
    p.id = tree.getProperty (ID_id).toString();
    p.name = tree.getProperty (ID_name).toString();
    p.keyPitchClass = tree.getProperty (ID_keyPitchClass, 0);
    p.mode = tree.getProperty (ID_mode).toString();
    p.totalBeats = tree.getProperty (ID_totalBeats, 0.0);
    p.bpm = tree.getProperty (ID_bpm, 120.0);
    p.timeSignatureNum = tree.getProperty (ID_timeSigNum, 4);
    p.timeSignatureDen = tree.getProperty (ID_timeSigDen, 4);
    p.rawMidi = stringToMidiSequence (tree.getProperty (ID_rawMidi).toString());

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (child.hasType (ID_ProgressionChord))
            p.chords.push_back (chordFromValueTree (child));
    }

    return p;
}

juce::ValueTree ProgressionLibrary::toValueTree() const
{
    juce::ValueTree tree (ID_ProgressionLibrary);
    for (const auto& p : progressions)
        tree.appendChild (progressionToValueTree (p), nullptr);
    return tree;
}

void ProgressionLibrary::fromValueTree (const juce::ValueTree& tree)
{
    progressions.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (child.hasType (ID_Progression))
            progressions.push_back (progressionFromValueTree (child));
    }
}
