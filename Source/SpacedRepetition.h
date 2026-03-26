#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

struct PracticeRecord
{
    juce::String voicingId;
    int keyIndex = 0;           // 0-11 (C=0 .. B=11)
    int successes = 0;
    int failures = 0;
    double lastAttemptTime = 0; // seconds since epoch
    double intervalDays = 1.0;
    double easeFactor = 2.5;
    int lastResponseQuality = -1; // -1 = no data, 0-5 SM-2 quality
};

struct PracticeChallenge
{
    juce::String voicingId;
    int keyIndex = 0;           // root pitch class for this challenge
    int rootMidiNote = 60;      // actual MIDI note to transpose to (middle octave)
};

class SpacedRepetitionEngine
{
public:
    SpacedRepetitionEngine() = default;

    // Record a practice result
    void recordSuccess (const juce::String& voicingId, int keyIndex);
    void recordFailure (const juce::String& voicingId, int keyIndex);

    // Quality-based recording (0-5, full SM-2 scale)
    // 0=timeout, 1=wrong then corrected, 2=slow, 3=ok, 4=good, 5=perfect
    void recordAttempt (const juce::String& voicingId, int keyIndex, int quality);

    // Get the next challenge for a given voicing (picks the most-overdue key)
    PracticeChallenge getNextChallenge (const juce::String& voicingId) const;

    // Get the next challenge across all voicings (needs list of voicing IDs)
    PracticeChallenge getNextChallenge (const std::vector<juce::String>& voicingIds) const;

    // Per-key stats for a voicing
    struct KeyStats
    {
        int successes = 0;
        int failures = 0;
        int lastQuality = -1;
        double accuracy() const
        {
            int total = successes + failures;
            return total > 0 ? static_cast<double> (successes) / total : -1.0;
        }
    };

    std::array<KeyStats, 12> getStatsForVoicing (const juce::String& voicingId) const;

    // Global stats
    int getTotalAttempts() const;
    int getTotalSuccesses() const;
    double getAccuracy() const;

    // Serialization
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    std::vector<PracticeRecord> records;

    PracticeRecord* findRecord (const juce::String& voicingId, int keyIndex);
    const PracticeRecord* findRecord (const juce::String& voicingId, int keyIndex) const;
    PracticeRecord& getOrCreateRecord (const juce::String& voicingId, int keyIndex);

    // SM-2 algorithm helpers
    static void applySuccess (PracticeRecord& r);
    static void applyFailure (PracticeRecord& r);
    static void applyQuality (PracticeRecord& r, int quality);
    static double getOverdueScore (const PracticeRecord& r, double currentTime);
};
