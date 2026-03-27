#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class TempoEngine
{
public:
    TempoEngine() = default;

    // Connect to APVTS raw parameter pointers (call after APVTS construction)
    void connectParameters (std::atomic<float>* bpm,
                            std::atomic<float>* metronomeOn,
                            std::atomic<float>* useHostSync,
                            std::atomic<float>* responseWindowBeats,
                            std::atomic<float>* metronomeVolume = nullptr);

    void prepare (double sampleRate, int samplesPerBlock);

    // Call from processBlock — advances clock, renders click, updates atomics
    void process (juce::AudioBuffer<float>& buffer,
                  juce::AudioPlayHead* playHead,
                  int numSamples);

    // GUI-thread queries (lock-free atomic reads)
    int getBeatNumber() const { return beatNumber.load (std::memory_order_relaxed); }
    float getBeatPhase() const { return beatPhase.load (std::memory_order_relaxed); }
    double getEffectiveBpm() const { return effectiveBpm.load (std::memory_order_relaxed); }

    // Returns true once per beat crossing, then resets
    bool consumeBeatTick();

    // Reset internal clock to beat 0 (call when starting timed practice)
    void resetBeatPosition();

    // Challenge timing API (called from GUI thread)
    void markChallengeStart();
    double getBeatsSinceChallengeStart() const;
    void clearChallengeStart();

    float getResponseWindowBeats() const;

private:
    // APVTS parameter pointers
    std::atomic<float>* bpmParam = nullptr;
    std::atomic<float>* metronomeOnParam = nullptr;
    std::atomic<float>* useHostSyncParam = nullptr;
    std::atomic<float>* responseWindowBeatsParam = nullptr;
    std::atomic<float>* metronomeVolumeParam = nullptr;

    // Audio-thread state
    double sr = 44100.0;
    double samplesPerBeat = 22050.0;
    double samplePosition = 0.0;
    int currentBeat = 0;
    double absoluteBeatCount = 0.0;
    double lastBpm = 120.0;

    // Click synthesis state
    int clickSamplesRemaining = 0;
    int clickSampleOffset = 0;
    double clickPhase = 0.0;
    bool clickIsDownbeat = false;
    int clickDownbeatLength = 0;
    int clickUpbeatLength = 0;

    // Atomics for GUI thread
    std::atomic<int> beatNumber { 0 };
    std::atomic<float> beatPhase { 0.0f };
    std::atomic<bool> beatTick { false };
    std::atomic<double> effectiveBpm { 120.0 };
    std::atomic<double> currentBeatPosition { 0.0 };

    // Pending reset (GUI thread requests, audio thread executes)
    std::atomic<bool> pendingReset { false };

    // Challenge timing
    std::atomic<double> challengeStartBeatPosition { -1.0 };

    void triggerClick (int sampleOffset, bool isDownbeat);
    void renderClick (juce::AudioBuffer<float>& buffer, int numSamples);
};
