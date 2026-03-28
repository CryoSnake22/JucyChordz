#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProgressionModel.h"
#include "ProgressionChartComponent.h"
#include "VoicingStatsChart.h"

class AudioPluginAudioProcessor;

class ProgressionLibraryPanel : public juce::Component,
                                 private juce::ListBoxModel
{
public:
    ProgressionLibraryPanel (AudioPluginAudioProcessor& processor);

    void resized() override;
    void paint (juce::Graphics& g) override;

    void refresh();

    // Called from editor's 60Hz timer with current active notes
    void updateRecording (const std::vector<int>& activeNotes);

    // Called from editor's 60Hz timer to drive playback cursor + count-in
    void updateTimerCallback();

    juce::String getSelectedProgressionId() const;
    void refreshStatsChart();

    std::function<void (const juce::String& progressionId)> onSelectionChanged;
    bool isEditing() const;
    std::function<void()> onRecordStarted;
    std::function<void()> onEditStarted;
    void setButtonsEnabled (bool enabled);
    std::function<void (const std::vector<int>& midiNotes)> onChordPreview;
    std::function<void (const Progression& transposed)> onTransposedPreview;

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowClicked) override;

    AudioPluginAudioProcessor& processorRef;

    enum class PanelState { Idle, CountIn, Waiting, Recording, Editing, Confirming };
    PanelState panelState = PanelState::Idle;

    // --- Idle mode components ---
    juce::Label headerLabel;
    juce::Label recordingIndicator;
    juce::TextEditor searchEditor;
    juce::ListBox progressionList;
    juce::TextButton recordButton { "Record" };
    juce::TextButton playButton { "Play" };
    juce::TextButton editButton { "Edit" };
    juce::TextButton deleteButton { "Delete" };
    VoicingStatsChart statsChart;
    int statsPlayingKey = -1;

    // Filtered display list
    std::vector<Progression> displayedProgressions;
    void updateDisplayedProgressions();

    // --- Count-in state ---
    int countInBeatsElapsed = 0;
    int lastCountInBeat = -1;

    // --- Editing mode components ---
    juce::Label editHeader;
    ProgressionChartComponent editChart;
    juce::TextButton quantRawBtn { "Raw" };
    juce::TextButton quantBeatBtn { "Beat" };
    juce::TextButton quantHalfBtn { "1/2" };
    juce::TextButton quantQuarterBtn { "1/4" };
    juce::TextButton editPlayBtn { "Play" };
    juce::TextButton transposeDownBtn { "-1" };
    juce::TextButton transposeUpBtn { "+1" };
    juce::Label transposeLabel;
    juce::Label editChordLabel;
    juce::TextEditor editChordNameEditor;
    juce::Label editRootLabel;
    juce::ComboBox editRootCombo;
    juce::Label editQualityLabel;
    juce::ComboBox editQualityCombo;
    juce::TextButton editDeleteChordBtn { "Del" };
    juce::TextButton editDoneButton { "Done" };
    juce::TextButton editCancelButton { "Cancel" };

    // --- Confirming mode components ---
    juce::Label confirmHeader;
    juce::Label confirmNameLabel;
    juce::TextEditor confirmNameEditor;
    juce::Label confirmKeyLabel;
    juce::ComboBox confirmKeyCombo;
    juce::Label confirmModeLabel;
    juce::ComboBox confirmModeCombo;
    juce::TextButton confirmSaveButton { "Save" };
    juce::TextButton confirmCancelButton { "Cancel " };

    // Pending progression being built
    Progression pendingProgression;
    double currentQuantizeResolution = 1.0;

    // Chord preview playback (timer-based note-off)
    std::vector<int> chordPreviewNotes;
    int chordPreviewFrames = 0;
    static constexpr int chordPreviewHoldFrames = 30; // ~0.5s at 60Hz

    void enterIdle();
    void enterCountIn();
    void enterRecording();
    void enterEditing();
    void enterConfirming();

    void onStopRecording();
    void applyQuantize (double resolution);
    void onEditChordSelected (int index);
    void onEditDeleteChord();
    void onEditDone();
    void onConfirmSave();
    void onDelete();
    void onEditExisting();
    void onTranspose (int semitones);
    void onPlayToggle();
    void onEditPlayToggle();
    void playChordPreview (const ProgressionChord& chord);

    void setIdleModeVisible (bool v);
    void setEditModeVisible (bool v);
    void setConfirmModeVisible (bool v);

    void layoutIdleMode (juce::Rectangle<int> area);
    void layoutEditMode (juce::Rectangle<int> area);
    void layoutConfirmMode (juce::Rectangle<int> area);
};
