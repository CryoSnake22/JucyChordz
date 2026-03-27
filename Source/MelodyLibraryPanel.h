#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MelodyModel.h"
#include "MelodyChartComponent.h"
#include "VoicingStatsChart.h"

class AudioPluginAudioProcessor;

class MelodyLibraryPanel : public juce::Component,
                            private juce::ListBoxModel
{
public:
    MelodyLibraryPanel (AudioPluginAudioProcessor& processor);

    void resized() override;
    void paint (juce::Graphics& g) override;

    void refresh();

    void updateRecording (const std::vector<int>& activeNotes);
    void updateTimerCallback();

    juce::String getSelectedMelodyId() const;
    void refreshStatsChart();

    std::function<void (const juce::String& melodyId)> onSelectionChanged;
    std::function<void (const std::vector<int>& midiNotes)> onNotePreview;

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowClicked) override;

    AudioPluginAudioProcessor& processorRef;

    enum class PanelState { Idle, CountIn, Recording, Editing, Confirming };
    PanelState panelState = PanelState::Idle;

    // --- Idle mode ---
    juce::Label headerLabel;
    juce::Label recordingIndicator;
    juce::ListBox melodyList;
    MelodyChartComponent chartPreview;
    juce::TextButton recordButton { "Record" };
    juce::TextButton playButton { "Play" };
    juce::TextButton editButton { "Edit" };
    juce::TextButton deleteButton { "Delete" };
    VoicingStatsChart statsChart;
    int statsPlayingKey = -1;

    // --- Count-in ---
    int countInBeatsElapsed = 0;
    int lastCountInBeat = -1;

    // --- Editing mode ---
    juce::Label editHeader;
    MelodyChartComponent editChart;
    juce::TextButton quantRawBtn { "Raw" };
    juce::TextButton quantBeatBtn { "Beat" };
    juce::TextButton quantHalfBtn { "1/2" };
    juce::TextButton quantQuarterBtn { "1/4" };
    juce::TextButton transposeDownBtn { "-1" };
    juce::TextButton transposeUpBtn { "+1" };
    juce::Label transposeLabel;

    // Chord context editing
    juce::TextButton addChordBtn { "Add Chord" };
    juce::TextButton delChordBtn { "Del" };
    juce::Label ccRootLabel;
    juce::ComboBox ccRootCombo;
    juce::Label ccQualityLabel;
    juce::ComboBox ccQualityCombo;
    juce::TextEditor ccAlterationsEditor;

    juce::TextButton editPlayButton { "Play" };
    juce::TextButton editDoneButton { "Done" };
    juce::TextButton editCancelButton { "Cancel" };

    // --- Confirming mode ---
    juce::Label confirmHeader;
    juce::Label confirmNameLabel;
    juce::TextEditor confirmNameEditor;
    juce::Label confirmKeyLabel;
    juce::ComboBox confirmKeyCombo;
    juce::TextButton confirmSaveButton { "Save" };
    juce::TextButton confirmCancelButton { "Cancel " };

    // Pending melody being built
    Melody pendingMelody;
    double currentQuantizeResolution = 0.5;
    int analysisKeyRootMidi = 60;   // C4 as default reference for analysis

    // Note preview (timer-based note-off)
    std::vector<int> notePreviewNotes;
    int notePreviewFrames = 0;
    static constexpr int notePreviewHoldFrames = 30;

    void enterIdle();
    void enterCountIn();
    void enterRecording();
    void enterEditing();
    void enterConfirming();

    void onStopRecording();
    void applyQuantize (double resolution);
    void onEditDone();
    void onConfirmSave();
    void onDelete();
    void onEditExisting();
    void onTranspose (int semitones);
    void onPlayToggle();
    void onEditPlayToggle();

    // Chord context editing
    void onAddChordContext();
    void onDeleteChordContext();
    void onChordContextSelected (int index);
    void updateChordContextFromCombos();

    void setIdleModeVisible (bool v);
    void setEditModeVisible (bool v);
    void setConfirmModeVisible (bool v);

    void layoutIdleMode (juce::Rectangle<int> area);
    void layoutEditMode (juce::Rectangle<int> area);
    void layoutConfirmMode (juce::Rectangle<int> area);
};
