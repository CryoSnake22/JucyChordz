#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProgressionModel.h"

class ProgressionChartComponent : public juce::Component
{
public:
    ProgressionChartComponent();

    void setProgression (const Progression* prog);
    void setCursorBeat (double beat);
    void setSelectedChord (int index);
    void setEditMode (bool enabled);

    int getSelectedChord() const { return selectedChordIndex; }

    std::function<void (int chordIndex)> onChordSelected;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    const Progression* progression = nullptr;
    double cursorBeat = -1.0;
    int selectedChordIndex = -1;
    bool editMode = false;

    static constexpr int beatsPerRow = 16;
    static constexpr int rowHeight = 30;
    static constexpr int rowGap = 3;
    static constexpr int leftPad = 4;
    static constexpr int rightPad = 4;

    float getBeatWidth() const;
    int getRowForBeat (double beat) const;
    juce::Rectangle<float> getChordRect (const ProgressionChord& chord, int row) const;
    int hitTestChord (juce::Point<float> pos) const;
};
