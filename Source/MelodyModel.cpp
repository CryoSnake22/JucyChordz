#include "MelodyModel.h"
#include <algorithm>
#include <map>

//==============================================================================
// MelodyLibrary
//==============================================================================

void MelodyLibrary::addMelody (const Melody& m)
{
    melodies.push_back (m);
}

void MelodyLibrary::removeMelody (const juce::String& id)
{
    melodies.erase (
        std::remove_if (melodies.begin(), melodies.end(),
                        [&id] (const Melody& m) { return m.id == id; }),
        melodies.end());
}

const Melody* MelodyLibrary::getMelody (const juce::String& id) const
{
    for (const auto& m : melodies)
        if (m.id == id)
            return &m;
    return nullptr;
}

Melody MelodyLibrary::transposeMelody (const Melody& m, int semitones)
{
    Melody result = m;
    result.keyPitchClass = (m.keyPitchClass + semitones + 120) % 12;

    // Rebuild chord context display names
    for (auto& cc : result.chordContexts)
    {
        int rootPc = (result.keyPitchClass + cc.intervalFromKeyRoot + 120) % 12;
        juce::String rootName = ChordDetector::noteNameFromPitchClass (rootPc);
        juce::String qualLabel = ChordDetector::qualitySuffix (cc.quality);
        if (cc.alterations.isNotEmpty())
            qualLabel += cc.alterations;
        cc.name = rootName + qualLabel;
    }

    // Transpose rawMidi note events so playback matches the transposed key
    if (semitones != 0)
    {
        juce::MidiMessageSequence transposedMidi;
        for (int i = 0; i < result.rawMidi.getNumEvents(); ++i)
        {
            auto* evt = result.rawMidi.getEventPointer (i);
            auto msg = evt->message;
            if (msg.isNoteOnOrOff())
            {
                int newNote = juce::jlimit (0, 127, msg.getNoteNumber() + semitones);
                if (msg.isNoteOn())
                    msg = juce::MidiMessage::noteOn (msg.getChannel(), newNote, msg.getVelocity());
                else
                    msg = juce::MidiMessage::noteOff (msg.getChannel(), newNote, msg.getFloatVelocity());
                msg.setTimeStamp (evt->message.getTimeStamp());
            }
            transposedMidi.addEvent (msg);
        }
        result.rawMidi = transposedMidi;
    }

    return result;
}

//==============================================================================
// Analysis helpers
//==============================================================================

std::vector<MelodyNote> MelodyLibrary::analyzeMelodyNotes (
    const juce::MidiMessageSequence& midi,
    int keyRootMidiNote)
{
    std::vector<MelodyNote> result;

    // Track note-ons to match with note-offs
    // Key = MIDI note number, Value = (startBeat, velocity)
    std::map<int, std::pair<double, int>> activeNotes;

    for (int i = 0; i < midi.getNumEvents(); ++i)
    {
        auto* evt = midi.getEventPointer (i);
        auto& msg = evt->message;

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            activeNotes[msg.getNoteNumber()] = { msg.getTimeStamp(), msg.getVelocity() };
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            auto it = activeNotes.find (msg.getNoteNumber());
            if (it != activeNotes.end())
            {
                MelodyNote note;
                note.intervalFromKeyRoot = msg.getNoteNumber() - keyRootMidiNote;
                note.startBeat = it->second.first;
                note.durationBeats = std::max (0.05, msg.getTimeStamp() - it->second.first);
                note.velocity = it->second.second;
                result.push_back (note);
                activeNotes.erase (it);
            }
        }
    }

    // Sort by start beat
    std::sort (result.begin(), result.end(),
               [] (const MelodyNote& a, const MelodyNote& b) { return a.startBeat < b.startBeat; });

    return result;
}

std::vector<MelodyNote> MelodyLibrary::quantizeMelodyNotes (
    const std::vector<MelodyNote>& notes,
    double resolution)
{
    if (notes.empty() || resolution <= 0.0)
        return notes;

    std::vector<MelodyNote> result = notes;

    for (auto& note : result)
    {
        // Snap start beat to grid
        note.startBeat = std::round (note.startBeat / resolution) * resolution;

        // Snap duration to grid (minimum = resolution)
        note.durationBeats = std::max (resolution,
            std::round (note.durationBeats / resolution) * resolution);
    }

    // Remove duplicates at the same start position (keep the first)
    auto it = std::unique (result.begin(), result.end(),
                           [] (const MelodyNote& a, const MelodyNote& b)
                           { return std::abs (a.startBeat - b.startBeat) < 0.001
                                    && a.intervalFromKeyRoot == b.intervalFromKeyRoot; });
    result.erase (it, result.end());

    return result;
}

//==============================================================================
// Serialization
//==============================================================================

static const juce::Identifier ID_MelodyLibrary ("MelodyLibrary");
static const juce::Identifier ID_Melody ("Melody");
static const juce::Identifier ID_MelodyNote ("MelodyNote");
static const juce::Identifier ID_MelodyChordContext ("MelodyChordContext");

static const juce::Identifier ID_id ("id");
static const juce::Identifier ID_name ("name");
static const juce::Identifier ID_keyPitchClass ("keyPitchClass");
static const juce::Identifier ID_totalBeats ("totalBeats");
static const juce::Identifier ID_bpm ("bpm");
static const juce::Identifier ID_timeSigNum ("timeSigNum");
static const juce::Identifier ID_timeSigDen ("timeSigDen");
static const juce::Identifier ID_rawMidi ("rawMidi");

