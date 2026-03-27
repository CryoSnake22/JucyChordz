#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SpacedRepetition.h"
#include <array>
#include <functional>

class VoicingStatsChart : public juce::Component
{
public:
    VoicingStatsChart();

    void setStats (const std::array<SpacedRepetitionEngine::KeyStats, 12>& stats);
    void clearStats();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseEnter (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

    // Callback: called with key index (0=C .. 11=B) when a bar is clicked
    std::function<void (int keyIndex)> onKeyClicked;

    // Visual feedback: which bar is currently playing (-1 = none)
    void setPlayingKey (int keyIndex);
    int getPlayingKey() const { return playingBar; }

private:
    std::array<SpacedRepetitionEngine::KeyStats, 12> keyStats {};
    bool hasData = false;
    int hoveredBar = -1;
    int playingBar = -1;

    int getBarIndexAt (float x) const;
};
