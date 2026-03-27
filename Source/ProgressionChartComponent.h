#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProgressionModel.h"
#include <map>

class ProgressionChartComponent : public juce::Component
{
public:
    ProgressionChartComponent();

    // NOTE: setProgression takes a non-const pointer in edit mode so drags can modify chords
    void setProgression (Progression* prog);
    void setProgressionReadOnly (const Progression* prog);
    void setCursorBeat (double beat);
    void setSelectedChord (int index);
    void setEditMode (bool enabled);
    void setQuantizeGrid (double resolution) { quantizeGrid = resolution; }

    int getSelectedChord() const { return selectedChordIndex; }
    int getIdealHeight() const;

    // Detailed (piano-roll) vs simple (chord names) view
    void setDetailedView (bool enabled);
    bool isDetailedView() const { return detailedView; }

    // Per-chord per-note state tracking for practice feedback
    enum class NoteState { Default, Target, Correct, Missed };
    void setNoteState (int chordIndex, int noteIndex, NoteState state);
    void setAllChordNoteStates (int chordIndex, NoteState state);
    void clearNoteStates();

    std::function<void (int chordIndex)> onChordSelected;
    std::function<void()> onChordResized;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    Progression* progression = nullptr;
    const Progression* progressionReadOnly = nullptr;
    const Progression* getProgression() const { return progression != nullptr ? progression : progressionReadOnly; }

    double cursorBeat = -1.0;
    int selectedChordIndex = -1;
    bool editMode = false;
    bool detailedView = true;
    double quantizeGrid = 1.0;

    // Per-note states: key = {chordIndex, noteIndexWithinChord}
    std::map<std::pair<int,int>, NoteState> noteStates;

    // Drag state
    enum class DragEdge { None, Left, Right, EndMarker };
    DragEdge dragEdge = DragEdge::None;
    int dragChordIndex = -1;
    double dragOrigStartBeat = 0.0;
    double dragOrigDuration = 0.0;

    static constexpr int beatsPerRow = 16;
    static constexpr int simpleRowHeight = 30;
    static constexpr int rowGap = 3;
    static constexpr int leftPad = 4;
    static constexpr int rightPad = 4;
    static constexpr float edgeHitZone = 6.0f;

    // Detailed view layout
    static constexpr int detailedNoteAreaHeight = 100;
    static constexpr int detailedChordLabelHeight = 22;

    int getEffectiveRowHeight() const;
    float getBeatWidth() const;
    int getRowForBeat (double beat) const;
    juce::Rectangle<float> getChordRect (const ProgressionChord& chord, int row) const;
    int hitTestChord (juce::Point<float> pos) const;
    DragEdge hitTestEdge (juce::Point<float> pos, int& outChordIndex) const;
    double xToBeat (float x, int row) const;
    double snapBeat (double beat) const;

    // Detailed view helpers
    void computePitchRange (int& minMidi, int& maxMidi) const;
    float midiToY (int midiNote, int minMidi, int maxMidi, int row) const;
    juce::Rectangle<float> getNoteDetailRect (int midiNote, double startBeat,
        double durationBeats, int minMidi, int maxMidi, int row) const;

    void paintSimple (juce::Graphics& g, const Progression* prog);
    void paintDetailed (juce::Graphics& g, const Progression* prog);
};
