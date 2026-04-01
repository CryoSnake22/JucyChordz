#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MelodyModel.h"
#include "MidiFileUtils.h"
#include "LibraryExporter.h"
#include "MelodyChartComponent.h"
#include "VoicingStatsChart.h"
#include "AccuracyTimeChart.h"

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
    std::vector<juce::String> getSelectedIds() const;
    int getSelectionCount() const;
    void refreshStatsChart();
    void refreshAccuracyChart();
    void setDrillStatus (bool active, int mastered, int total, int bpmLevel, float startBpm);

    std::function<void (const juce::String& melodyId)> onSelectionChanged;
    bool isEditing() const;
    void togglePlay();  // spacebar: play/stop in idle or edit mode
    std::function<void()> onRecordStarted;
    std::function<void()> onEditStarted;
    void setButtonsEnabled (bool enabled);
    std::function<void (const std::vector<int>& midiNotes)> onNotePreview;
    std::function<void (const Melody& transposed)> onTransposedPreview;

    void refreshFolderCombo();

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowClicked) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;

    AudioPluginAudioProcessor& processorRef;

    enum class PanelState { Idle, CountIn, Recording, Editing, Confirming };
    PanelState panelState = PanelState::Idle;

    // --- Idle mode ---
    juce::Label headerLabel;
    juce::Label recordingIndicator;
    juce::TextButton moreButton { "..." };
    juce::ComboBox folderCombo;
    juce::TextEditor searchEditor;
    juce::ListBox melodyList;
    MelodyChartComponent chartPreview;
    juce::TextButton recordButton { "Record" };
    juce::TextButton playButton { "Play" };
    juce::TextButton editButton { "Edit" };
    juce::TextButton deleteButton { "Delete" };
    VoicingStatsChart statsChart;
    AccuracyTimeChart accuracyChart;
    juce::TextButton statsToggleButton { "Stats" };
    bool showingStatsView = false;
    juce::Label drillStatusLabel;
    bool practiceActive = false;
    bool drillActive = false;
    int drillMastered = 0;
    int drillTotal = 12;
    int drillBpmLvl = 0;
    float drillStartBpmVal = 120.0f;
    int statsPlayingKey = -1;

    // --- Count-in ---
    int countInBeatsElapsed = 0;
    int lastCountInBeat = -1;

    // --- Editing mode ---
    juce::Label editHeader;
    juce::Viewport editChartViewport;
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

    // Filtered display list
    std::vector<Melody> displayedMelodies;
    void updateDisplayedMelodies();

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

    std::unique_ptr<juce::FileChooser> fileChooser;
    void setIdleModeVisible (bool v);
    void setEditModeVisible (bool v);
    void setConfirmModeVisible (bool v);

    void showMoreMenu();
    void showContextMenu (int rowIndex);
    void moveSelectedToFolder (const juce::String& folderId);
    juce::PopupMenu buildFolderSubmenu (int baseId);
    void handleFolderSubmenuResult (int result, int baseId);

    void layoutIdleMode (juce::Rectangle<int> area);
    void layoutEditMode (juce::Rectangle<int> area);
    void layoutConfirmMode (juce::Rectangle<int> area);
};
