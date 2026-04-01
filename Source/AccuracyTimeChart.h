#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SpacedRepetition.h"
#include <vector>

class AccuracyTimeChart : public juce::Component
{
public:
    AccuracyTimeChart();

    void setData (const std::vector<AttemptEntry>& attempts, int windowSize = 5);
    void clearData();
    void setBpmOptions (const std::vector<float>& bpms);
    void selectBpm (float bpm);
    float getSelectedBpm() const { return selectedBpm; }

    // Callback when user changes BPM via stepper
    std::function<void (float bpm)> onBpmChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    struct DataPoint
    {
        int index;
        double accuracy; // 0-1
    };

    std::vector<DataPoint> points;
    std::vector<float> availableBpms;
    float selectedBpm = 120.0f;
    int selectedBpmIndex = 0;

    // BPM stepper buttons
    juce::TextButton prevBpmButton { "<" };
    juce::TextButton nextBpmButton { ">" };
    juce::Label bpmLabel;

    void updateBpmLabel();
    void onPrevBpm();
    void onNextBpm();

    static constexpr int stepperHeight = 22;
};
