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
// Chord roots use their own natural spelling based on common jazz usage,
// not forced by the key's sharp/flat preference.
// Pitch classes with enharmonic choices: prefer the most common jazz name.
juce::String pitchClassToLyChordRoot (int pc)
{
    pc = ((pc % 12) + 12) % 12;
    // For chord roots, use standard jazz naming:
    // C=0, Db=1, D=2, Eb=3, E=4, F=5, F#/Gb=6, G=7, Ab=8, A=9, Bb=10, B=11
    // Only pc=6 is ambiguous; use sharp if key uses sharps, flat otherwise
    if (pc == 6)
        return pitchClassToLy (pc, g_useSharps);

    // For all others, use the conventional name (flat for 1,3,8,10; natural for rest)
    static const char* names[] = {
        "c", "des", "d", "ees", "e", "f",
        "ges", "g", "aes", "a", "bes", "b"
    };
    return names[pc];
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

// Split MIDI notes into treble (>= splitPoint) and bass (< splitPoint) sets.
// splitPoint defaults to 60 (middle C) but should be adjusted for transpositions
// to keep bass/treble assignment consistent with the original key.
void splitTrebleBass (const std::vector<int>& midiNotes,
                      std::vector<int>& treble,
                      std::vector<int>& bass,
                      int splitPoint = 60)
{
    treble.clear();
    bass.clear();

    auto sorted = midiNotes;
    std::sort (sorted.begin(), sorted.end());

    for (int n : sorted)
    {
        if (n < splitPoint)
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
    ly += "  system-system-spacing.basic-distance = #20\n";
    ly += "  system-system-spacing.minimum-distance = #16\n";
    ly += "  system-system-spacing.padding = #4\n";
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

        // Split into treble/bass (adaptive split to preserve original staff assignment)
        std::vector<int> treble, bass;
        splitTrebleBass (midiNotes, treble, bass, 60 + semitones);

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

    // Helper: generate chord symbols for one key's chords
    auto generateChordSymbols = [&] (const std::vector<ProgressionChord>& chords) -> juce::String
    {
        juce::String out;
        double chordPos = 0.0;
        for (size_t ci = 0; ci < chords.size(); ++ci)
        {
            const auto& chord = chords[ci];
            double gap = chord.startBeat - chordPos;
            if (gap > 0.2)
            {
                auto gapTokens = decomposeDuration (gap, std::fmod (chordPos, beatsPerBar), beatsPerBar);
                for (const auto& tok : gapTokens)
                    out += "s" + tok.duration + " ";
            }

            auto durTokens = decomposeDuration (chord.durationBeats,
                                                 std::fmod (chord.startBeat, beatsPerBar),
                                                 beatsPerBar);
            for (size_t ti = 0; ti < durTokens.size(); ++ti)
            {
                if (ti == 0)
                {
                    out += pitchClassToLyChordRoot (chord.rootPitchClass);
                    out += durTokens[ti].duration;
                    out += qualityToLyChordMode (chord.quality);
                }
                else
                    out += "s" + durTokens[ti].duration;
                out += " ";
            }
            chordPos = chord.startBeat + chord.durationBeats;
        }
        return out;
    };

    // Helper: generate notes for one staff from events
    struct StaffEvent {
        double start;
        double duration;
        std::vector<int> notes;
        std::vector<int> graceNotes;  // MIDI notes to render as acciaccatura before this event
    };

    // Generate a single monophonic voice from non-overlapping events
    auto generateMonoVoice = [&] (const std::vector<StaffEvent>& events, double endBeat) -> juce::String
    {
        juce::String out;
        double pos = 0.0;

        for (const auto& ev : events)
        {
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

            // Render grace notes (acciaccatura) before the main chord.
            // Grace notes render correctly inside polyphonic voices when LilyPond
            // handles line breaks automatically (no forced \break at barlines).
            if (! ev.graceNotes.empty())
            {
                out += "\\acciaccatura { ";
                for (int gn : ev.graceNotes)
                    out += midiNoteToLyPitch (gn) + "8 ";
                out += "} ";
            }

            double dur = std::max (0.25, ev.duration);
            auto durTokens = decomposeDuration (dur, std::fmod (ev.start, beatsPerBar), beatsPerBar);
            for (size_t ti = 0; ti < durTokens.size(); ++ti)
            {
                out += midiNotesToLyChord (ev.notes, durTokens[ti].duration);
                if (durTokens[ti].tieAfter) out += "~ ";
                else out += " ";
            }
            pos = ev.start + dur;
        }

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

    // Generate staff notes, splitting into polyphonic voices when notes overlap
    auto generateStaffNotes = [&] (const std::vector<StaffEvent>& rawEvents, double endBeat) -> juce::String
    {
        if (rawEvents.empty())
        {
            int numBars = static_cast<int> (std::ceil (endBeat / beatsPerBar));
            juce::String out;
            for (int bar = 0; bar < numBars; ++bar)
                out += "R1 ";
            return out;
        }

        // Sort events by start time
        auto events = rawEvents;
        std::sort (events.begin(), events.end(), [] (const StaffEvent& a, const StaffEvent& b) {
            return a.start < b.start;
        });

        // Split into two voices using duration-based classification:
        // Voice 1 (stems up): held/sustained notes (longest at each start beat)
        // Voice 2 (stems down): moving/melodic notes (shorter notes + notes during held)
        std::vector<StaffEvent> voice1; // held / sustained
        std::vector<StaffEvent> voice2; // moving / melodic

        // Track active voice1 events to know when notes occur "during" a held note
        double voice1ActiveUntil = -1.0;

        size_t ei = 0;
        while (ei < events.size())
        {
            double groupStart = events[ei].start;

            // Collect all events at the same start beat
            std::vector<size_t> groupIndices;
            while (ei < events.size() && std::abs (events[ei].start - groupStart) < 0.05)
                groupIndices.push_back (ei++);

            if (groupIndices.size() == 1)
            {
                auto& ev = events[groupIndices[0]];
                bool duringHeld = (groupStart < voice1ActiveUntil - 0.05);

                if (duringHeld)
                {
                    // This note plays during a held note -> voice 2
                    voice2.push_back (ev);
                }
                else
                {
                    // Check if this note overlaps with future events
                    bool overlapsNext = false;
                    if (ei < events.size() && ev.start + ev.duration > events[ei].start + 0.1)
                        overlapsNext = true;

                    if (overlapsNext && ev.duration > 1.0)
                    {
                        // Long note that overlaps others -> held voice
                        voice1.push_back (ev);
                        voice1ActiveUntil = std::max (voice1ActiveUntil, ev.start + ev.duration);
                    }
                    else
                    {
                        voice2.push_back (ev);
                    }
                }
            }
            else
            {
                // Multiple events at same start beat -> compare durations
                // Find the longest
                size_t longestIdx = groupIndices[0];
                double longestDur = events[longestIdx].duration;
                double shortestDur = longestDur;
                for (size_t gi : groupIndices)
                {
                    if (events[gi].duration > longestDur)
                    {
                        longestDur = events[gi].duration;
                        longestIdx = gi;
                    }
                    if (events[gi].duration < shortestDur)
                        shortestDur = events[gi].duration;
                }

                bool duringHeld = (groupStart < voice1ActiveUntil - 0.05);
                bool significantlyLonger = (longestDur > shortestDur + 1.0) || (longestDur > shortestDur * 2.0);

                if (significantlyLonger && ! duringHeld)
                {
                    // The longest goes to voice 1, rest to voice 2
                    for (size_t gi : groupIndices)
                    {
                        if (gi == longestIdx)
                        {
                            voice1.push_back (events[gi]);
                            voice1ActiveUntil = std::max (voice1ActiveUntil, events[gi].start + events[gi].duration);
                        }
                        else
                        {
                            voice2.push_back (events[gi]);
                        }
                    }
                }
                else
                {
                    // All similar duration or during held -> all to voice 2
                    for (size_t gi : groupIndices)
                        voice2.push_back (events[gi]);
                }
            }
        }

        // If voice1 is empty, no polyphony needed
        if (voice1.empty())
            return generateMonoVoice (voice2, endBeat);
        if (voice2.empty())
            return generateMonoVoice (voice1, endBeat);

        // Grace notes render naturally inside their voice via \acciaccatura.
        // LilyPond handles grace notes in individual voices within << \\ >> correctly.
        juce::String out;
        out += "<< { \\voiceOne ";
        out += generateMonoVoice (voice1, endBeat);
        out += "} \\\\ { \\voiceTwo ";
        out += generateMonoVoice (voice2, endBeat);
        out += "} >> \\oneVoice ";
        return out;
    };

    // Pre-compute bars per key from quantized chord data
    // Use the actual chord span, not totalBeats (which may include trailing silence)
    auto sampleChords = quantizeChordsForExport (prog.chords, beatsPerBar);
    double actualEnd = 0.0;
    for (const auto& c : sampleChords)
        actualEnd = std::max (actualEnd, c.startBeat + c.durationBeats);
    // Build all keys' content into parallel string streams
    // (LilyPond auto-breaks at double barlines for optimal layout)
    juce::String allChordSymbols;
    juce::String allUpper;
    juce::String allLower;
    juce::String allSingle;  // for non-grand-staff

    for (size_t ki = 0; ki < opts.keys.size(); ++ki)
    {
        int targetPc = opts.keys[ki];
        g_useSharps = keyUsesSharps (targetPc);
        int semitones = targetPc - prog.keyPitchClass;

        auto transposed = (semitones == 0)
            ? prog
            : ProgressionLibrary::transposeProgression (prog, semitones);

        transposed.chords = quantizeChordsForExport (transposed.chords, beatsPerBar);

        // Use actual chord span for bar count (not totalBeats which may include trailing silence)
        double chordEnd = 0.0;
        for (const auto& c : transposed.chords)
            chordEnd = std::max (chordEnd, c.startBeat + c.durationBeats);
        double endBeat = std::ceil (chordEnd / beatsPerBar) * beatsPerBar;
        if (endBeat < beatsPerBar) endBeat = beatsPerBar;

        // Key label as rehearsal mark
        juce::String keyMark = "\\mark \\markup { \\bold \"Key of " + keyDisplayName (targetPc) + "\" } ";

        // Chord symbols
        if (opts.includeChordSymbols)
        {
            if (ki == 0)
                allChordSymbols += keyMark;
            else
                allChordSymbols += keyMark;

            allChordSymbols += generateChordSymbols (transposed.chords);
        }

        // Extract individual notes from raw MIDI for accurate per-note rendering
        auto rawNotes = ProgressionRecorder::extractNotes (transposed.rawMidi);

        // Adaptive split point: preserves original treble/bass assignment after transposition
        int splitPoint = 60 + semitones;

        // --- Grace note detection (before quantization) ---
        // Short notes (< 0.3 beats) followed by a longer note within 0.5 beats
        // on the same staff are appogiaturas. Store them keyed by target note index.
        // Map: target note index -> list of grace MIDI notes
        std::map<size_t, std::vector<int>> graceNotesForTarget;

        // Sort by start beat for scanning
        std::sort (rawNotes.begin(), rawNotes.end(), [] (const ProgressionNote& a, const ProgressionNote& b) {
            return a.startBeat < b.startBeat;
        });

        std::vector<bool> isGrace (rawNotes.size(), false);
        for (size_t i = 0; i < rawNotes.size(); ++i)
        {
            if (rawNotes[i].durationBeats >= 0.3)
                continue;  // not short enough for a grace note

            bool isTreble = rawNotes[i].midiNote >= splitPoint;

            // Find the closest-in-pitch longer note on the same staff within 0.5 beats
            size_t bestTarget = rawNotes.size(); // invalid
            int bestPitchDist = 999;

            for (size_t j = i + 1; j < rawNotes.size(); ++j)
            {
                if (rawNotes[j].startBeat - rawNotes[i].startBeat > 0.5)
                    break;  // too far away

                bool targetTreble = rawNotes[j].midiNote >= splitPoint;
                if (targetTreble != isTreble)
                    continue;  // different staff

                if (rawNotes[j].durationBeats > rawNotes[i].durationBeats
                    && rawNotes[i].durationBeats < rawNotes[j].durationBeats * 0.5)
                {
                    int dist = std::abs (rawNotes[j].midiNote - rawNotes[i].midiNote);
                    if (dist < bestPitchDist)
                    {
                        bestPitchDist = dist;
                        bestTarget = j;
                    }
                }
            }

            if (bestTarget < rawNotes.size())
            {
                isGrace[i] = true;
                graceNotesForTarget[bestTarget].push_back (rawNotes[i].midiNote);
            }
        }

        // Remove grace notes from main list, but remember target associations
        // Build a new note list without grace notes, and a map from old index to new index
        std::vector<ProgressionNote> filteredNotes;
        std::map<size_t, size_t> oldToNewIndex;
        for (size_t i = 0; i < rawNotes.size(); ++i)
        {
            if (! isGrace[i])
            {
                oldToNewIndex[i] = filteredNotes.size();
                filteredNotes.push_back (rawNotes[i]);
            }
        }

        // Remap grace notes to new indices
        std::map<size_t, std::vector<int>> remappedGraces;
        for (auto& [oldTarget, graces] : graceNotesForTarget)
        {
            auto it = oldToNewIndex.find (oldTarget);
            if (it != oldToNewIndex.end())
                remappedGraces[it->second] = graces;
        }

        rawNotes = filteredNotes;
        // --- End grace note detection ---

        // Build per-note events with pre-sort index for grace note lookup
        struct NoteWithDur { int midiNote; double start; double dur; size_t preSortIndex; };
        std::vector<NoteWithDur> allNoteEvents;
        for (size_t ri = 0; ri < rawNotes.size(); ++ri)
            allNoteEvents.push_back ({ rawNotes[ri].midiNote, rawNotes[ri].startBeat, rawNotes[ri].durationBeats, ri });

        // Quantize individual note timings to sixteenth grid
        for (auto& n : allNoteEvents)
        {
            n.start = snapBeat (n.start);
            n.dur = std::max (0.25, snapBeat (n.dur));
        }

        std::vector<StaffEvent> trebleEvents, bassEvents;

        // Sort by start, then duration, then pitch
        std::sort (allNoteEvents.begin(), allNoteEvents.end(), [] (const NoteWithDur& a, const NoteWithDur& b) {
            if (std::abs (a.start - b.start) > 0.01) return a.start < b.start;
            if (std::abs (a.dur - b.dur) > 0.3) return a.dur < b.dur;
            return a.midiNote < b.midiNote;
        });

        size_t ni = 0;
        while (ni < allNoteEvents.size())
        {
            double groupStart = allNoteEvents[ni].start;
            double groupDur = allNoteEvents[ni].dur;
            std::vector<int> trebleNotes, bassNotes;
            std::vector<int> trebleGraces, bassGraces;

            // Collect notes with same start AND similar duration (within 0.3 beats)
            while (ni < allNoteEvents.size()
                   && std::abs (allNoteEvents[ni].start - groupStart) < 0.01
                   && std::abs (allNoteEvents[ni].dur - groupDur) < 0.3)
            {
                // Collect any grace notes attached to this note
                auto graceIt = remappedGraces.find (allNoteEvents[ni].preSortIndex);

                if (allNoteEvents[ni].midiNote >= splitPoint)
                {
                    trebleNotes.push_back (allNoteEvents[ni].midiNote);
                    if (graceIt != remappedGraces.end())
                        for (int gn : graceIt->second)
                            trebleGraces.push_back (gn);
                }
                else
                {
                    bassNotes.push_back (allNoteEvents[ni].midiNote);
                    if (graceIt != remappedGraces.end())
                        for (int gn : graceIt->second)
                            bassGraces.push_back (gn);
                }
                ++ni;
            }

            if (! trebleNotes.empty())
                trebleEvents.push_back ({ groupStart, groupDur, trebleNotes, trebleGraces });
            if (! bassNotes.empty())
                bassEvents.push_back ({ groupStart, groupDur, bassNotes, bassGraces });
        }

        if (opts.grandStaff)
        {
            // Key mark goes in chord symbols (or upper staff if no chords).
            if (! opts.includeChordSymbols)
                allUpper += keyMark;
            // Wrap each key's content in { } braces so grace notes and polyphony
            // are scoped inside the key block (prevents grace notes leaking before barlines)
            allUpper += "{ " + generateStaffNotes (trebleEvents, endBeat) + "} ";
            allLower += "{ " + generateStaffNotes (bassEvents, endBeat) + "} ";
        }
        else
        {
            if (! opts.includeChordSymbols)
                allSingle += keyMark;
            allSingle += "{ " + generateStaffNotes (trebleEvents, endBeat) + "} ";
        }

        // Double barline between keys
        if (ki + 1 < opts.keys.size())
        {
            allChordSymbols += "\\bar \"||\" ";
            allUpper += "\\bar \"||\" ";
            allLower += "\\bar \"||\" ";
            allSingle += "\\bar \"||\" ";
        }

        // Let LilyPond auto-break at double barlines.
        // Forced \break conflicts with grace notes at system boundaries
        // (grace notes get pushed to a separate empty system).
    }

    // Build single score
    ly += "\\score {\n  <<\n";

    if (opts.includeChordSymbols)
    {
        ly += "    \\new ChordNames \\chordmode {\n";
        ly += "      \\set chordChanges = ##f\n";
        ly += "      " + allChordSymbols + "\n";
        ly += "    }\n";
    }

    if (opts.grandStaff)
    {
        ly += "    \\new PianoStaff <<\n";
        ly += "      \\new Staff = \"upper\" \\absolute {\n";
        ly += "        \\clef treble \\key c \\major\n";
        ly += "        \\numericTimeSignature \\time "
            + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
        ly += "        " + allUpper + "\n";
        ly += "      }\n";
        ly += "      \\new Staff = \"lower\" \\with { \\omit TimeSignature } \\absolute {\n";
        ly += "        \\clef bass \\key c \\major\n";
        ly += "        \\time " + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
        ly += "        " + allLower + "\n";
        ly += "      }\n";
        ly += "    >>\n";
    }
    else
    {
        ly += "    \\new Staff \\absolute {\n";
        ly += "      \\clef treble \\key c \\major \\time "
            + juce::String (beatsPerBar) + "/" + juce::String (timeSigDen) + "\n";
        ly += "      " + allSingle + "\n";
        ly += "    }\n";
    }

    ly += "  >>\n";
    ly += "  \\layout {\n";
    ly += "    indent = 0\n";
    ly += "    \\context {\n";
    ly += "      \\Score\n";
    ly += "      \\override RehearsalMark.self-alignment-X = #LEFT\n";
    ly += "      \\override RehearsalMark.font-size = #1\n";
    ly += "    }\n";
    ly += "  }\n";
    ly += "}\n";

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

    // Debug: also save to a known location for inspection
    juce::File ("/tmp/chordy_debug.ly").replaceWithText (lyContent);

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
