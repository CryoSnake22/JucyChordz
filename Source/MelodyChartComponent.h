#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MelodyModel.h"
#include <set>

class MelodyChartComponent : public juce::Component
{
public:
    MelodyChartComponent();

    void setMelody (Melody* melody);
    void setMelodyReadOnly (const Melody* melody);
    void setCursorBeat (double beat);
    void setHighlightedNoteIndex (int index);
    void setEditMode (bool enabled);
    void setQuantizeGrid (double resolution) { quantizeGrid = resolution; }

    // Practice highlighting: note index -> state
    enum class NoteState { Default, Target, Correct, Missed };
    void setNoteState (int noteIndex, NoteState state);
    void clearNoteStates();

    int getHighlightedNoteIndex() const { return highlightedNoteIndex; }

    // Chord context editing
    int getSelectedChordContext() const { return selectedChordContextIndex; }
    void setSelectedChordContext (int index);

    std::function<void (int noteIndex)> onNoteSelected;
    std::function<void (int chordContextIndex)> onChordContextSelected;
    std::function<void()> onChordContextResized;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    Melody* melody = nullptr;
    const Melody* melodyReadOnly = nullptr;
    const Melody* getMelody() const { return melody != nullptr ? melody : melodyReadOnly; }

    double cursorBeat = -1.0;
    int highlightedNoteIndex = -1;
    int selectedChordContextIndex = -1;
    bool editMode = false;
    double quantizeGrid = 0.5;

    std::map<int, NoteState> noteStates;

    // Drag state for chord context edges
    enum class DragEdge { None, Left, Right };
    DragEdge dragEdge = DragEdge::None;
    int dragChordIndex = -1;

    static constexpr int beatsPerRow = 8;
    static constexpr int noteAreaHeight = 70;
    static constexpr int chordBarHeight = 20;
    static constexpr int rowGap = 4;
    static constexpr int leftPad = 4;
    static constexpr int rightPad = 4;
    static constexpr float edgeHitZone = 6.0f;

    int rowHeight() const { return noteAreaHeight + chordBarHeight; }

    float getBeatWidth() const;
    int getRowForBeat (double beat) const;
    void computePitchRange (int& minInterval, int& maxInterval) const;
    float intervalToY (int interval, int minInterval, int maxInterval, int row) const;
    juce::Rectangle<float> getNoteRect (const MelodyNote& note, int minInterval, int maxInterval, int row) const;
    juce::Rectangle<float> getChordContextRect (const MelodyChordContext& cc, int row) const;
    int hitTestNote (juce::Point<float> pos) const;
    int hitTestChordContext (juce::Point<float> pos) const;
    DragEdge hitTestChordEdge (juce::Point<float> pos, int& outIndex) const;
    double xToBeat (float x, int row) const;
    double snapBeat (double beat) const;
};
