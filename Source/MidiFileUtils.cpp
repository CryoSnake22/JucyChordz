#include "MidiFileUtils.h"
#include "ProgressionRecorder.h"
#include "ChordDetector.h"

static constexpr int kTicksPerQuarter = 480;

MidiFileUtils::ImportResult MidiFileUtils::importMidiFile (const juce::File& file)
{
    ImportResult result;

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
        return result;

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream))
        return result;

    // Detect BPM from tempo events
    juce::MidiMessageSequence tempoTrack;
    midiFile.findAllTempoEvents (tempoTrack);
    if (tempoTrack.getNumEvents() > 0)
    {
        auto& msg = tempoTrack.getEventPointer (0)->message;
        if (msg.isTempoMetaEvent())
            result.bpm = 60.0 / msg.getTempoSecondsPerQuarterNote();
    }

    // Detect time signature
    juce::MidiMessageSequence timeSigTrack;
    midiFile.findAllTimeSigEvents (timeSigTrack);
    if (timeSigTrack.getNumEvents() > 0)
    {
        int num = 4, den = 4;
        timeSigTrack.getEventPointer (0)->message.getTimeSignatureInfo (num, den);
        result.timeSigNum = num;
        result.timeSigDen = den;
    }

    // Convert ticks to seconds first
    midiFile.convertTimestampTicksToSeconds();

    // Extract tracks with note events, converting seconds to beats
    double beatsPerSecond = result.bpm / 60.0;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* srcTrack = midiFile.getTrack (t);
        if (srcTrack == nullptr)
            continue;

        // Check if this track has any note events
        bool hasNotes = false;
        juce::String trackName;
        for (int e = 0; e < srcTrack->getNumEvents(); ++e)
        {
            auto& msg = srcTrack->getEventPointer (e)->message;
            if (msg.isNoteOn())
                hasNotes = true;
            if (msg.isTrackNameEvent())
                trackName = msg.getTextFromTextMetaEvent();
        }

        if (! hasNotes)
            continue;

        // Convert to beat-relative timestamps
        juce::MidiMessageSequence beatTrack;
        for (int e = 0; e < srcTrack->getNumEvents(); ++e)
        {
            auto msg = srcTrack->getEventPointer (e)->message;
            double beatTime = msg.getTimeStamp() * beatsPerSecond;
            msg.setTimeStamp (beatTime);
            beatTrack.addEvent (msg);
        }
        beatTrack.updateMatchedPairs();

        result.tracks.push_back (std::move (beatTrack));
        result.trackNames.push_back (trackName.isEmpty()
            ? "Track " + juce::String (result.tracks.size())
            : trackName);
    }

    result.success = ! result.tracks.empty();
    return result;
}

Voicing MidiFileUtils::midiToVoicing (const juce::MidiMessageSequence& track)
{
    // Collect all notes that start within the first 0.1 beats (simultaneous chord)
    std::vector<int> notes;
    std::vector<int> velocities;
    double firstNoteTime = -1.0;

    for (int e = 0; e < track.getNumEvents(); ++e)
    {
        auto& msg = track.getEventPointer (e)->message;
        if (msg.isNoteOn() && msg.getVelocity() > 0)
        {
            if (firstNoteTime < 0.0)
                firstNoteTime = msg.getTimeStamp();

            if (msg.getTimeStamp() - firstNoteTime <= 0.1)
            {
                notes.push_back (msg.getNoteNumber());
                velocities.push_back (msg.getVelocity());
            }
            else
                break; // past the first chord
        }
    }

    return VoicingLibrary::createFromNotes (notes, "", velocities);
}

Progression MidiFileUtils::midiToProgression (const juce::MidiMessageSequence& track,
                                               double bpm, int tsNum, int tsDen)
{
    Progression p;
    p.id = juce::Uuid().toString();
    p.bpm = bpm;
    p.timeSignatureNum = tsNum;
    p.timeSignatureDen = tsDen;
    p.rawMidi = track;
    p.quantizeResolution = 0.0; // Raw by default

    // Calculate total beats from last event, round up to bar
    double lastBeat = 0.0;
    for (int e = 0; e < track.getNumEvents(); ++e)
    {
        double t = track.getEventPointer (e)->message.getTimeStamp();
        if (t > lastBeat) lastBeat = t;
    }
    double beatsPerBar = static_cast<double> (tsNum);
    p.totalBeats = std::ceil (lastBeat / beatsPerBar) * beatsPerBar;
    if (p.totalBeats < beatsPerBar) p.totalBeats = beatsPerBar;

    // Analyze chords using existing pipeline
    p.chords = ProgressionRecorder::analyzeChordChanges (track, nullptr);

    // Detect key from most common root pitch class
    std::array<int, 12> rootCounts {};
    for (const auto& c : p.chords)
        rootCounts[static_cast<size_t> (c.rootPitchClass)]++;
    p.keyPitchClass = static_cast<int> (
        std::distance (rootCounts.begin(),
                       std::max_element (rootCounts.begin(), rootCounts.end())));
    p.mode = "Major";

    return p;
}

