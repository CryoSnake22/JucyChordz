#include "ProgressionRecorder.h"
#include "ChordDetector.h"
#include <algorithm>
#include <cmath>

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

        // Only record note-on and note-off events
        if (! msg.isNoteOnOrOff())
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
// Chord Change Detection
//==============================================================================

std::vector<ProgressionChord> ProgressionRecorder::analyzeChordChanges (
    const juce::MidiMessageSequence& midi,
    const VoicingLibrary* voicingLibrary)
{
    std::vector<ProgressionChord> chords;

    if (midi.getNumEvents() == 0)
        return chords;

    // Walk through events tracking which notes are currently held
    std::set<int> activeNotes;
    std::set<int> lastChordNotes;
    double chordStartBeat = 0.0;

    for (int i = 0; i < midi.getNumEvents(); ++i)
    {
        auto* evt = midi.getEventPointer (i);
        auto& msg = evt->message;
        double beatTime = msg.getTimeStamp();

        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            activeNotes.insert (msg.getNoteNumber());
        }
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
            activeNotes.erase (msg.getNoteNumber());
        }

        // Detect chord change: active note set differs from last chord
        // Only trigger when we have notes and they've changed
        if (! activeNotes.empty() && activeNotes != lastChordNotes)
        {
            // If we had a previous chord, close it out
            if (! lastChordNotes.empty() && ! chords.empty())
            {
                chords.back().durationBeats = beatTime - chordStartBeat;
            }

            // Build the new chord
            std::vector<int> noteVec (activeNotes.begin(), activeNotes.end());
            std::sort (noteVec.begin(), noteVec.end());

            ProgressionChord chord;
            chord.midiNotes = noteVec;
            chord.startBeat = beatTime;
            chord.durationBeats = 0.0; // will be set when next chord starts or at end

            // Compute intervals from lowest note
            int bassNote = noteVec[0];
            chord.rootPitchClass = bassNote % 12;
            for (int note : noteVec)
                chord.intervals.push_back (note - bassNote);

            // Try voicing library match first
            if (voicingLibrary != nullptr)
            {
                juce::String displayName;
                auto* match = voicingLibrary->findByNotes (noteVec, displayName);
                if (match != nullptr)
                {
                    chord.linkedVoicingId = match->id;
                    chord.name = match->name;
                    chord.quality = match->quality;
                    chord.alterations = match->alterations;
                    chord.rootPitchClass = match->rootPitchClass;
                }
            }

            // Fall back to ChordDetector
            if (chord.name.isEmpty())
            {
                auto result = ChordDetector::detect (noteVec);
                if (result.isValid())
                {
                    chord.rootPitchClass = result.rootPitchClass;
                    chord.quality = result.quality;
                    chord.name = result.displayName;
                }
                else
                {
                    chord.name = ChordDetector::noteNameFromPitchClass (chord.rootPitchClass) + "?";
                }
            }

            chords.push_back (chord);
            lastChordNotes = activeNotes;
            chordStartBeat = beatTime;
        }

        // When all notes release, mark the end but don't start a new chord
        if (activeNotes.empty() && ! lastChordNotes.empty())
        {
            if (! chords.empty())
                chords.back().durationBeats = beatTime - chordStartBeat;

            lastChordNotes.clear();
        }
    }

    // Close the last chord if it wasn't closed
    if (! chords.empty() && chords.back().durationBeats <= 0.0)
    {
        double endBeat = midi.getEndTime();
        chords.back().durationBeats = endBeat - chords.back().startBeat;
        if (chords.back().durationBeats <= 0.0)
            chords.back().durationBeats = 1.0;
    }

    return chords;
}

//==============================================================================
// Quantization
//==============================================================================

std::vector<ProgressionChord> ProgressionRecorder::quantize (
    const std::vector<ProgressionChord>& chords,
    double resolution,
    double totalBeats)
{
    if (chords.empty() || resolution <= 0.0)
        return chords;

    std::vector<ProgressionChord> quantized = chords;

    // Snap each chord's startBeat to the nearest grid point
    for (auto& chord : quantized)
    {
        chord.startBeat = std::round (chord.startBeat / resolution) * resolution;
    }

    // Remove duplicate positions (keep the last chord at each position)
    for (size_t i = quantized.size() - 1; i > 0; --i)
    {
        if (std::abs (quantized[i - 1].startBeat - quantized[i].startBeat) < 0.001)
            quantized.erase (quantized.begin() + static_cast<std::ptrdiff_t> (i - 1));
    }

    // Recalculate durations to fill gaps
    for (size_t i = 0; i + 1 < quantized.size(); ++i)
    {
        quantized[i].durationBeats = quantized[i + 1].startBeat - quantized[i].startBeat;
    }

    // Last chord fills to total duration (rounded to grid)
    if (! quantized.empty())
    {
        double endBeat = std::ceil (totalBeats / resolution) * resolution;
        if (endBeat <= quantized.back().startBeat)
            endBeat = quantized.back().startBeat + resolution;
        quantized.back().durationBeats = endBeat - quantized.back().startBeat;
    }

    // Ensure minimum duration
    for (auto& chord : quantized)
    {
        if (chord.durationBeats < resolution)
            chord.durationBeats = resolution;
    }

    return quantized;
}
