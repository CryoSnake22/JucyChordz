#include "ProgressionRecorder.h"
#include "ChordDetector.h"
#include <algorithm>
#include <cmath>
#include <map>

//==============================================================================
// Recording
//==============================================================================

void ProgressionRecorder::startRecording (double bpm, double sampleRate)
{
    rawMidi.clear();
    samplePosition = 0.0;
    totalRecordedBeats = 0.0;
    bpm_ = bpm;
    sampleRate_ = sampleRate;
    recording = true;
}

void ProgressionRecorder::stopRecording()
{
    if (! recording)
        return;

    recording = false;
    totalRecordedBeats = samplePosition / (sampleRate_ * 60.0 / bpm_);
    rawMidi.updateMatchedPairs();
}

void ProgressionRecorder::injectEvent (const juce::MidiMessage& msg)
{
    rawMidi.addEvent (msg);
}

void ProgressionRecorder::processBlock (const juce::MidiBuffer& midi, int numSamples)
{
    if (! recording)
        return;

    double samplesPerBeat = sampleRate_ * 60.0 / bpm_;

    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();

        // Record note-on, note-off, and sustain pedal (CC64) events
        if (! msg.isNoteOnOrOff() && ! msg.isSustainPedalOn() && ! msg.isSustainPedalOff())
            continue;

        // Compute beat-relative timestamp
        double sampleOffset = samplePosition + meta.samplePosition;
        double beatTime = sampleOffset / samplesPerBeat;

        auto timedMsg = msg;
        timedMsg.setTimeStamp (beatTime);
        rawMidi.addEvent (timedMsg);
    }

    samplePosition += numSamples;
}

//==============================================================================
// Note Extraction (Phase 1 — like analyzeMelodyNotes)
//==============================================================================

std::vector<ProgressionNote> ProgressionRecorder::extractNotes (
    const juce::MidiMessageSequence& midi)
{
    std::vector<ProgressionNote> result;

    // Track note-ons to match with note-offs
    std::map<int, std::pair<double, int>> activeNotes; // note -> (startBeat, velocity)

    for (int i = 0; i < midi.getNumEvents(); ++i)
    {
        auto* evt = midi.getEventPointer (i);
        auto& msg = evt->message;

        if (msg.isController())
            continue;

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            activeNotes[msg.getNoteNumber()] = { msg.getTimeStamp(), msg.getVelocity() };
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            auto it = activeNotes.find (msg.getNoteNumber());
            if (it != activeNotes.end())
            {
                ProgressionNote note;
                note.midiNote = msg.getNoteNumber();
                note.startBeat = it->second.first;
                note.durationBeats = std::max (0.05, msg.getTimeStamp() - it->second.first);
                note.velocity = it->second.second;
                result.push_back (note);
                activeNotes.erase (it);
            }
        }
    }

    // Sort by start beat, then by MIDI note number
    std::sort (result.begin(), result.end(),
               [] (const ProgressionNote& a, const ProgressionNote& b)
               {
                   if (std::abs (a.startBeat - b.startBeat) > 0.001)
                       return a.startBeat < b.startBeat;
                   return a.midiNote < b.midiNote;
               });

    return result;
}

//==============================================================================
// Chord Grouping (Phase 2 — group overlapping notes into chords)
//==============================================================================

std::vector<ProgressionChord> ProgressionRecorder::groupNotesIntoChords (
    const std::vector<ProgressionNote>& notes,
    const VoicingLibrary* voicingLibrary)
{
    std::vector<ProgressionChord> chords;
    if (notes.empty())
        return chords;

    // No automatic chord grouping — put all notes in a single group.
    // Users manually add chord boundaries in edit mode if desired.
    // Per-note timing is preserved for practice scoring.
    struct ChordGroup
    {
        std::vector<size_t> noteIndices;
        double earliestStart = 1e9;
        double latestEnd = 0.0;
    };

    ChordGroup singleGroup;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        singleGroup.noteIndices.push_back (i);
        singleGroup.earliestStart = std::min (singleGroup.earliestStart, notes[i].startBeat);
        singleGroup.latestEnd = std::max (singleGroup.latestEnd,
            notes[i].startBeat + notes[i].durationBeats);
    }

    std::vector<ChordGroup> groups;
    if (! singleGroup.noteIndices.empty())
        groups.push_back (singleGroup);

    // Convert each group into a ProgressionChord
    for (const auto& group : groups)
    {
        ProgressionChord chord;
        chord.startBeat = group.earliestStart;
        chord.durationBeats = group.latestEnd - group.earliestStart;
        if (chord.durationBeats <= 0.0)
            chord.durationBeats = 0.5;

        // Collect notes into parallel arrays
        struct NoteEntry { int midi; int vel; double start; double dur; };
        std::vector<NoteEntry> entries;
        for (size_t idx : group.noteIndices)
        {
            const auto& n = notes[idx];
            entries.push_back ({ n.midiNote, n.velocity, n.startBeat, n.durationBeats });
        }

        // Sort by MIDI note number
        std::sort (entries.begin(), entries.end(),
                   [] (const NoteEntry& a, const NoteEntry& b) { return a.midi < b.midi; });

        for (const auto& e : entries)
        {
            chord.midiNotes.push_back (e.midi);
            chord.midiVelocities.push_back (e.vel);
            chord.noteStartBeats.push_back (e.start);
            chord.noteDurations.push_back (e.dur);
        }

        // Compute intervals from bass note
        if (! chord.midiNotes.empty())
        {
            int bassNote = chord.midiNotes[0];
            chord.rootPitchClass = bassNote % 12;
            for (int note : chord.midiNotes)
                chord.intervals.push_back (note - bassNote);
        }

        // No automatic chord name detection — leave name empty by default.
        // Users can manually label chords in edit mode.

        chords.push_back (chord);
    }

    return chords;
}

