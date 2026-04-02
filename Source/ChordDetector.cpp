#include "ChordDetector.h"
#include <algorithm>

const std::vector<ChordDetector::ChordTemplate>& ChordDetector::getTemplates()
{
    // Ordered from most specific (more notes) to least specific so longer matches win
    static const std::vector<ChordTemplate> templates = {
        // 6-note chords (13ths)
        { { 0, 2, 4, 7, 9, 10 }, ChordQuality::Dom13 },
        { { 0, 2, 4, 7, 9, 11 }, ChordQuality::Maj13 },
        { { 0, 2, 3, 7, 9, 10 }, ChordQuality::Min13 },

        // 5-note chords (6/9, 11ths, 9ths, altered)
        { { 0, 2, 4, 7, 9 },  ChordQuality::Maj69 },
        { { 0, 2, 3, 7, 9 },  ChordQuality::Min69 },
        { { 0, 2, 4, 7, 10 }, ChordQuality::Dom9 },
        { { 0, 2, 4, 7, 11 }, ChordQuality::Maj9 },
        { { 0, 2, 3, 7, 10 }, ChordQuality::Min9 },
        { { 0, 2, 3, 7, 11 }, ChordQuality::MinMaj9 },
        { { 0, 1, 4, 7, 10 }, ChordQuality::Dom7b9 },
        { { 0, 3, 4, 7, 10 }, ChordQuality::Dom7sharp9 },
        { { 0, 2, 4, 5, 10 }, ChordQuality::Dom11 },
        { { 0, 2, 3, 5, 10 }, ChordQuality::Min11 },
        { { 0, 2, 4, 6, 11 }, ChordQuality::Maj7sharp11 },

        // 4-note chords (7ths, 6ths, add9, altered dominants)
        { { 0, 4, 7, 11 }, ChordQuality::Maj7 },
        { { 0, 4, 7, 10 }, ChordQuality::Dom7 },
        { { 0, 3, 7, 10 }, ChordQuality::Min7 },
        { { 0, 3, 7, 11 }, ChordQuality::MinMaj7 },
        { { 0, 3, 6, 10 }, ChordQuality::HalfDim7 },
        { { 0, 3, 6,  9 }, ChordQuality::Dim7 },
        { { 0, 4, 8, 10 }, ChordQuality::Dom7sharp5 },
        { { 0, 4, 6, 10 }, ChordQuality::Dom7b5 },
        { { 0, 4, 7,  9 }, ChordQuality::Maj6 },
        { { 0, 3, 7,  9 }, ChordQuality::Min6 },
        { { 0, 2, 4, 7 },  ChordQuality::Add9 },
        { { 0, 2, 3, 7 },  ChordQuality::MinAdd9 },

        // 3-note chords (triads)
        { { 0, 4, 7 }, ChordQuality::Major },
        { { 0, 3, 7 }, ChordQuality::Minor },
        { { 0, 3, 6 }, ChordQuality::Diminished },
        { { 0, 4, 8 }, ChordQuality::Augmented },
        { { 0, 2, 7 }, ChordQuality::Sus2 },
        { { 0, 5, 7 }, ChordQuality::Sus4 },
    };
    return templates;
}

juce::String ChordDetector::noteNameFromPitchClass (int pitchClass)
{
    static const char* names[] = { "C", "Db", "D", "Eb", "E", "F",
                                   "Gb", "G", "Ab", "A", "Bb", "B" };
    if (pitchClass < 0 || pitchClass > 11)
        return "?";
    return names[pitchClass];
}

