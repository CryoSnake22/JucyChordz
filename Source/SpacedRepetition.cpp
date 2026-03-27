#include "SpacedRepetition.h"
#include <algorithm>
#include <cmath>

static double currentTimeSeconds()
{
    return juce::Time::currentTimeMillis() / 1000.0;
}

// --- SM-2 Algorithm ---

void SpacedRepetitionEngine::applySuccess (PracticeRecord& r)
{
    r.successes++;
    r.lastAttemptTime = currentTimeSeconds();
    r.intervalDays *= r.easeFactor;
    r.easeFactor = std::min (2.5, r.easeFactor + 0.1);
}

void SpacedRepetitionEngine::applyFailure (PracticeRecord& r)
{
    r.failures++;
    r.lastAttemptTime = currentTimeSeconds();
    r.intervalDays = 1.0;
    r.easeFactor = std::max (1.3, r.easeFactor - 0.2);
}

void SpacedRepetitionEngine::applyQuality (PracticeRecord& r, int quality)
{
    r.lastAttemptTime = currentTimeSeconds();
    r.lastResponseQuality = quality;

    if (quality >= 3)
    {
        // Success path
        r.successes++;
        int n = r.successes;
        if (n == 1)
            r.intervalDays = 1.0;
        else if (n == 2)
            r.intervalDays = 6.0;
        else
            r.intervalDays *= r.easeFactor;
    }
    else
    {
        // Failure path — reset interval but keep easeFactor
        r.failures++;
        r.intervalDays = 1.0;
    }

    // SM-2 ease factor update: EF' = EF + (0.1 - (5-q) * (0.08 + (5-q) * 0.02))
    double q = static_cast<double> (quality);
    r.easeFactor += 0.1 - (5.0 - q) * (0.08 + (5.0 - q) * 0.02);
    r.easeFactor = std::max (1.3, r.easeFactor);
}

double SpacedRepetitionEngine::getOverdueScore (const PracticeRecord& r,
                                                 double currentTime)
{
    double elapsedDays = (currentTime - r.lastAttemptTime) / 86400.0;
    return elapsedDays / r.intervalDays; // > 1.0 means overdue
}

// --- Record management ---

PracticeRecord* SpacedRepetitionEngine::findRecord (const juce::String& voicingId,
                                                     int keyIndex)
{
    for (auto& r : records)
        if (r.voicingId == voicingId && r.keyIndex == keyIndex)
            return &r;
    return nullptr;
}

const PracticeRecord* SpacedRepetitionEngine::findRecord (
    const juce::String& voicingId, int keyIndex) const
{
    for (const auto& r : records)
        if (r.voicingId == voicingId && r.keyIndex == keyIndex)
            return &r;
    return nullptr;
}

PracticeRecord& SpacedRepetitionEngine::getOrCreateRecord (
    const juce::String& voicingId, int keyIndex)
{
    auto* existing = findRecord (voicingId, keyIndex);
    if (existing != nullptr)
        return *existing;

    PracticeRecord newRecord;
    newRecord.voicingId = voicingId;
    newRecord.keyIndex = keyIndex;
    records.push_back (newRecord);
    return records.back();
}

// --- Public API ---

void SpacedRepetitionEngine::recordSuccess (const juce::String& voicingId,
                                             int keyIndex)
{
    auto& r = getOrCreateRecord (voicingId, keyIndex);
    applySuccess (r);
}

void SpacedRepetitionEngine::recordFailure (const juce::String& voicingId,
                                             int keyIndex)
{
    auto& r = getOrCreateRecord (voicingId, keyIndex);
    applyFailure (r);
}

void SpacedRepetitionEngine::recordAttempt (const juce::String& voicingId,
                                            int keyIndex, int quality)
{
    auto& r = getOrCreateRecord (voicingId, keyIndex);
    applyQuality (r, quality);
}

PracticeChallenge SpacedRepetitionEngine::getNextChallenge (
    const juce::String& voicingId, int avoidKey) const
{
    double now = currentTimeSeconds();
    double bestScore = -1.0;
    int bestKey = -1;
    double secondBestScore = -1.0;
    int secondBestKey = -1;

    // Check all 12 keys
    for (int key = 0; key < 12; ++key)
    {
        const auto* r = findRecord (voicingId, key);
        double score;

        if (r == nullptr)
        {
            // Never practiced = highest priority (score 100)
            score = 100.0;
        }
        else
        {
            score = getOverdueScore (*r, now);
            // Boost keys with more failures
            if (r->failures > r->successes)
                score *= 1.5;
        }

        if (score > bestScore)
        {
            secondBestScore = bestScore;
            secondBestKey = bestKey;
            bestScore = score;
            bestKey = key;
        }
        else if (score > secondBestScore)
        {
            secondBestScore = score;
            secondBestKey = key;
        }
    }

    // Avoid repeating the same key if possible
    if (bestKey == avoidKey && secondBestKey >= 0)
        bestKey = secondBestKey;

    if (bestKey < 0)
        bestKey = 0;

    PracticeChallenge challenge;
    challenge.voicingId = voicingId;
    challenge.keyIndex = bestKey;
    challenge.rootMidiNote = 48 + bestKey; // C3 octave base
    return challenge;
}