//==============================================================================
// Chord Change Detection (uses extractNotes + groupNotesIntoChords)
//==============================================================================

std::vector<ProgressionChord> ProgressionRecorder::analyzeChordChanges (
    const juce::MidiMessageSequence& midi,
    const VoicingLibrary* voicingLibrary)
{
    auto notes = extractNotes (midi);
    return groupNotesIntoChords (notes, voicingLibrary);
}

//==============================================================================
// Quantization
//==============================================================================

std::vector<ProgressionChord> ProgressionRecorder::quantize (
    const std::vector<ProgressionChord>& chords,
    double resolution,
    double /*totalBeats*/)
{
    if (chords.empty() || resolution <= 0.0)
        return chords;

    std::vector<ProgressionChord> quantized = chords;

    // Snap per-note timing to grid, then recompute chord-level timing
    for (auto& chord : quantized)
    {
        for (size_t i = 0; i < chord.noteStartBeats.size(); ++i)
        {
            chord.noteStartBeats[i] = std::round (chord.noteStartBeats[i] / resolution) * resolution;
            chord.noteDurations[i] = std::max (resolution,
                std::round (chord.noteDurations[i] / resolution) * resolution);
        }

        // Recompute chord-level timing from per-note data
        if (! chord.noteStartBeats.empty())
        {
            double minStart = *std::min_element (chord.noteStartBeats.begin(), chord.noteStartBeats.end());
            double maxEnd = 0.0;
            for (size_t i = 0; i < chord.noteStartBeats.size(); ++i)
                maxEnd = std::max (maxEnd, chord.noteStartBeats[i] + chord.noteDurations[i]);
            chord.startBeat = minStart;
            chord.durationBeats = maxEnd - minStart;
        }
        else
        {
            chord.startBeat = std::round (chord.startBeat / resolution) * resolution;
        }
    }

    // Remove duplicate positions (keep the last chord at each position)
    for (size_t i = quantized.size() - 1; i > 0; --i)
    {
        if (std::abs (quantized[i - 1].startBeat - quantized[i].startBeat) < 0.001)
            quantized.erase (quantized.begin() + static_cast<std::ptrdiff_t> (i - 1));
    }

    // Ensure minimum chord-level duration
    for (auto& chord : quantized)
    {
        if (chord.durationBeats < resolution)
            chord.durationBeats = resolution;
    }

    return quantized;
}

//==============================================================================
// MIDI Quantization (snap event timestamps to grid)
//==============================================================================

juce::MidiMessageSequence ProgressionRecorder::quantizeMidi (
    const juce::MidiMessageSequence& rawMidi,
    double resolution)
{
    if (rawMidi.getNumEvents() == 0 || resolution <= 0.0)
        return rawMidi;

    juce::MidiMessageSequence quantized;

    for (int i = 0; i < rawMidi.getNumEvents(); ++i)
    {
        auto* evt = rawMidi.getEventPointer (i);
        auto msg = evt->message;
        double snapped = std::round (msg.getTimeStamp() / resolution) * resolution;
        if (snapped < 0.0) snapped = 0.0;
        msg.setTimeStamp (snapped);
        quantized.addEvent (msg);
    }

    quantized.sort();

    // Ensure note-off never precedes its paired note-on (enforce minimum duration)
    quantized.updateMatchedPairs();
    for (int i = 0; i < quantized.getNumEvents(); ++i)
    {
        auto* evt = quantized.getEventPointer (i);
        if (evt->message.isNoteOn() && evt->noteOffObject != nullptr)
        {
            double onTime = evt->message.getTimeStamp();
            double offTime = evt->noteOffObject->message.getTimeStamp();
            if (offTime <= onTime)
            {
                evt->noteOffObject->message.setTimeStamp (onTime + resolution);
            }
        }
    }

    quantized.sort();
    quantized.updateMatchedPairs();
    return quantized;
}