juce::String ChordDetector::qualitySuffix (ChordQuality q)
{
    switch (q)
    {
        case ChordQuality::Major:         return "";
        case ChordQuality::Minor:         return "m";
        case ChordQuality::Diminished:    return "dim";
        case ChordQuality::Augmented:     return "aug";
        case ChordQuality::Maj6:          return "6";
        case ChordQuality::Min6:          return "m6";
        case ChordQuality::Dom7:          return "7";
        case ChordQuality::Maj7:          return "maj7";
        case ChordQuality::Min7:          return "m7";
        case ChordQuality::MinMaj7:       return "m(maj7)";
        case ChordQuality::Dim7:          return "dim7";
        case ChordQuality::HalfDim7:      return "m7b5";
        case ChordQuality::Dom7b5:        return "7b5";
        case ChordQuality::Dom7sharp5:    return "7#5";
        case ChordQuality::Dom7b9:        return "7b9";
        case ChordQuality::Dom7sharp9:    return "7#9";
        case ChordQuality::Dom9:          return "9";
        case ChordQuality::Maj9:          return "maj9";
        case ChordQuality::Min9:          return "m9";
        case ChordQuality::MinMaj9:       return "m(maj9)";
        case ChordQuality::Dom11:         return "11";
        case ChordQuality::Min11:         return "m11";
        case ChordQuality::Maj7sharp11:   return "maj7#11";
        case ChordQuality::Dom13:         return "13";
        case ChordQuality::Maj13:         return "maj13";
        case ChordQuality::Min13:         return "m13";
        case ChordQuality::Maj69:         return "6/9";
        case ChordQuality::Min69:         return "m6/9";
        case ChordQuality::Add9:          return "add9";
        case ChordQuality::MinAdd9:       return "m(add9)";
        case ChordQuality::Sus2:          return "sus2";
        case ChordQuality::Sus4:          return "sus4";
        case ChordQuality::Unknown:       return "?";
    }
    return "?";
}

std::vector<int> ChordDetector::getChordTones (ChordQuality q)
{
    const auto& templates = getTemplates();
    for (const auto& tmpl : templates)
        if (tmpl.quality == q)
            return std::vector<int> (tmpl.intervals.begin(), tmpl.intervals.end());

    // Unknown or no template found — return just the root
    return { 0 };
}

ChordResult ChordDetector::detect (const std::vector<int>& midiNotes)
{
    ChordResult result;

    if (midiNotes.size() < 2)
        return result;

    // Find bass note (lowest)
    result.bassNote = *std::min_element (midiNotes.begin(), midiNotes.end());
    int bassPitchClass = result.bassNote % 12;

    // Build pitch class set
    std::set<int> pitchClasses;
    for (int note : midiNotes)
        pitchClasses.insert (note % 12);

    if (pitchClasses.size() < 2)
        return result;

    const auto& templates = getTemplates();

    // Best match tracking
    int bestScore = -1;
    int bestRoot = -1;
    ChordQuality bestQuality = ChordQuality::Unknown;

    // Try each of the 12 possible roots
    for (int candidateRoot = 0; candidateRoot < 12; ++candidateRoot)
    {
        // Compute intervals relative to this root
        std::set<int> intervals;
        for (int pc : pitchClasses)
            intervals.insert ((pc - candidateRoot + 12) % 12);

        // Must contain the root (interval 0)
        if (intervals.find (0) == intervals.end())
            continue;

        // Try matching against templates
        for (const auto& tmpl : templates)
        {
            // Check if the template intervals are a subset of our intervals
            bool allMatch = true;
            for (int ti : tmpl.intervals)
            {
                if (intervals.find (ti) == intervals.end())
                {
                    allMatch = false;
                    break;
                }
            }

            if (! allMatch)
                continue;

            // Score: prefer more template notes matched, heavily prefer root = bass note
            // Bass note is the strongest signal for root in jazz voicings
            int score = static_cast<int> (tmpl.intervals.size()) * 10;
            if (candidateRoot == bassPitchClass)
                score += 50;

            if (score > bestScore)
            {
                bestScore = score;
                bestRoot = candidateRoot;
                bestQuality = tmpl.quality;
            }
        }
    }

    if (bestRoot >= 0)
    {
        result.rootPitchClass = bestRoot;
        result.quality = bestQuality;

        // Build display name
        juce::String name = noteNameFromPitchClass (bestRoot) + qualitySuffix (bestQuality);

        // Add slash bass if different from root
        if (bassPitchClass != bestRoot)
            name += "/" + noteNameFromPitchClass (bassPitchClass);

        result.displayName = name;
    }

    return result;
}