PracticeChallenge SpacedRepetitionEngine::getNextChallenge (
    const std::vector<juce::String>& voicingIds) const
{
    if (voicingIds.empty())
        return {};

    double now = currentTimeSeconds();
    double bestScore = -1.0;
    PracticeChallenge bestChallenge;

    for (const auto& vid : voicingIds)
    {
        for (int key = 0; key < 12; ++key)
        {
            const auto* r = findRecord (vid, key);
            double score;

            if (r == nullptr)
                score = 100.0;
            else
            {
                score = getOverdueScore (*r, now);
                if (r->failures > r->successes)
                    score *= 1.5;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestChallenge.voicingId = vid;
                bestChallenge.keyIndex = key;
                bestChallenge.rootMidiNote = 48 + key;
            }
        }
    }

    return bestChallenge;
}

std::array<SpacedRepetitionEngine::KeyStats, 12>
SpacedRepetitionEngine::getStatsForVoicing (const juce::String& voicingId) const
{
    std::array<KeyStats, 12> stats {};
    for (const auto& r : records)
    {
        if (r.voicingId == voicingId && r.keyIndex >= 0 && r.keyIndex < 12)
        {
            stats[static_cast<size_t> (r.keyIndex)].successes = r.successes;
            stats[static_cast<size_t> (r.keyIndex)].failures = r.failures;
            stats[static_cast<size_t> (r.keyIndex)].lastQuality = r.lastResponseQuality;
        }
    }
    return stats;
}

int SpacedRepetitionEngine::getTotalAttempts() const
{
    int total = 0;
    for (const auto& r : records)
        total += r.successes + r.failures;
    return total;
}

int SpacedRepetitionEngine::getTotalSuccesses() const
{
    int total = 0;
    for (const auto& r : records)
        total += r.successes;
    return total;
}

double SpacedRepetitionEngine::getAccuracy() const
{
    int attempts = getTotalAttempts();
    if (attempts == 0)
        return 0.0;
    return static_cast<double> (getTotalSuccesses()) / attempts;
}

// --- Serialization ---

static const juce::Identifier ID_SpacedRepetition ("SpacedRepetition");
static const juce::Identifier ID_Record ("Record");
static const juce::Identifier ID_voicingId ("voicingId");
static const juce::Identifier ID_key ("key");
static const juce::Identifier ID_successes ("successes");
static const juce::Identifier ID_failures ("failures");
static const juce::Identifier ID_lastAttempt ("lastAttempt");
static const juce::Identifier ID_interval ("interval");
static const juce::Identifier ID_ease ("ease");
static const juce::Identifier ID_quality ("quality");

juce::ValueTree SpacedRepetitionEngine::toValueTree() const
{
    juce::ValueTree tree (ID_SpacedRepetition);
    for (const auto& r : records)
    {
        juce::ValueTree child (ID_Record);
        child.setProperty (ID_voicingId, r.voicingId, nullptr);
        child.setProperty (ID_key, r.keyIndex, nullptr);
        child.setProperty (ID_successes, r.successes, nullptr);
        child.setProperty (ID_failures, r.failures, nullptr);
        child.setProperty (ID_lastAttempt, r.lastAttemptTime, nullptr);
        child.setProperty (ID_interval, r.intervalDays, nullptr);
        child.setProperty (ID_ease, r.easeFactor, nullptr);
        child.setProperty (ID_quality, r.lastResponseQuality, nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

void SpacedRepetitionEngine::fromValueTree (const juce::ValueTree& tree)
{
    records.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (child.hasType (ID_Record))
        {
            PracticeRecord r;
            r.voicingId = child.getProperty (ID_voicingId).toString();
            r.keyIndex = child.getProperty (ID_key, 0);
            r.successes = child.getProperty (ID_successes, 0);
            r.failures = child.getProperty (ID_failures, 0);
            r.lastAttemptTime = child.getProperty (ID_lastAttempt, 0.0);
            r.intervalDays = child.getProperty (ID_interval, 1.0);
            r.easeFactor = child.getProperty (ID_ease, 2.5);
            r.lastResponseQuality = child.getProperty (ID_quality, -1);
            records.push_back (r);
        }
    }
}
