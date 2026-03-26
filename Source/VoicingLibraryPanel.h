#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "VoicingModel.h"
#include "VoicingStatsChart.h"

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

    // Get the currently selected voicing ID (empty if none)
    juce::String getSelectedVoicingId() const;

    // Callback when selection changes
    std::function<void (const juce::String& voicingId)> onSelectionChanged;

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowClicked) override;

    AudioPluginAudioProcessor& processorRef;

    // --- Normal mode components ---
    juce::Label headerLabel;
    juce::Label recordingIndicator;
    juce::ComboBox qualityFilter;
    juce::ListBox voicingList;
    juce::TextButton recordButton { "Record" };
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
    juce::TextButton confirmSaveButton { "Save" };
    juce::TextButton confirmCancelButton { "Cancel" };

    // Recording state machine
    enum class RecordState { Idle, Waiting, Capturing, Confirming };
    RecordState recordState = RecordState::Idle;
    std::vector<int> capturedNotes;
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

    void populateConfirmFields();
};
