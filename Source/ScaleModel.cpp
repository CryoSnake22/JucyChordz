#include "ScaleModel.h"
#include <algorithm>

std::vector<int> ScaleModel::getScaleIntervals (ScaleType type)
{
    switch (type)
    {
        case ScaleType::Major:            return { 0, 2, 4, 5, 7, 9, 11 };
        case ScaleType::NaturalMinor:     return { 0, 2, 3, 5, 7, 8, 10 };
        case ScaleType::Dorian:           return { 0, 2, 3, 5, 7, 9, 10 };
        case ScaleType::Mixolydian:       return { 0, 2, 4, 5, 7, 9, 10 };
        case ScaleType::Lydian:           return { 0, 2, 4, 6, 7, 9, 11 };
        case ScaleType::Phrygian:         return { 0, 1, 3, 5, 7, 8, 10 };
        case ScaleType::Locrian:          return { 0, 1, 3, 5, 6, 8, 10 };
        case ScaleType::HarmonicMinor:    return { 0, 2, 3, 5, 7, 8, 11 };
        case ScaleType::MelodicMinor:     return { 0, 2, 3, 5, 7, 9, 11 };
        case ScaleType::MajorPentatonic:  return { 0, 2, 4, 7, 9 };
        case ScaleType::MinorPentatonic:  return { 0, 3, 5, 7, 10 };
        case ScaleType::Blues:            return { 0, 3, 5, 6, 7, 10 };
        case ScaleType::WholeTone:        return { 0, 2, 4, 6, 8, 10 };
        case ScaleType::DiminishedHW:     return { 0, 1, 3, 4, 6, 7, 9, 10 };
        case ScaleType::DiminishedWH:     return { 0, 2, 3, 5, 6, 8, 9, 11 };
        case ScaleType::Chromatic:        return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        case ScaleType::NumScaleTypes:    break;
    }
    return { 0, 2, 4, 5, 7, 9, 11 };
}

juce::String ScaleModel::getScaleName (ScaleType type)
{
    switch (type)
    {
        case ScaleType::Major:            return "Major";
        case ScaleType::NaturalMinor:     return "Natural Minor";
        case ScaleType::Dorian:           return "Dorian";
        case ScaleType::Mixolydian:       return "Mixolydian";
        case ScaleType::Lydian:           return "Lydian";
        case ScaleType::Phrygian:         return "Phrygian";
        case ScaleType::Locrian:          return "Locrian";
        case ScaleType::HarmonicMinor:    return "Harmonic Minor";
        case ScaleType::MelodicMinor:     return "Melodic Minor";
        case ScaleType::MajorPentatonic:  return "Major Pentatonic";
        case ScaleType::MinorPentatonic:  return "Minor Pentatonic";
        case ScaleType::Blues:            return "Blues";
        case ScaleType::WholeTone:        return "Whole Tone";
        case ScaleType::DiminishedHW:     return "Diminished (HW)";
        case ScaleType::DiminishedWH:     return "Diminished (WH)";
        case ScaleType::Chromatic:        return "Chromatic";
        case ScaleType::NumScaleTypes:    break;
    }
    return "Major";
}

bool ScaleModel::voicingFitsInScale (const Voicing& v, ScaleType type, int rootPC)
{
    auto intervals = getScaleIntervals (type);

    // Build pitch class set for the scale rooted at rootPC
    std::set<int> scalePCs;
    for (int i : intervals)
        scalePCs.insert ((rootPC + i) % 12);

    // Check every voicing note's pitch class
    for (int interval : v.intervals)
    {
        int pc = (rootPC + interval) % 12;
        if (scalePCs.find (pc) == scalePCs.end())
            return false;
    }

    return true;
}

std::vector<int> ScaleModel::buildScaleRootSequence (ScaleType type, int rootPC, int baseMidi)
{
    juce::ignoreUnused (rootPC);
    auto intervals = getScaleIntervals (type);
    int n = static_cast<int> (intervals.size());

    // Returns MIDI root notes (not pitch classes) so the voicing
    // actually ascends through the octave.
    // Ascending: root up through scale degrees to the octave root
    std::vector<int> sequence;
    for (int i : intervals)
        sequence.push_back (baseMidi + i);
    sequence.push_back (baseMidi + 12);  // octave root at top

    // Descending all the way back to root
    for (int i = n - 1; i >= 0; --i)
        sequence.push_back (baseMidi + intervals[static_cast<size_t> (i)]);

    return sequence;
}

std::vector<int> ScaleModel::buildMelodyFollowSequence (const Melody& mel, int baseMidi)
{
    // Returns MIDI root notes preserving the melody's octave contour,
    // based around the voicing's original octave.
    std::vector<int> sequence;

    for (const auto& note : mel.notes)
        sequence.push_back (juce::jlimit (0, 127, baseMidi + note.intervalFromKeyRoot));

    return sequence;
}

std::vector<int> ScaleModel::buildScaleDegreeUpAndBack (int numDegrees)
{
    std::vector<int> sequence;

    // Ascending: 0, 1, ..., numDegrees (octave root at top)
    for (int i = 0; i <= numDegrees; ++i)
        sequence.push_back (i);

    // Descending all the way back to root: numDegrees-1, ..., 0
    for (int i = numDegrees - 1; i >= 0; --i)
        sequence.push_back (i);

    return sequence;
}

std::vector<int> ScaleModel::diatonicTranspose (const Voicing& v, ScaleType type,
                                                 int scaleRootPC, int degree, int baseMidi)
{
    auto intervals = getScaleIntervals (type);

    // Build scale ladder: all MIDI notes in this scale across all octaves (0-127)
    std::vector<int> ladder;
    for (int oct = -2; oct <= 11; ++oct)
    {
        for (int si : intervals)
        {
            int note = scaleRootPC + si + oct * 12;
            if (note >= 0 && note <= 127)
                ladder.push_back (note);
        }
    }
    std::sort (ladder.begin(), ladder.end());
    ladder.erase (std::unique (ladder.begin(), ladder.end()), ladder.end());

    std::vector<int> result;
    for (int interval : v.intervals)
    {
        int baseNote = baseMidi + interval;

        // Find this note in the ladder
        auto it = std::lower_bound (ladder.begin(), ladder.end(), baseNote);
        int idx;

        if (it != ladder.end() && *it == baseNote)
        {
            idx = static_cast<int> (it - ladder.begin());
        }
        else
        {
            // Note not in scale (shouldn't happen after validation), find nearest
            if (it == ladder.end())
                idx = static_cast<int> (ladder.size()) - 1;
            else if (it == ladder.begin())
                idx = 0;
            else
            {
                int above = *it;
                int below = *(it - 1);
                idx = (baseNote - below <= above - baseNote)
                    ? static_cast<int> (it - ladder.begin()) - 1
                    : static_cast<int> (it - ladder.begin());
            }
        }

        int newIdx = idx + degree;
        newIdx = std::clamp (newIdx, 0, static_cast<int> (ladder.size()) - 1);
        result.push_back (ladder[static_cast<size_t> (newIdx)]);
    }

    return result;
}