static const juce::Identifier ID_intervalFromKeyRoot ("intervalFromKeyRoot");
static const juce::Identifier ID_startBeat ("startBeat");
static const juce::Identifier ID_durationBeats ("durationBeats");
static const juce::Identifier ID_velocity ("velocity");
static const juce::Identifier ID_quality ("quality");
static const juce::Identifier ID_alterations ("alterations");

// Serialize MidiMessageSequence to compact string (same format as ProgressionModel)
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

juce::ValueTree MelodyLibrary::noteToValueTree (const MelodyNote& n)
{
    juce::ValueTree tree (ID_MelodyNote);
    tree.setProperty (ID_intervalFromKeyRoot, n.intervalFromKeyRoot, nullptr);
    tree.setProperty (ID_startBeat, n.startBeat, nullptr);
    tree.setProperty (ID_durationBeats, n.durationBeats, nullptr);
    tree.setProperty (ID_velocity, n.velocity, nullptr);
    return tree;
}

MelodyNote MelodyLibrary::noteFromValueTree (const juce::ValueTree& tree)
{
    MelodyNote n;
    n.intervalFromKeyRoot = tree.getProperty (ID_intervalFromKeyRoot, 0);
    n.startBeat = tree.getProperty (ID_startBeat, 0.0);
    n.durationBeats = tree.getProperty (ID_durationBeats, 0.5);
    n.velocity = tree.getProperty (ID_velocity, 100);
    return n;
}

juce::ValueTree MelodyLibrary::chordContextToValueTree (const MelodyChordContext& cc)
{
    juce::ValueTree tree (ID_MelodyChordContext);
    tree.setProperty (ID_intervalFromKeyRoot, cc.intervalFromKeyRoot, nullptr);
    tree.setProperty (ID_quality, static_cast<int> (cc.quality), nullptr);
    tree.setProperty (ID_alterations, cc.alterations, nullptr);
    tree.setProperty (ID_startBeat, cc.startBeat, nullptr);
    tree.setProperty (ID_durationBeats, cc.durationBeats, nullptr);
    tree.setProperty (ID_name, cc.name, nullptr);
    return tree;
}

MelodyChordContext MelodyLibrary::chordContextFromValueTree (const juce::ValueTree& tree)
{
    MelodyChordContext cc;
    cc.intervalFromKeyRoot = tree.getProperty (ID_intervalFromKeyRoot, 0);
    cc.quality = static_cast<ChordQuality> (static_cast<int> (tree.getProperty (ID_quality)));
    cc.alterations = tree.getProperty (ID_alterations).toString();
    cc.startBeat = tree.getProperty (ID_startBeat, 0.0);
    cc.durationBeats = tree.getProperty (ID_durationBeats, 4.0);
    cc.name = tree.getProperty (ID_name).toString();
    return cc;
}

juce::ValueTree MelodyLibrary::melodyToValueTree (const Melody& m)
{
    juce::ValueTree tree (ID_Melody);
    tree.setProperty (ID_id, m.id, nullptr);
    tree.setProperty (ID_name, m.name, nullptr);
    tree.setProperty (ID_keyPitchClass, m.keyPitchClass, nullptr);
    tree.setProperty (ID_totalBeats, m.totalBeats, nullptr);
    tree.setProperty (ID_bpm, m.bpm, nullptr);
    tree.setProperty (ID_timeSigNum, m.timeSignatureNum, nullptr);
    tree.setProperty (ID_timeSigDen, m.timeSignatureDen, nullptr);
    tree.setProperty (ID_rawMidi, midiSequenceToString (m.rawMidi), nullptr);

    for (const auto& cc : m.chordContexts)
        tree.appendChild (chordContextToValueTree (cc), nullptr);

    for (const auto& note : m.notes)
        tree.appendChild (noteToValueTree (note), nullptr);

    return tree;
}

Melody MelodyLibrary::melodyFromValueTree (const juce::ValueTree& tree)
{
    Melody m;
    m.id = tree.getProperty (ID_id).toString();
    m.name = tree.getProperty (ID_name).toString();
    m.keyPitchClass = tree.getProperty (ID_keyPitchClass, 0);
    m.totalBeats = tree.getProperty (ID_totalBeats, 0.0);
    m.bpm = tree.getProperty (ID_bpm, 120.0);
    m.timeSignatureNum = tree.getProperty (ID_timeSigNum, 4);
    m.timeSignatureDen = tree.getProperty (ID_timeSigDen, 4);
    m.rawMidi = stringToMidiSequence (tree.getProperty (ID_rawMidi).toString());

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (child.hasType (ID_MelodyChordContext))
            m.chordContexts.push_back (chordContextFromValueTree (child));
        else if (child.hasType (ID_MelodyNote))
            m.notes.push_back (noteFromValueTree (child));
    }

    return m;
}

juce::ValueTree MelodyLibrary::toValueTree() const
{
    juce::ValueTree tree (ID_MelodyLibrary);
    for (const auto& m : melodies)
        tree.appendChild (melodyToValueTree (m), nullptr);
    return tree;
}

void MelodyLibrary::fromValueTree (const juce::ValueTree& tree)
{
    melodies.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (child.hasType (ID_Melody))
            melodies.push_back (melodyFromValueTree (child));
    }
}
