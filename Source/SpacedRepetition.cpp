#include "SpacedRepetition.h"
#include <algorithm>
#include <cmath>
#include <set>

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
    r.attemptHistory.push_back (5.0);
    if (r.attemptHistory.size() > 100)
        r.attemptHistory.erase (r.attemptHistory.begin(),
            r.attemptHistory.begin() + static_cast<std::ptrdiff_t> (r.attemptHistory.size() - 100));
}

void SpacedRepetitionEngine::applyFailure (PracticeRecord& r)
{
    r.failures++;
    r.lastAttemptTime = currentTimeSeconds();
    r.intervalDays = 1.0;
    r.easeFactor = std::max (1.3, r.easeFactor - 0.2);
    r.attemptHistory.push_back (0.0);
    if (r.attemptHistory.size() > 100)
        r.attemptHistory.erase (r.attemptHistory.begin(),
            r.attemptHistory.begin() + static_cast<std::ptrdiff_t> (r.attemptHistory.size() - 100));
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

    r.attemptHistory.push_back (static_cast<double> (quality));
    if (r.attemptHistory.size() > 100)
        r.attemptHistory.erase (r.attemptHistory.begin(),
            r.attemptHistory.begin() + static_cast<std::ptrdiff_t> (r.attemptHistory.size() - 100));
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
                                             int keyIndex, float bpm)
{
    auto& r = getOrCreateRecord (voicingId, keyIndex);
    applySuccess (r);

    AttemptEntry entry;
    entry.quality = 5.0;
    entry.bpm = bpm;
    entry.timestamp = currentTimeSeconds();
    r.detailedHistory.push_back (entry);
    if (r.detailedHistory.size() > 500)
        r.detailedHistory.erase (r.detailedHistory.begin(),
            r.detailedHistory.begin() + static_cast<std::ptrdiff_t> (r.detailedHistory.size() - 500));
}

void SpacedRepetitionEngine::recordFailure (const juce::String& voicingId,
                                             int keyIndex)
{
    auto& r = getOrCreateRecord (voicingId, keyIndex);
    applyFailure (r);
}

void SpacedRepetitionEngine::recordAttempt (const juce::String& voicingId,
                                            int keyIndex, int quality, float bpm)
{
    auto& r = getOrCreateRecord (voicingId, keyIndex);
    applyQuality (r, quality);

    // Also store in detailed history for BPM-differentiated stats
    AttemptEntry entry;
    entry.quality = static_cast<double> (quality);
    entry.bpm = bpm;
    entry.timestamp = currentTimeSeconds();
    r.detailedHistory.push_back (entry);
    if (r.detailedHistory.size() > 500)
        r.detailedHistory.erase (r.detailedHistory.begin(),
            r.detailedHistory.begin() + static_cast<std::ptrdiff_t> (r.detailedHistory.size() - 500));
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
            auto idx = static_cast<size_t> (r.keyIndex);
            stats[idx].successes = r.successes;
            stats[idx].failures = r.failures;
            stats[idx].lastQuality = r.lastResponseQuality;
            stats[idx].recentQualities = r.attemptHistory;
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
    // Use recency-weighted accuracy if any history exists
    double weightedSum = 0.0;
    double weightTotal = 0.0;
    for (const auto& r : records)
    {
        int n = static_cast<int> (r.attemptHistory.size());
        for (int i = 0; i < n; ++i)
        {
            double weight = (i >= n - 10) ? 2.0 : 1.0;
            weightedSum += (r.attemptHistory[static_cast<size_t> (i)] / 5.0) * weight;
            weightTotal += weight;
        }
    }
    if (weightTotal > 0.0)
        return weightedSum / weightTotal;

    // Legacy fallback
    int attempts = getTotalAttempts();
    if (attempts == 0)
        return 0.0;
    return static_cast<double> (getTotalSuccesses()) / attempts;
}

// --- Detailed history queries ---

std::vector<AttemptEntry> SpacedRepetitionEngine::getDetailedHistory (
    const juce::String& voicingId, float bpm) const
{
    std::vector<AttemptEntry> result;
    for (const auto& r : records)
    {
        if (r.voicingId != voicingId)
            continue;
        for (const auto& entry : r.detailedHistory)
        {
            if (bpm < 0.0f || std::abs (entry.bpm - bpm) < 0.1f)
                result.push_back (entry);
        }
    }
    // Sort by timestamp
    std::sort (result.begin(), result.end(),
        [] (const AttemptEntry& a, const AttemptEntry& b) { return a.timestamp < b.timestamp; });
    return result;
}

std::vector<float> SpacedRepetitionEngine::getDistinctBpms (const juce::String& voicingId) const
{
    std::set<int> bpmSet;
    for (const auto& r : records)
    {
        if (r.voicingId != voicingId)
            continue;
        for (const auto& entry : r.detailedHistory)
            bpmSet.insert (static_cast<int> (entry.bpm));
    }
    std::vector<float> result;
    for (int b : bpmSet)
        result.push_back (static_cast<float> (b));
    return result;
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
static const juce::Identifier ID_attemptHistory ("attemptHistory");
static const juce::Identifier ID_detailedHistory ("detailedHistory");

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

        // Serialize attemptHistory as comma-separated doubles
        if (! r.attemptHistory.empty())
        {
            juce::StringArray histParts;
            for (double q : r.attemptHistory)
                histParts.add (juce::String (q, 1));
            child.setProperty (ID_attemptHistory, histParts.joinIntoString (","), nullptr);
        }

        // Serialize detailedHistory as "quality:bpm:timestamp;..." compact string
        if (! r.detailedHistory.empty())
        {
            juce::StringArray entries;
            for (const auto& e : r.detailedHistory)
                entries.add (juce::String (e.quality, 1) + ":"
                             + juce::String (static_cast<int> (e.bpm)) + ":"
                             + juce::String (e.timestamp, 0));
            child.setProperty (ID_detailedHistory, entries.joinIntoString (";"), nullptr);
        }

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

            // Deserialize attemptHistory
            juce::String histStr = child.getProperty (ID_attemptHistory).toString();
            if (histStr.isNotEmpty())
            {
                auto parts = juce::StringArray::fromTokens (histStr, ",", "");
                for (const auto& p : parts)
                    if (p.trim().isNotEmpty())
                        r.attemptHistory.push_back (p.getDoubleValue());
            }

            // Deserialize detailedHistory ("quality:bpm:timestamp;...")
            juce::String detailedStr = child.getProperty (ID_detailedHistory).toString();
            if (detailedStr.isNotEmpty())
            {
                auto entries = juce::StringArray::fromTokens (detailedStr, ";", "");
                for (const auto& entry : entries)
                {
                    auto fields = juce::StringArray::fromTokens (entry, ":", "");
                    if (fields.size() >= 3)
                    {
                        AttemptEntry ae;
                        ae.quality = fields[0].getDoubleValue();
                        ae.bpm = static_cast<float> (fields[1].getDoubleValue());
                        ae.timestamp = fields[2].getDoubleValue();
                        r.detailedHistory.push_back (ae);
                    }
                }
            }

            records.push_back (r);
        }
    }
}