Melody MidiFileUtils::midiToMelody (const juce::MidiMessageSequence& track,
                                     double bpm, int tsNum, int tsDen)
{
    Melody m;
    m.id = juce::Uuid().toString();
    m.bpm = bpm;
    m.timeSignatureNum = tsNum;
    m.timeSignatureDen = tsDen;
    m.rawMidi = track;
    m.quantizeResolution = 0.0;

    // Calculate total beats
    double lastBeat = 0.0;
    for (int e = 0; e < track.getNumEvents(); ++e)
    {
        double t = track.getEventPointer (e)->message.getTimeStamp();
        if (t > lastBeat) lastBeat = t;
    }
    double beatsPerBar = static_cast<double> (tsNum);
    m.totalBeats = std::ceil (lastBeat / beatsPerBar) * beatsPerBar;
    if (m.totalBeats < beatsPerBar) m.totalBeats = beatsPerBar;

    // Detect key from most common pitch class
    std::array<int, 12> pitchCounts {};
    for (int e = 0; e < track.getNumEvents(); ++e)
    {
        auto& msg = track.getEventPointer (e)->message;
        if (msg.isNoteOn() && msg.getVelocity() > 0)
            pitchCounts[static_cast<size_t> (msg.getNoteNumber() % 12)]++;
    }
    m.keyPitchClass = static_cast<int> (
        std::distance (pitchCounts.begin(),
                       std::max_element (pitchCounts.begin(), pitchCounts.end())));

    // Analyze notes using C4 (60) as reference, offset by detected key
    int keyRootMidi = 60 + m.keyPitchClass;
    if (keyRootMidi > 66) keyRootMidi -= 12;
    m.notes = MelodyLibrary::analyzeMelodyNotes (track, keyRootMidi);

    return m;
}

// --- Export ---

static juce::MidiMessageSequence beatsToTicks (const juce::MidiMessageSequence& beats)
{
    juce::MidiMessageSequence ticks;
    for (int e = 0; e < beats.getNumEvents(); ++e)
    {
        auto msg = beats.getEventPointer (e)->message;
        msg.setTimeStamp (msg.getTimeStamp() * kTicksPerQuarter);
        ticks.addEvent (msg);
    }
    ticks.updateMatchedPairs();
    return ticks;
}

static void addTempoEvent (juce::MidiMessageSequence& track, double bpm)
{
    double secondsPerBeat = 60.0 / bpm;
    int microsecondsPerBeat = juce::roundToInt (secondsPerBeat * 1000000.0);
    auto tempoMsg = juce::MidiMessage::tempoMetaEvent (microsecondsPerBeat);
    tempoMsg.setTimeStamp (0.0);
    track.addEvent (tempoMsg);
}

bool MidiFileUtils::exportVoicingToMidi (const Voicing& v, const juce::File& dest)
{
    if (v.intervals.empty()) return false;

    juce::MidiMessageSequence track;
    addTempoEvent (track, 120.0);

    // Build absolute MIDI notes from root + intervals
    auto notes = VoicingLibrary::transposeToKey (v, v.octaveReference);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        int vel = i < v.velocities.size() ? v.velocities[i] : 100;
        auto noteOn = juce::MidiMessage::noteOn (1, notes[i], static_cast<juce::uint8> (vel));
        noteOn.setTimeStamp (0.0);
        auto noteOff = juce::MidiMessage::noteOff (1, notes[i]);
        noteOff.setTimeStamp (2.0 * kTicksPerQuarter); // 2 beats
        track.addEvent (noteOn);
        track.addEvent (noteOff);
    }
    track.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (kTicksPerQuarter);
    midiFile.addTrack (track);

    dest.deleteFile();
    juce::FileOutputStream stream (dest);
    if (! stream.openedOk()) return false;
    return midiFile.writeTo (stream);
}

bool MidiFileUtils::exportProgressionToMidi (const Progression& p, const juce::File& dest)
{
    if (p.rawMidi.getNumEvents() == 0) return false;

    auto track = beatsToTicks (p.rawMidi);
    addTempoEvent (track, p.bpm);
    track.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (kTicksPerQuarter);
    midiFile.addTrack (track);

    dest.deleteFile();
    juce::FileOutputStream stream (dest);
    if (! stream.openedOk()) return false;
    return midiFile.writeTo (stream);
}

bool MidiFileUtils::exportMelodyToMidi (const Melody& m, const juce::File& dest)
{
    if (m.rawMidi.getNumEvents() == 0) return false;

    auto track = beatsToTicks (m.rawMidi);
    addTempoEvent (track, m.bpm);
    track.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (kTicksPerQuarter);
    midiFile.addTrack (track);

    dest.deleteFile();
    juce::FileOutputStream stream (dest);
    if (! stream.openedOk()) return false;
    return midiFile.writeTo (stream);
}
