#include "TempoEngine.h"
#include <cmath>

void TempoEngine::connectParameters (std::atomic<float>* bpm,
                                     std::atomic<float>* metronomeOn,
                                     std::atomic<float>* useHostSync,
                                     std::atomic<float>* responseWindowBeats,
                                     std::atomic<float>* metronomeVolume)
{
    bpmParam = bpm;
    metronomeOnParam = metronomeOn;
    useHostSyncParam = useHostSync;
    responseWindowBeatsParam = responseWindowBeats;
    metronomeVolumeParam = metronomeVolume;
}

void TempoEngine::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
    double bpm = (bpmParam != nullptr) ? bpmParam->load() : 120.0;
    samplesPerBeat = (60.0 / bpm) * sr;
    lastBpm = bpm;
    samplePosition = 0.0;
    currentBeat = 0;
    absoluteBeatCount = 0.0;
    clickSamplesRemaining = 0;

    clickDownbeatLength = static_cast<int> (sr * 0.030); // 30ms
    clickUpbeatLength   = static_cast<int> (sr * 0.020); // 20ms
}

void TempoEngine::process (juce::AudioBuffer<float>& buffer,
                           juce::AudioPlayHead* playHead,
                           int numSamples)
{
    // Apply pending reset on the audio thread (avoids cutting off mid-render click)
    if (pendingReset.exchange (false, std::memory_order_acquire))
    {
        samplePosition = 0.0;
        currentBeat = 0;
        absoluteBeatCount = 0.0;
        // Let any in-progress click finish naturally (don't zero clickSamplesRemaining)
        clickSampleOffset = 0;

        beatNumber.store (0, std::memory_order_relaxed);
        beatPhase.store (0.0f, std::memory_order_relaxed);
        beatTick.store (false, std::memory_order_relaxed);
        currentBeatPosition.store (0.0, std::memory_order_relaxed);
    }

    float bpm = (bpmParam != nullptr) ? bpmParam->load() : 120.0f;
    bool metronomeOn = (metronomeOnParam != nullptr) && metronomeOnParam->load() > 0.5f;
    bool useHost = (useHostSyncParam != nullptr) && useHostSyncParam->load() > 0.5f;

    // Host sync path
    if (useHost && playHead != nullptr)
    {
        auto pos = playHead->getPosition();
        if (pos.hasValue() && pos->getBpm().hasValue() && pos->getIsPlaying())
        {
            double hostBpm = *pos->getBpm();
            samplesPerBeat = (60.0 / hostBpm) * sr;
            effectiveBpm.store (hostBpm, std::memory_order_relaxed);

            if (pos->getPpqPosition().hasValue())
            {
                double ppq = *pos->getPpqPosition();
                int beat = static_cast<int> (std::floor (ppq)) % 4;
                if (beat < 0) beat += 4;
                float phase = static_cast<float> (ppq - std::floor (ppq));

                beatNumber.store (beat, std::memory_order_relaxed);
                beatPhase.store (phase, std::memory_order_relaxed);
                currentBeatPosition.store (ppq, std::memory_order_relaxed);

                // Detect beat crossing within this buffer
                double ppqAtEnd = ppq + static_cast<double> (numSamples) / samplesPerBeat;
                double nextBeatBoundary = std::ceil (ppq);

                // Avoid triggering on exact boundary (ppq == nextBeatBoundary)
                if (ppq < nextBeatBoundary && nextBeatBoundary <= ppqAtEnd)
                {
                    int offset = static_cast<int> ((nextBeatBoundary - ppq) * samplesPerBeat);
                    offset = juce::jlimit (0, numSamples - 1, offset);
                    bool isDownbeat = (static_cast<int> (nextBeatBoundary) % 4 == 0);
                    triggerClick (offset, isDownbeat);
                    beatTick.store (true, std::memory_order_relaxed);
                }
            }

            if (metronomeOn)
                renderClick (buffer, numSamples);
            return;
        }
    }

    // Internal clock path
    if (std::abs (static_cast<double> (bpm) - lastBpm) > 0.001)
    {
        samplesPerBeat = (60.0 / static_cast<double> (bpm)) * sr;
        lastBpm = bpm;
    }
    effectiveBpm.store (static_cast<double> (bpm), std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        samplePosition += 1.0;
        if (samplePosition >= samplesPerBeat)
        {
            samplePosition -= samplesPerBeat;
            currentBeat = (currentBeat + 1) % 4;
            absoluteBeatCount += 1.0;

            beatNumber.store (currentBeat, std::memory_order_relaxed);
            beatTick.store (true, std::memory_order_relaxed);

            if (metronomeOn)
                triggerClick (i, currentBeat == 0);
        }
    }

    float phase = static_cast<float> (samplePosition / samplesPerBeat);
    beatPhase.store (phase, std::memory_order_relaxed);
    currentBeatPosition.store (absoluteBeatCount + static_cast<double> (phase),
                               std::memory_order_relaxed);

    if (metronomeOn)
        renderClick (buffer, numSamples);
}

bool TempoEngine::consumeBeatTick()
{
    return beatTick.exchange (false, std::memory_order_relaxed);
}

void TempoEngine::resetBeatPosition()
{
    // Request reset — the audio thread will apply it at the start of the next buffer
    // to avoid cutting off a mid-render metronome click (which causes a pop)
    pendingReset.store (true, std::memory_order_release);
}

void TempoEngine::markChallengeStart()
{
    challengeStartBeatPosition.store (
        currentBeatPosition.load (std::memory_order_relaxed),
        std::memory_order_relaxed);
}

double TempoEngine::getBeatsSinceChallengeStart() const
{
    double start = challengeStartBeatPosition.load (std::memory_order_relaxed);
    if (start < 0.0)
        return 0.0;
    return currentBeatPosition.load (std::memory_order_relaxed) - start;
}

void TempoEngine::clearChallengeStart()
{
    challengeStartBeatPosition.store (-1.0, std::memory_order_relaxed);
}

float TempoEngine::getResponseWindowBeats() const
{
    return (responseWindowBeatsParam != nullptr) ? responseWindowBeatsParam->load() : 4.0f;
}

// --- Click synthesis ---

void TempoEngine::triggerClick (int sampleOffset, bool isDownbeat)
{
    clickSampleOffset = sampleOffset;
    clickIsDownbeat = isDownbeat;
    clickSamplesRemaining = isDownbeat ? clickDownbeatLength : clickUpbeatLength;
    clickPhase = 0.0;
}

void TempoEngine::renderClick (juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (clickSamplesRemaining <= 0)
        return;

    int totalClickDuration = clickIsDownbeat ? clickDownbeatLength : clickUpbeatLength;
    double freq = clickIsDownbeat ? 1000.0 : 800.0;
    float amplitude = clickIsDownbeat ? 0.4f : 0.28f;
    float metVol = (metronomeVolumeParam != nullptr) ? metronomeVolumeParam->load() : 0.7f;
    amplitude *= metVol;
    double phaseInc = freq / sr * juce::MathConstants<double>::twoPi;

    int startSample = clickSampleOffset;
    clickSampleOffset = 0; // reset for subsequent process() calls

    for (int i = startSample; i < numSamples && clickSamplesRemaining > 0;
         ++i, --clickSamplesRemaining)
    {
        float envelope = static_cast<float> (clickSamplesRemaining)
                         / static_cast<float> (totalClickDuration);
        float sample = std::sin (static_cast<float> (clickPhase)) * amplitude * envelope;
        clickPhase += phaseInc;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[i] += sample;
    }
}
