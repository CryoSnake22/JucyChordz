#include "LilyPondExporter.h"
#include "ChordDetector.h"
#include <algorithm>
#include <cmath>

namespace LilyPondExporter
{

// ============================================================================
// Internal helpers
// ============================================================================

namespace
{

// Whether a key signature uses sharps (true) or flats (false).
// Sharp keys: G, D, A, E, B, F#/Gb (we pick sharps for Gb)
// Flat keys: F, Bb, Eb, Ab, Db, C is neutral (flats by default)
bool keyUsesSharps (int keyPitchClass)
{
    // pitch classes that are sharp keys: G=7, D=2, A=9, E=4, B=11, F#=6
    switch (((keyPitchClass % 12) + 12) % 12)
    {
        case 2: case 4: case 6: case 7: case 9: case 11:
            return true;
        default:
            return false;
    }
}

// Pitch class -> LilyPond note name (no octave marks), enharmonic-aware
juce::String pitchClassToLy (int pc, bool useSharps)
{
    pc = ((pc % 12) + 12) % 12;

    if (useSharps)
    {
        static const char* names[] = {
            "c", "cis", "d", "dis", "e", "f",
            "fis", "g", "gis", "a", "ais", "b"
        };
        return names[pc];
    }
    else
    {
        static const char* names[] = {
            "c", "des", "d", "ees", "e", "f",
            "ges", "g", "aes", "a", "bes", "b"
        };
        return names[pc];
    }
}

// Thread-local context for current key's enharmonic preference
// (avoids passing useSharps through every function)
static thread_local bool g_useSharps = false;

// MIDI note number -> LilyPond absolute pitch (e.g., "c'", "bes,")
// In \absolute mode: c (no marks) = C3 = MIDI 48, c' = C4 = MIDI 60
juce::String midiNoteToLyPitch (int midiNote)
{
    int pc = ((midiNote % 12) + 12) % 12;
    int lyOctave = midiNote / 12 - 4;

    juce::String result = pitchClassToLy (pc, g_useSharps);

    if (lyOctave > 0)
        for (int i = 0; i < lyOctave; ++i)
            result += "'";
    else if (lyOctave < 0)
        for (int i = 0; i < -lyOctave; ++i)
            result += ",";

    return result;
}

// ChordQuality -> LilyPond \chordmode suffix
juce::String qualityToLyChordMode (ChordQuality q)
{
    switch (q)
    {
        case ChordQuality::Major:         return "";
        case ChordQuality::Minor:         return ":m";
        case ChordQuality::Diminished:    return ":dim";
        case ChordQuality::Augmented:     return ":aug";
        case ChordQuality::Maj6:          return ":6";
        case ChordQuality::Min6:          return ":m6";
        case ChordQuality::Dom7:          return ":7";
        case ChordQuality::Maj7:          return ":maj7";
        case ChordQuality::Min7:          return ":m7";
        case ChordQuality::MinMaj7:       return ":m7+";
        case ChordQuality::Dim7:          return ":dim7";
        case ChordQuality::HalfDim7:      return ":m7.5-";
        case ChordQuality::Dom7b5:        return ":7.5-";
        case ChordQuality::Dom7sharp5:    return ":7.5+";
        case ChordQuality::Dom7b9:        return ":7.9-";
        case ChordQuality::Dom7sharp9:    return ":7.9+";
        case ChordQuality::Dom9:          return ":9";
        case ChordQuality::Maj9:          return ":maj9";
        case ChordQuality::Min9:          return ":m9";
        case ChordQuality::MinMaj9:       return ":m9.7+";
        case ChordQuality::Dom11:         return ":11";
        case ChordQuality::Min11:         return ":m11";
        case ChordQuality::Maj7sharp11:   return ":maj7.11+";
        case ChordQuality::Dom13:         return ":13";
        case ChordQuality::Maj13:         return ":13.11";
        case ChordQuality::Min13:         return ":m13";
        case ChordQuality::Maj69:         return ":6.9";
        case ChordQuality::Min69:         return ":m6.9";
        case ChordQuality::Add9:          return ":5.9";
        case ChordQuality::MinAdd9:       return ":m5.9";
        case ChordQuality::Sus2:          return ":sus2";
        case ChordQuality::Sus4:          return ":sus4";
        case ChordQuality::Unknown:       return "";
    }
    return "";
}

// Pitch class -> LilyPond chordmode root (in octave below middle C for bass voicing)
juce::String pitchClassToLyChordRoot (int pc)
{
    return pitchClassToLy (pc, g_useSharps);
}

// Snap a beat duration to the nearest representable note value.
// Returns the snapped value.
double snapDuration (double beats)
{
    // Representable durations in descending order
    static const double grid[] = { 4.0, 3.0, 2.0, 1.5, 1.0, 0.75, 0.5, 0.375, 0.25 };
    double best = 0.25;
    double bestDist = 999.0;

    for (double g : grid)
    {
        double dist = std::abs (beats - g);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = g;
        }
    }
    return best;
}

// Snap a beat position to the nearest grid position (resolution = 0.25 beats = sixteenth note)
double snapBeat (double beat, double resolution = 0.25)
{
    return std::round (beat / resolution) * resolution;
}

// Quantize progression chords for clean notation export.
// Snaps start beats and durations to the nearest grid, removes micro-gaps.
std::vector<ProgressionChord> quantizeChordsForExport (const std::vector<ProgressionChord>& chords, int beatsPerBar)
{
    if (chords.empty()) return chords;

    std::vector<ProgressionChord> result;
    result.reserve (chords.size());

    for (size_t i = 0; i < chords.size(); ++i)
    {
        auto c = chords[i];

        // Snap start beat to nearest sixteenth
        c.startBeat = snapBeat (c.startBeat);

        // Snap duration: extend to meet the next chord or end of bar
        if (i + 1 < chords.size())
        {
            double nextStart = snapBeat (chords[i + 1].startBeat);
            c.durationBeats = nextStart - c.startBeat;
        }
        else
        {
            // Last chord: snap duration to fill to end of bar
            double snappedDur = snapBeat (c.durationBeats);
            if (snappedDur < 0.25) snappedDur = 0.25;
            double endBeat = c.startBeat + snappedDur;
            // Round up to bar boundary
            double barEnd = std::ceil (endBeat / beatsPerBar) * beatsPerBar;
            c.durationBeats = barEnd - c.startBeat;
        }

        if (c.durationBeats < 0.25) c.durationBeats = 0.25;
        result.push_back (c);
    }

    return result;
}

// Convert a beat duration to a LilyPond duration string.
// Only handles values that map to a single note (from the grid).
juce::String singleBeatsToDuration (double beats)
{
    if (std::abs (beats - 4.0)   < 0.05) return "1";
    if (std::abs (beats - 3.0)   < 0.05) return "2.";
    if (std::abs (beats - 2.0)   < 0.05) return "2";
    if (std::abs (beats - 1.5)   < 0.05) return "4.";
    if (std::abs (beats - 1.0)   < 0.05) return "4";
    if (std::abs (beats - 0.75)  < 0.05) return "8.";
    if (std::abs (beats - 0.5)   < 0.05) return "8";
    if (std::abs (beats - 0.375) < 0.05) return "16.";
    if (std::abs (beats - 0.25)  < 0.05) return "16";
    return "4"; // fallback
}

// Decompose a beat duration into a sequence of tied note durations,
// respecting barline boundaries.
// Returns pairs of (duration_string, needs_tie_after).
struct DurationToken
{
    juce::String duration;
    bool tieAfter = false;
};

std::vector<DurationToken> decomposeDuration (double totalBeats, double posInBar, int beatsPerBar)
{
    std::vector<DurationToken> tokens;
    double remaining = totalBeats;
    double pos = posInBar;

    // Representable single-note durations (descending)
    static const double grid[] = { 4.0, 3.0, 2.0, 1.5, 1.0, 0.75, 0.5, 0.375, 0.25 };

    while (remaining > 0.05)
    {
        double beatsUntilBarline = static_cast<double> (beatsPerBar) - pos;
        if (beatsUntilBarline < 0.05)
        {
            beatsUntilBarline = static_cast<double> (beatsPerBar);
            pos = 0.0;
        }

        double maxChunk = std::min (remaining, beatsUntilBarline);

        // Find largest grid value that fits
        double bestFit = 0.25;
        for (double g : grid)
        {
            if (g <= maxChunk + 0.05)
            {
                bestFit = g;
                break;
            }
        }

        // Clamp to minimum
        if (bestFit < 0.25)
            bestFit = 0.25;

        DurationToken tok;
        tok.duration = singleBeatsToDuration (bestFit);
        tok.tieAfter = (remaining - bestFit > 0.05);
        tokens.push_back (tok);

        remaining -= bestFit;
        pos += bestFit;
        if (pos >= static_cast<double> (beatsPerBar) - 0.01)
            pos = 0.0;
    }

    return tokens;
}

// Build LilyPond chord notation from a set of MIDI notes + duration
juce::String midiNotesToLyChord (const std::vector<int>& notes, const juce::String& duration)
{
    if (notes.empty())
        return "r" + duration;

    if (notes.size() == 1)
        return midiNoteToLyPitch (notes[0]) + duration;

    juce::String result = "<";
    for (size_t i = 0; i < notes.size(); ++i)
    {
        if (i > 0) result += " ";
        result += midiNoteToLyPitch (notes[i]);
    }
    result += ">" + duration;
    return result;
}

// Split MIDI notes into treble (>= 60) and bass (< 60) sets.
// Ensures bass has at least the lowest note if all are >= 60.
void splitTrebleBass (const std::vector<int>& midiNotes,
                      std::vector<int>& treble,
                      std::vector<int>& bass)
{
    treble.clear();
    bass.clear();

    auto sorted = midiNotes;
    std::sort (sorted.begin(), sorted.end());

    for (int n : sorted)
    {
        if (n < 60)
            bass.push_back (n);
        else
            treble.push_back (n);
    }

    // Don't force notes to the other staff -- if all notes are in one register,
    // keep them there and the other staff gets a rest.
}

// Generate the LilyPond header block
juce::String generateHeader (const ExportOptions& opts)
{
    juce::String ly;
    ly += "\\version \"2.24.0\"\n\n";

    ly += "\\header {\n";
    if (opts.title.isNotEmpty())
        ly += "  title = \"" + opts.title + "\"\n";
    if (opts.composer.isNotEmpty())
        ly += "  composer = \"" + opts.composer + "\"\n";
    ly += "  tagline = ##f\n";
    ly += "}\n\n";

    ly += "\\paper {\n";
    ly += "  #(set-paper-size \"" + opts.paperSize + "\")\n";
    ly += "  indent = 0\n";
    ly += "}\n\n";

    return ly;
}

// Note name for display (e.g., "C", "F#"), enharmonic-aware
juce::String keyDisplayName (int pitchClass)
{
    if (keyUsesSharps (pitchClass))
    {
        static const char* names[] = {
            "C", "C#", "D", "D#", "E", "F",
            "F#", "G", "G#", "A", "A#", "B"
        };
        int pc = ((pitchClass % 12) + 12) % 12;
        return names[pc];
    }
    return ChordDetector::noteNameFromPitchClass (pitchClass);
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

juce::File findLilyPondBinary()
{
    // 1. Check bundled location (future auto-download)
    auto appSupport = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("Chordy")
                          .getChildFile ("lilypond")
                          .getChildFile ("bin")
                          .getChildFile ("lilypond");
    if (appSupport.existsAsFile())
        return appSupport;

    // 2. Check PATH via 'which'
    juce::ChildProcess whichProc;
    if (whichProc.start ("which lilypond"))
    {
        whichProc.waitForProcessToFinish (5000);
        auto path = whichProc.readAllProcessOutput().trim();
        if (path.isNotEmpty())
        {
            juce::File f (path);
            if (f.existsAsFile())
                return f;
        }
    }

    // 3. Common macOS locations
    static const char* commonPaths[] = {
        "/opt/homebrew/bin/lilypond",
        "/usr/local/bin/lilypond",
        "/Applications/LilyPond.app/Contents/Resources/bin/lilypond"
    };

    for (const auto* p : commonPaths)
    {
        juce::File f (p);
        if (f.existsAsFile())
            return f;
    }

    return {};
}

juce::String generateVoicingLy (const Voicing& v, const ExportOptions& opts)
{
    juce::String ly = generateHeader (opts);

    // Build chord names and notes for each key
    // 1 voicing per bar (whole note), 4 bars per line
    juce::String chordNames;
    juce::String upperNotes;
    juce::String lowerNotes;

    for (size_t ki = 0; ki < opts.keys.size(); ++ki)
    {
        int targetPc = opts.keys[ki];
        g_useSharps = keyUsesSharps (targetPc);
        int semitones = targetPc - v.rootPitchClass;

        // Compute the root MIDI note in the same octave region as original
        int rootMidi = v.octaveReference + semitones;
        // Keep within reasonable range (don't drift too far)
        while (rootMidi < v.octaveReference - 6) rootMidi += 12;
        while (rootMidi > v.octaveReference + 6) rootMidi -= 12;

        auto midiNotes = VoicingLibrary::transposeToKey (v, rootMidi);

        // Chord name in chordmode (whole note = 1 bar)
        chordNames += pitchClassToLyChordRoot (targetPc);
        chordNames += "1";
        chordNames += qualityToLyChordMode (v.quality);
        chordNames += " ";

        // Split into treble/bass
        std::vector<int> treble, bass;
        splitTrebleBass (midiNotes, treble, bass);

        upperNotes += midiNotesToLyChord (treble, "1") + " ";
        lowerNotes += midiNotesToLyChord (bass, "1") + " ";

        // Line break every 4 bars
        if ((ki + 1) % 4 == 0 && ki + 1 < opts.keys.size())
        {
            chordNames += "\\break\n    ";
            upperNotes += "\\break\n    ";
            lowerNotes += "\\break\n    ";
        }
    }

    ly += "\\score {\n  <<\n";

    if (opts.includeChordSymbols)
    {
        ly += "    \\new ChordNames \\chordmode {\n";
        ly += "      \\set chordChanges = ##f\n";
        ly += "      " + chordNames + "\n";
        ly += "    }\n";
    }

    if (opts.grandStaff)
    {
        ly += "    \\new PianoStaff <<\n";
        ly += "      \\new Staff = \"upper\" \\absolute {\n";
        ly += "        \\clef treble \\key c \\major \\time 4/4\n";
        ly += "        " + upperNotes + "\n";
        ly += "      }\n";
        ly += "      \\new Staff = \"lower\" \\absolute {\n";
        ly += "        \\clef bass \\key c \\major \\time 4/4\n";
        ly += "        " + lowerNotes + "\n";
        ly += "      }\n";
        ly += "    >>\n";
    }
    else
    {
        ly += "    \\new Staff \\absolute {\n";
        ly += "      \\clef treble \\key c \\major \\time 4/4\n";
        ly += "      " + upperNotes + "\n";
        ly += "    }\n";
    }

    ly += "  >>\n";
    ly += "  \\layout {\n";
    ly += "    indent = 0\n";
    ly += "  }\n";
    ly += "}\n";

    return ly;
}

juce::String generateProgressionLy (const Progression& prog, const ExportOptions& opts)
{
    juce::String ly = generateHeader (opts);

    int beatsPerBar = prog.timeSignatureNum;
    int timeSigDen = prog.timeSignatureDen;

    for (size_t ki = 0; ki < opts.keys.size(); ++ki)
    {
        int targetPc = opts.keys[ki];
        g_useSharps = keyUsesSharps (targetPc);
        int semitones = targetPc - prog.keyPitchClass;

        auto transposed = (semitones == 0)
            ? prog
            : ProgressionLibrary::transposeProgression (prog, semitones);

        // Quantize chord timing for clean notation
        transposed.chords = quantizeChordsForExport (transposed.chords, beatsPerBar);

        // Score with key label in header (prevents orphan labels at page breaks)
        ly += "\\score {\n  <<\n";

        // Chord symbols
        if (opts.includeChordSymbols)
        {
            ly += "    \\new ChordNames \\chordmode {\n";
            ly += "      \\set chordChanges = ##f\n      ";

            double chordPos = 0.0;
            for (size_t ci = 0; ci < transposed.chords.size(); ++ci)
            {
                const auto& chord = transposed.chords[ci];

                // Snap small gaps (< 0.2 beats) by skipping the spacer
                double gap = chord.startBeat - chordPos;
                if (gap > 0.2)
                {
                    auto gapTokens = decomposeDuration (gap, std::fmod (chordPos, beatsPerBar), beatsPerBar);
                    for (const auto& tok : gapTokens)
                        ly += "s" + tok.duration + " ";
                }

                // Compute effective duration (extend to cover small gap to next chord)
                double effectiveDur = chord.durationBeats;
                if (ci + 1 < transposed.chords.size())
                {
                    double nextStart = transposed.chords[ci + 1].startBeat;
                    double microGap = nextStart - (chord.startBeat + effectiveDur);
                    if (microGap > 0.0 && microGap < 0.2)
                        effectiveDur += microGap;
                }

                auto durTokens = decomposeDuration (effectiveDur,
                                                     std::fmod (chord.startBeat, beatsPerBar),
                                                     beatsPerBar);

                for (size_t ti = 0; ti < durTokens.size(); ++ti)
                {
                    if (ti == 0)
                    {
                        ly += pitchClassToLyChordRoot (chord.rootPitchClass);
                        ly += durTokens[ti].duration;
                        ly += qualityToLyChordMode (chord.quality);
                    }
                    else
                    {
                        ly += "s" + durTokens[ti].duration;
                    }
                    if (durTokens[ti].tieAfter)
                        ly += "~ ";
                    else
                        ly += " ";
                }

                chordPos = chord.startBeat + effectiveDur;
            }

            ly += "\n    }\n";
        }

        // Notes -- build each staff independently
        if (opts.grandStaff)
        {
            // First pass: collect events per staff with gap snapping
            struct StaffEvent { double start; double duration; std::vector<int> notes; };
            std::vector<StaffEvent> trebleEvents, bassEvents;

            for (size_t ci = 0; ci < transposed.chords.size(); ++ci)
            {
                const auto& chord = transposed.chords[ci];
                std::vector<int> treble, bass;
                splitTrebleBass (chord.midiNotes, treble, bass);

                double start = chord.startBeat;
                double dur = chord.durationBeats;

                // Snap small gaps: if gap to next chord < 0.2 beats, extend this chord
                if (ci + 1 < transposed.chords.size())
                {
                    double nextStart = transposed.chords[ci + 1].startBeat;
                    double gap = nextStart - (start + dur);
                    if (gap > 0.0 && gap < 0.2)
                        dur += gap;
                }

                if (! treble.empty())
                    trebleEvents.push_back ({ start, dur, treble });
                if (! bass.empty())
                    bassEvents.push_back ({ start, dur, bass });
            }

            // Helper lambda: generate notes for one staff from its events
            double totalBars = std::ceil (transposed.totalBeats / beatsPerBar);
            double endBeat = totalBars * beatsPerBar;

            auto generateStaff = [&] (const std::vector<StaffEvent>& events) -> juce::String
            {
                juce::String out;
                double pos = 0.0;

                if (events.empty())
                {
                    // Entire staff is empty -- fill with whole-bar rests
                    for (int bar = 0; bar < static_cast<int> (totalBars); ++bar)
                        out += "R1 ";
                    return out;
                }

                for (const auto& ev : events)
                {
                    // Rest before this event
                    double gap = ev.start - pos;
                    if (gap > 0.1)
                    {
                        auto gapTokens = decomposeDuration (gap, std::fmod (pos, beatsPerBar), beatsPerBar);
                        for (const auto& tok : gapTokens)
                        {
                            out += "r" + tok.duration;
                            if (tok.tieAfter) out += "~ ";
                            else out += " ";
                        }
                    }

                    auto durTokens = decomposeDuration (ev.duration,
                                                         std::fmod (ev.start, beatsPerBar),
                                                         beatsPerBar);
                    for (size_t ti = 0; ti < durTokens.size(); ++ti)
                    {
                        out += midiNotesToLyChord (ev.notes, durTokens[ti].duration);
                        if (durTokens[ti].tieAfter) out += "~ ";
                        else out += " ";
                    }

                    pos = ev.start + ev.duration;
                }

                // Trailing rest
                if (endBeat - pos > 0.1)
                {
                    auto gapTokens = decomposeDuration (endBeat - pos, std::fmod (pos, beatsPerBar), beatsPerBar);
                    for (const auto& tok : gapTokens)
                    {
                        out += "r" + tok.duration;
                        if (tok.tieAfter) out += "~ ";
                        else out += " ";
                    }
                }

                return out;
            };

            juce::String upperNotes = generateStaff (trebleEvents);
            juce::String lowerNotes = generateStaff (bassEvents);

            ly += "    \\new PianoStaff <<\n";

            ly += "      \\new Staff = \"upper\" \\absolute {\n";
            ly += "        \\clef treble \\key c \\major \\time "
                + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
            ly += "        " + upperNotes + "\n";
            ly += "      }\n";

            ly += "      \\new Staff = \"lower\" \\absolute {\n";
            ly += "        \\clef bass \\key c \\major \\time "
                + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
            ly += "        " + lowerNotes + "\n";
            ly += "      }\n";

            ly += "    >>\n";
        }
        else
        {
            // Single staff (all notes together) with gap snapping
            juce::String notes;
            double notePos = 0.0;

            for (size_t ci = 0; ci < transposed.chords.size(); ++ci)
            {
                const auto& chord = transposed.chords[ci];

                double gap = chord.startBeat - notePos;
                if (gap > 0.2)
                {
                    auto gapTokens = decomposeDuration (gap, std::fmod (notePos, beatsPerBar), beatsPerBar);
                    for (const auto& tok : gapTokens)
                    {
                        notes += "r" + tok.duration;
                        if (tok.tieAfter) notes += "~ ";
                        else notes += " ";
                    }
                }

                double effectiveDur = chord.durationBeats;
                if (ci + 1 < transposed.chords.size())
                {
                    double nextStart = transposed.chords[ci + 1].startBeat;
                    double microGap = nextStart - (chord.startBeat + effectiveDur);
                    if (microGap > 0.0 && microGap < 0.2)
                        effectiveDur += microGap;
                }

                auto durTokens = decomposeDuration (effectiveDur,
                                                     std::fmod (chord.startBeat, beatsPerBar),
                                                     beatsPerBar);

                for (size_t ti = 0; ti < durTokens.size(); ++ti)
                {
                    notes += midiNotesToLyChord (chord.midiNotes, durTokens[ti].duration);
                    if (durTokens[ti].tieAfter) notes += "~ ";
                    else notes += " ";
                }

                notePos = chord.startBeat + effectiveDur;
            }

            ly += "    \\new Staff \\absolute {\n";
            ly += "      \\clef treble \\key c \\major \\time "
                + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
            ly += "      " + notes + "\n";
            ly += "    }\n";
        }

        ly += "  >>\n";
        ly += "  \\header { piece = \\markup { \\bold \\large \"Key of " + keyDisplayName (targetPc) + "\" } }\n";
        ly += "  \\layout { indent = 0 }\n";
        ly += "}\n\n";
    }

    return ly;
}

juce::String generateMelodyLy (const Melody& mel, const ExportOptions& opts)
{
    juce::String ly = generateHeader (opts);

    int beatsPerBar = mel.timeSignatureNum;
    int timeSigDen = mel.timeSignatureDen;

    for (size_t ki = 0; ki < opts.keys.size(); ++ki)
    {
        int targetPc = opts.keys[ki];
        g_useSharps = keyUsesSharps (targetPc);
        int semitones = targetPc - mel.keyPitchClass;

        auto transposed = (semitones == 0)
            ? mel
            : MelodyLibrary::transposeMelody (mel, semitones);

        // Compute base MIDI note for the key root (C4 region)
        int keyRootMidi = 60 + (transposed.keyPitchClass % 12);
        if (keyRootMidi > 66) keyRootMidi -= 12;  // keep in C4-F#4 range

        // Score with key label in header (prevents orphan labels at page breaks)
        ly += "\\score {\n  <<\n";

        // Chord symbols from chord contexts
        if (opts.includeChordSymbols && ! transposed.chordContexts.empty())
        {
            ly += "    \\new ChordNames \\chordmode {\n";
            ly += "      \\set chordChanges = ##f\n      ";

            double chordPos = 0.0;
            for (const auto& cc : transposed.chordContexts)
            {
                double gap = cc.startBeat - chordPos;
                if (gap > 0.1)
                {
                    auto gapTokens = decomposeDuration (gap, std::fmod (chordPos, beatsPerBar), beatsPerBar);
                    for (const auto& tok : gapTokens)
                        ly += "s" + tok.duration + " ";
                }

                int chordRootPc = (transposed.keyPitchClass + cc.intervalFromKeyRoot + 120) % 12;
                auto durTokens = decomposeDuration (cc.durationBeats,
                                                     std::fmod (cc.startBeat, beatsPerBar),
                                                     beatsPerBar);

                for (size_t ti = 0; ti < durTokens.size(); ++ti)
                {
                    if (ti == 0)
                    {
                        ly += pitchClassToLyChordRoot (chordRootPc);
                        ly += durTokens[ti].duration;
                        ly += qualityToLyChordMode (cc.quality);
                    }
                    else
                    {
                        ly += "s" + durTokens[ti].duration;
                    }
                    ly += " ";
                }

                chordPos = cc.startBeat + cc.durationBeats;
            }

            ly += "\n    }\n";
        }

        // Melody notes
        juce::String notes;
        double notePos = 0.0;

        for (const auto& note : transposed.notes)
        {
            int midiNote = keyRootMidi + note.intervalFromKeyRoot;

            double gap = note.startBeat - notePos;
            if (gap > 0.1)
            {
                auto gapTokens = decomposeDuration (gap, std::fmod (notePos, beatsPerBar), beatsPerBar);
                for (const auto& tok : gapTokens)
                {
                    notes += "r" + tok.duration;
                    if (tok.tieAfter) notes += "~ ";
                    else notes += " ";
                }
            }

            auto durTokens = decomposeDuration (note.durationBeats,
                                                 std::fmod (note.startBeat, beatsPerBar),
                                                 beatsPerBar);

            for (size_t ti = 0; ti < durTokens.size(); ++ti)
            {
                notes += midiNoteToLyPitch (midiNote) + durTokens[ti].duration;
                if (durTokens[ti].tieAfter) notes += "~ ";
                else notes += " ";
            }

            notePos = note.startBeat + note.durationBeats;
        }

        // Trailing rest to fill final bar
        double totalBars = std::ceil (transposed.totalBeats / beatsPerBar);
        double endBeat = totalBars * beatsPerBar;
        if (endBeat - notePos > 0.1)
        {
            auto gapTokens = decomposeDuration (endBeat - notePos, std::fmod (notePos, beatsPerBar), beatsPerBar);
            for (const auto& tok : gapTokens)
            {
                notes += "r" + tok.duration;
                if (tok.tieAfter) notes += "~ ";
                else notes += " ";
            }
        }

        ly += "    \\new Staff \\absolute {\n";
        ly += "      \\clef treble \\key c \\major \\time "
            + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
        ly += "      " + notes + "\n";
        ly += "    }\n";

        ly += "  >>\n";
        ly += "  \\header { piece = \\markup { \\bold \\large \"Key of " + keyDisplayName (targetPc) + "\" } }\n";
        ly += "  \\layout { indent = 0 }\n";
        ly += "}\n\n";
    }

    return ly;
}

ExportResult renderToPdf (const juce::String& lyContent, const juce::File& outputPdf)
{
    ExportResult result;

    auto lilypondBin = findLilyPondBinary();
    if (! lilypondBin.existsAsFile())
    {
        result.errorMessage = "LilyPond not found. Install via: brew install lilypond";
        return result;
    }

    // Write .ly to temp file
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto lyFile = tempDir.getChildFile ("chordy_export_" + juce::String (juce::Random::getSystemRandom().nextInt (99999)) + ".ly");
    lyFile.replaceWithText (lyContent);

    // Output base name (lilypond appends .pdf)
    auto outputBase = outputPdf.getParentDirectory()
                          .getChildFile (outputPdf.getFileNameWithoutExtension());

    // Run lilypond via shell so PATH includes Homebrew bin dirs
    // (GUI apps don't inherit the user's shell PATH, so gs/ghostscript may not be found)
    juce::ChildProcess proc;
    juce::String cmd = "export PATH=\"/opt/homebrew/bin:/usr/local/bin:$PATH\" && "
                     + lilypondBin.getFullPathName().quoted()
                     + " -dno-point-and-click"
                     + " --output=" + outputBase.getFullPathName().quoted()
                     + " " + lyFile.getFullPathName().quoted();

    juce::StringArray shellArgs;
    shellArgs.add ("/bin/sh");
    shellArgs.add ("-c");
    shellArgs.add (cmd);

    if (proc.start (shellArgs))
    {
        bool finished = proc.waitForProcessToFinish (30000);
        auto output = proc.readAllProcessOutput();
        auto exitCode = proc.getExitCode();

        if (! finished)
        {
            result.errorMessage = "LilyPond timed out (30s). Try a simpler export.";
            proc.kill();
        }
        else if (exitCode != 0)
        {
            result.errorMessage = "LilyPond error (exit code " + juce::String (exitCode) + "):\n" + output;
        }
        else if (outputPdf.existsAsFile())
        {
            result.success = true;
            result.pdfFile = outputPdf;
        }
        else
        {
            result.errorMessage = "LilyPond completed but PDF not found at: " + outputPdf.getFullPathName();
        }
    }
    else
    {
        result.errorMessage = "Failed to launch LilyPond process.";
    }

    // Clean up temp .ly file
    lyFile.deleteFile();

    return result;
}

} // namespace LilyPondExporter
