#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

struct AttemptEntry
{
    double quality = 0.0;     // 0-5
    float bpm = 120.0f;       // BPM at time of attempt
    double timestamp = 0.0;   // seconds since epoch
};

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
    std::vector<double> attemptHistory; // per-attempt quality scores (0-5), capped at 100
    std::vector<AttemptEntry> detailedHistory; // full history with BPM + timestamp, capped at 500
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
    void recordSuccess (const juce::String& voicingId, int keyIndex, float bpm = 120.0f);
    void recordFailure (const juce::String& voicingId, int keyIndex);

    // Quality-based recording (0-5, full SM-2 scale)
    // 0=timeout, 1=wrong then corrected, 2=slow, 3=ok, 4=good, 5=perfect
    void recordAttempt (const juce::String& voicingId, int keyIndex, int quality, float bpm = 120.0f);

    // Get the next challenge for a given voicing (picks the most-overdue key)
    // avoidKey: if >= 0, skip this key to prevent consecutive repeats
    PracticeChallenge getNextChallenge (const juce::String& voicingId, int avoidKey = -1) const;

    // Get the next challenge across all voicings (needs list of voicing IDs)
    PracticeChallenge getNextChallenge (const std::vector<juce::String>& voicingIds) const;

    // Per-key stats for a voicing
    struct KeyStats
    {
        int successes = 0;
        int failures = 0;
        int lastQuality = -1;
        std::vector<double> recentQualities;
        double accuracy() const
        {
            if (! recentQualities.empty())
            {
                // Recency-weighted: most recent 10 get 2x weight, older get 1x
                double weightedSum = 0.0;
                double weightTotal = 0.0;
                int n = static_cast<int> (recentQualities.size());
                for (int i = 0; i < n; ++i)
                {
                    double weight = (i >= n - 10) ? 2.0 : 1.0;
                    weightedSum += (recentQualities[static_cast<size_t> (i)] / 5.0) * weight;
                    weightTotal += weight;
                }
                return weightTotal > 0.0 ? weightedSum / weightTotal : -1.0;
            }
            // Legacy fallback
            int total = successes + failures;
            return total > 0 ? static_cast<double> (successes) / total : -1.0;
        }
    };

    std::array<KeyStats, 12> getStatsForVoicing (const juce::String& voicingId) const;

    // Per-voicing detailed history queries (for accuracy stats chart)
    std::vector<AttemptEntry> getDetailedHistory (const juce::String& voicingId, float bpm = -1.0f) const;
    std::vector<float> getDistinctBpms (const juce::String& voicingId) const;

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
