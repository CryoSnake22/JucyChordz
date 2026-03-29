#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "VoicingModel.h"
#include "VoicingStatsChart.h"
#include "MidiFileUtils.h"
#include "LibraryExporter.h"

// Forward declare
class AudioPluginAudioProcessor;

class VoicingLibraryPanel : public juce::Component,
                             private juce::ListBoxModel
{
public:
    VoicingLibraryPanel (AudioPluginAudioProcessor& processor);

    void resized() override;
    void paint (juce::Graphics& g) override;

    // Refresh the list from processor's library
    void refresh();

    // Called from editor's 60Hz timer with current active notes
    void updateRecording (const std::vector<int>& activeNotes);

    // Get the currently selected voicing ID (empty if none or multi-select)
    juce::String getSelectedVoicingId() const;
    std::vector<juce::String> getSelectedIds() const;
    int getSelectionCount() const;

    // Refresh the stats chart for the currently selected voicing (call during practice)
    void refreshStatsChart();

    // Callback when selection changes
    std::function<void (const juce::String& voicingId)> onSelectionChanged;
    std::function<void()> onRecordStarted;
    void setButtonsEnabled (bool enabled);

    // Callback for stats chart key preview (notes + velocities)
    std::function<void (const std::vector<int>& notes, const std::vector<int>& velocities)> onKeyPreview;

    // Public methods for external voicing creation
    void enterConfirmingWithVoicing (const Voicing& v);
    void selectVoicingById (const juce::String& id);

    // Update the folder combo box items
    void refreshFolderCombo();

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowClicked) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;

    AudioPluginAudioProcessor& processorRef;

    // --- Normal mode components ---
    juce::Label headerLabel;
    juce::Label recordingIndicator;
    juce::TextButton moreButton { "..." };
    juce::ComboBox folderCombo;
    juce::TextEditor searchEditor;
    juce::ComboBox qualityFilter;
    juce::ListBox voicingList;
    juce::TextButton recordButton { "Record" };
    juce::TextButton playButton { "Play" };
    juce::TextButton editButton { "Edit" };
    juce::TextButton deleteButton { "Delete" };
    VoicingStatsChart statsChart;

    // --- Confirmation mode components ---
    juce::Label confirmHeader;
    juce::Label confirmNameLabel;
    juce::TextEditor confirmNameEditor;
    juce::Label confirmRootLabel;
    juce::ComboBox confirmRootCombo;
    juce::Label confirmQualityLabel;
    juce::ComboBox confirmQualityCombo;
    juce::Label confirmAltLabel;
    juce::TextEditor confirmAltEditor;
    juce::TextButton confirmPlayButton { "Play" };
    juce::TextButton confirmSaveButton { "Save" };
    juce::TextButton confirmCancelButton { "Cancel" };

    // Recording state machine
    enum class RecordState { Idle, Waiting, Capturing, Confirming };
    RecordState recordState = RecordState::Idle;
    std::vector<int> capturedNotes;
    std::vector<int> capturedVelocities;
    Voicing pendingVoicing; // voicing being confirmed

    // Filtered list of voicings currently displayed
    std::vector<Voicing> displayedVoicings;

    void updateDisplayedVoicings();
    void onRecordToggle();
    void finishRecording();
    void cancelRecording();
    void onConfirmSave();
    void onDelete();

    void setNormalModeVisible (bool visible);
    void setConfirmModeVisible (bool visible);
    void layoutNormalMode (juce::Rectangle<int> area);
    void layoutConfirmMode (juce::Rectangle<int> area);

    std::unique_ptr<juce::FileChooser> fileChooser;
    void populateConfirmFields();
    void showMoreMenu();
    void showContextMenu (int rowIndex);
    void moveSelectedToFolder (const juce::String& folderId);
    juce::PopupMenu buildFolderSubmenu (int baseId);
    void handleFolderSubmenuResult (int result, int baseId);
};
