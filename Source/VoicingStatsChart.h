#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SpacedRepetition.h"
#include <array>

class VoicingStatsChart : public juce::Component
{
public:
    VoicingStatsChart() = default;

    void setStats (const std::array<SpacedRepetitionEngine::KeyStats, 12>& stats);
    void clearStats();

    void paint (juce::Graphics& g) override;

private:
    std::array<SpacedRepetitionEngine::KeyStats, 12> keyStats {};
    bool hasData = false;
};
