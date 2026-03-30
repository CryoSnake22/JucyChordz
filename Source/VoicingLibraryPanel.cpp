#include "VoicingLibraryPanel.h"
#include "PluginProcessor.h"
#include "ChordDetector.h"
#include "ChordyTheme.h"
#include "ExportSheetMusicDialog.h"

static void addQualityItems (juce::ComboBox& combo)
{
    combo.addItem ("Major",      1);
    combo.addItem ("Minor",      2);
    combo.addItem ("Dom7",       3);
    combo.addItem ("Maj7",       4);
    combo.addItem ("Min7",       5);
    combo.addItem ("Dim",        6);
    combo.addItem ("Dim7",       7);
    combo.addItem ("Aug",        8);
    combo.addItem ("HalfDim7",   9);
    combo.addItem ("MinMaj7",   10);
    combo.addItem ("Maj6",      11);
    combo.addItem ("Min6",      12);
    combo.addItem ("Sus2",      13);
    combo.addItem ("Sus4",      14);
    combo.addItem ("N/A",       15);
}

static ChordQuality qualityFromComboId (int id)
{
    switch (id)
    {
        case 1:  return ChordQuality::Major;
        case 2:  return ChordQuality::Minor;
        case 3:  return ChordQuality::Dom7;
        case 4:  return ChordQuality::Maj7;
        case 5:  return ChordQuality::Min7;
        case 6:  return ChordQuality::Diminished;
        case 7:  return ChordQuality::Dim7;
        case 8:  return ChordQuality::Augmented;
        case 9:  return ChordQuality::HalfDim7;
        case 10: return ChordQuality::MinMaj7;
        case 11: return ChordQuality::Maj6;
        case 12: return ChordQuality::Min6;
        case 13: return ChordQuality::Sus2;
        case 14: return ChordQuality::Sus4;
        case 15: return ChordQuality::Unknown;
        default: return ChordQuality::Unknown;
    }
}

static int comboIdFromQuality (ChordQuality q)
{
    switch (q)
    {
        case ChordQuality::Major:       return 1;
        case ChordQuality::Minor:       return 2;
        case ChordQuality::Dom7:        return 3;
        case ChordQuality::Maj7:        return 4;
        case ChordQuality::Min7:        return 5;
        case ChordQuality::Diminished:  return 6;
        case ChordQuality::Dim7:        return 7;
        case ChordQuality::Augmented:   return 8;
        case ChordQuality::HalfDim7:    return 9;
        case ChordQuality::MinMaj7:     return 10;
        case ChordQuality::Maj6:        return 11;
        case ChordQuality::Min6:        return 12;
        case ChordQuality::Sus2:        return 13;
        case ChordQuality::Sus4:        return 14;
        default:                        return 15; // N/A
    }
}

VoicingLibraryPanel::VoicingLibraryPanel (AudioPluginAudioProcessor& processor)
    : processorRef (processor)
{
    // --- Normal mode ---
    headerLabel.setText ("VOICING LIBRARY", juce::dontSendNotification);
    headerLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addAndMakeVisible (headerLabel);

    recordingIndicator.setText ("REC", juce::dontSendNotification);
    recordingIndicator.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    recordingIndicator.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
    recordingIndicator.setJustificationType (juce::Justification::centredRight);
    recordingIndicator.setVisible (false);
    addAndMakeVisible (recordingIndicator);

    moreButton.onClick = [this] { showMoreMenu(); };
    addAndMakeVisible (moreButton);

    folderCombo.onChange = [this] { updateDisplayedVoicings(); };
    addAndMakeVisible (folderCombo);
    refreshFolderCombo();

    qualityFilter.addItem ("All", 1);
    qualityFilter.addItem ("Major", 2);
    qualityFilter.addItem ("Minor", 3);
    qualityFilter.addItem ("Dom7", 4);
    qualityFilter.addItem ("Maj7", 5);
    qualityFilter.addItem ("Min7", 6);
    qualityFilter.addItem ("Dim", 7);
    qualityFilter.addItem ("Other", 8);
    qualityFilter.setSelectedId (1);
    qualityFilter.onChange = [this] { updateDisplayedVoicings(); };
    addAndMakeVisible (qualityFilter);

    searchEditor.setTextToShowWhenEmpty ("Search...", juce::Colour (ChordyTheme::textTertiary));
    searchEditor.onTextChange = [this] { updateDisplayedVoicings(); };
    addAndMakeVisible (searchEditor);

    voicingList.setModel (this);
    voicingList.setMultipleSelectionEnabled (true);
    voicingList.setOutlineThickness (1);
    voicingList.setRowHeight (36);
    addAndMakeVisible (voicingList);

    statsChart.onKeyClicked = [this](int keyIndex) {
        auto id = getSelectedVoicingId();
        if (id.isEmpty()) return;
        const auto* v = processorRef.voicingLibrary.getVoicing (id);
        if (v == nullptr) return;

        // Show yellow highlight on clicked bar
        statsChart.setPlayingKey (keyIndex);

        // Transpose to clicked key, staying near original octave
        int semitoneShift = keyIndex - v->rootPitchClass;
        if (semitoneShift < -6) semitoneShift += 12;
        if (semitoneShift > 6) semitoneShift -= 12;
        int rootMidi = v->octaveReference + semitoneShift;

        auto notes = VoicingLibrary::transposeToKey (*v, rootMidi);
        if (onKeyPreview)
            onKeyPreview (notes, v->velocities);
    };
    addAndMakeVisible (statsChart);

    recordButton.onClick = [this] { onRecordToggle(); };
    addAndMakeVisible (recordButton);

    playButton.onClick = [this] {
        auto id = getSelectedVoicingId();
        if (id.isEmpty()) return;
        const auto* v = processorRef.voicingLibrary.getVoicing (id);
        if (v == nullptr) return;
        auto notes = VoicingLibrary::transposeToKey (*v, v->octaveReference);
        if (onKeyPreview)
            onKeyPreview (notes, v->velocities);
    };
    addAndMakeVisible (playButton);

    editButton.onClick = [this] {
        auto id = getSelectedVoicingId();
        if (id.isEmpty()) return;
        const auto* v = processorRef.voicingLibrary.getVoicing (id);
        if (v == nullptr) return;
        enterConfirmingWithVoicing (*v);
    };
    addAndMakeVisible (editButton);

    deleteButton.onClick = [this] { onDelete(); };
    deleteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    addAndMakeVisible (deleteButton);

    // --- Confirmation mode ---
    auto labelStyle = [](juce::Label& l) {
        l.setFont (juce::FontOptions (ChordyTheme::fontBody));
        l.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textSecondary));
    };

    confirmHeader.setText ("CONFIRM VOICING", juce::dontSendNotification);
    confirmHeader.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    confirmHeader.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addChildComponent (confirmHeader);

    confirmNameLabel.setText ("Name:", juce::dontSendNotification);
    labelStyle (confirmNameLabel);
    addChildComponent (confirmNameLabel);

    confirmNameEditor.setTextToShowWhenEmpty ("Voicing name...", juce::Colour (ChordyTheme::textTertiary));
    addChildComponent (confirmNameEditor);

    confirmRootLabel.setText ("Root:", juce::dontSendNotification);
    labelStyle (confirmRootLabel);
    addChildComponent (confirmRootLabel);

    for (int i = 0; i < 12; ++i)
        confirmRootCombo.addItem (ChordDetector::noteNameFromPitchClass (i), i + 1);
    addChildComponent (confirmRootCombo);

    confirmQualityLabel.setText ("Quality:", juce::dontSendNotification);
    labelStyle (confirmQualityLabel);
    addChildComponent (confirmQualityLabel);

    addQualityItems (confirmQualityCombo);
    addChildComponent (confirmQualityCombo);

    confirmAltLabel.setText ("Alterations:", juce::dontSendNotification);
    labelStyle (confirmAltLabel);
    addChildComponent (confirmAltLabel);

    confirmAltEditor.setTextToShowWhenEmpty ("#9#11b5 etc. (optional)", juce::Colour (ChordyTheme::textTertiary));
    addChildComponent (confirmAltEditor);

    confirmPlayButton.onClick = [this] {
        auto notes = VoicingLibrary::transposeToKey (pendingVoicing, pendingVoicing.octaveReference);
        if (onKeyPreview)
            onKeyPreview (notes, pendingVoicing.velocities);
    };
    addChildComponent (confirmPlayButton);

    confirmSaveButton.onClick = [this] { onConfirmSave(); };
    addChildComponent (confirmSaveButton);

    confirmCancelButton.onClick = [this] { cancelRecording(); };
    addChildComponent (confirmCancelButton);

    updateDisplayedVoicings();
}

void VoicingLibraryPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (ChordyTheme::bgSurface));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), ChordyTheme::cornerRadius);

    if (recordState == RecordState::Waiting || recordState == RecordState::Capturing)
    {
        g.setColour (juce::Colour (ChordyTheme::danger));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), ChordyTheme::cornerRadius, 2.0f);
    }
    else if (recordState == RecordState::Confirming)
    {
        g.setColour (juce::Colour (ChordyTheme::accent));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), ChordyTheme::cornerRadius, 2.0f);
    }
}

void VoicingLibraryPanel::setNormalModeVisible (bool visible)
{
    headerLabel.setVisible (visible);
    moreButton.setVisible (visible);
    folderCombo.setVisible (visible);
    searchEditor.setVisible (visible);
    qualityFilter.setVisible (visible);
    voicingList.setVisible (visible);
    recordButton.setVisible (visible);
    playButton.setVisible (visible);
    editButton.setVisible (visible);
    deleteButton.setVisible (visible);
}

void VoicingLibraryPanel::setConfirmModeVisible (bool visible)
{
    confirmHeader.setVisible (visible);
    confirmNameLabel.setVisible (visible);
    confirmNameEditor.setVisible (visible);
    confirmRootLabel.setVisible (visible);
    confirmRootCombo.setVisible (visible);
    confirmQualityLabel.setVisible (visible);
    confirmQualityCombo.setVisible (visible);
    confirmAltLabel.setVisible (visible);
    confirmAltEditor.setVisible (visible);
    confirmPlayButton.setVisible (visible);
    confirmSaveButton.setVisible (visible);
    confirmCancelButton.setVisible (visible);
}

void VoicingLibraryPanel::resized()
{
    auto area = getLocalBounds().reduced (ChordyTheme::panelPadding);

    if (recordState == RecordState::Confirming)
        layoutConfirmMode (area);
    else
        layoutNormalMode (area);
}

void VoicingLibraryPanel::layoutNormalMode (juce::Rectangle<int> area)
{
    auto headerRow = area.removeFromTop (24);
    moreButton.setBounds (headerRow.removeFromRight (28));
    headerRow.removeFromRight (4);
    recordingIndicator.setBounds (headerRow.removeFromRight (headerRow.getWidth() / 2));
    headerLabel.setBounds (headerRow);
    area.removeFromTop (4);

    auto folderRow = area.removeFromTop (24);
    folderCombo.setBounds (folderRow);
    area.removeFromTop (4);

    auto searchRow = area.removeFromTop (24);
    searchEditor.setBounds (searchRow);
    area.removeFromTop (4);

    auto filterRow = area.removeFromTop (28);
    qualityFilter.setBounds (filterRow);
    area.removeFromTop (4);

    auto bottomRow = area.removeFromBottom (30);
    deleteButton.setBounds (bottomRow.removeFromRight (60));
    bottomRow.removeFromRight (4);
    editButton.setBounds (bottomRow.removeFromRight (45));
    bottomRow.removeFromRight (4);
    recordButton.setBounds (bottomRow.removeFromRight (70));
    bottomRow.removeFromRight (4);
    playButton.setBounds (bottomRow.removeFromRight (50));
    area.removeFromBottom (4);

    // Stats chart between list and buttons
    auto chartArea = area.removeFromBottom (60);
    area.removeFromBottom (4);
    statsChart.setBounds (chartArea);

    voicingList.setBounds (area);
}

void VoicingLibraryPanel::layoutConfirmMode (juce::Rectangle<int> area)
{
    confirmHeader.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto row = [&]() -> juce::Rectangle<int> {
        auto r = area.removeFromTop (26);
        area.removeFromTop (4);
        return r;
    };

    auto nameRow = row();
    confirmNameLabel.setBounds (nameRow.removeFromLeft (80));
    confirmNameEditor.setBounds (nameRow);

    auto rootRow = row();
    confirmRootLabel.setBounds (rootRow.removeFromLeft (80));
    confirmRootCombo.setBounds (rootRow);

    auto qualRow = row();
    confirmQualityLabel.setBounds (qualRow.removeFromLeft (80));
    confirmQualityCombo.setBounds (qualRow);

    auto altRow = row();
    confirmAltLabel.setBounds (altRow.removeFromLeft (80));
    confirmAltEditor.setBounds (altRow);

    area.removeFromTop (8);
    auto buttonRow = area.removeFromTop (30);
    int bw = (buttonRow.getWidth() - 16) / 3;
    confirmPlayButton.setBounds (buttonRow.removeFromLeft (bw));
    buttonRow.removeFromLeft (8);
    confirmSaveButton.setBounds (buttonRow.removeFromLeft (bw));
    buttonRow.removeFromLeft (8);
    confirmCancelButton.setBounds (buttonRow);
}

void VoicingLibraryPanel::refresh()
{
    refreshFolderCombo();
    updateDisplayedVoicings();
}

void VoicingLibraryPanel::updateRecording (const std::vector<int>& activeNotes)
{
    if (recordState == RecordState::Idle || recordState == RecordState::Confirming)
        return;

    if (recordState == RecordState::Waiting)
    {
        if (! activeNotes.empty())
        {
            recordState = RecordState::Capturing;
            capturedNotes.clear();
            capturedVelocities.clear();
            for (int n : activeNotes)
            {
                capturedNotes.push_back (n);
                capturedVelocities.push_back (processorRef.getNoteVelocity (n));
            }
            recordingIndicator.setText ("REC - Playing...", juce::dontSendNotification);
        }
        return;
    }

    if (recordState == RecordState::Capturing)
    {
        if (! activeNotes.empty())
        {
            // Accumulate: add any new notes not already captured
            for (int n : activeNotes)
            {
                if (std::find (capturedNotes.begin(), capturedNotes.end(), n) == capturedNotes.end())
                {
                    capturedNotes.push_back (n);
                    capturedVelocities.push_back (processorRef.getNoteVelocity (n));
                }
            }
        }
        else
        {
            // All notes released — finish with the full accumulated set
            finishRecording();
        }
    }
}

juce::String VoicingLibraryPanel::getSelectedVoicingId() const
{
    if (voicingList.getNumSelectedRows() != 1)
        return {};
    int row = voicingList.getSelectedRow();
    if (row >= 0 && row < static_cast<int> (displayedVoicings.size()))
        return displayedVoicings[static_cast<size_t> (row)].id;
    return {};
}

std::vector<juce::String> VoicingLibraryPanel::getSelectedIds() const
{
    std::vector<juce::String> ids;
    auto rows = voicingList.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
    {
        int row = rows[i];
        if (row >= 0 && row < static_cast<int> (displayedVoicings.size()))
            ids.push_back (displayedVoicings[static_cast<size_t> (row)].id);
    }
    return ids;
}

int VoicingLibraryPanel::getSelectionCount() const
{
    return voicingList.getNumSelectedRows();
}

void VoicingLibraryPanel::refreshStatsChart()
{
    auto selectedId = getSelectedVoicingId();
    if (selectedId.isNotEmpty())
        statsChart.setStats (processorRef.spacedRepetition.getStatsForVoicing (selectedId));
}

void VoicingLibraryPanel::setButtonsEnabled (bool enabled)
{
    recordButton.setEnabled (enabled);
    playButton.setEnabled (enabled);
    editButton.setEnabled (enabled);
    deleteButton.setEnabled (enabled);
    voicingList.setEnabled (enabled);
    moreButton.setEnabled (enabled);
    searchEditor.setEnabled (enabled);
    folderCombo.setEnabled (enabled);
    qualityFilter.setEnabled (enabled);
    statsChart.setInterceptsMouseClicks (enabled, enabled);
}

void VoicingLibraryPanel::updateDisplayedVoicings()
{
    int filterId = qualityFilter.getSelectedId();
    const auto& all = processorRef.voicingLibrary.getAllVoicings();

    displayedVoicings.clear();

    // Step 1: Start with all items
    if (filterId == 1)
    {
        displayedVoicings = all;
    }
    else
    {
        ChordQuality filterQuality = ChordQuality::Unknown;
        switch (filterId)
        {
            case 2: filterQuality = ChordQuality::Major; break;
            case 3: filterQuality = ChordQuality::Minor; break;
            case 4: filterQuality = ChordQuality::Dom7; break;
            case 5: filterQuality = ChordQuality::Maj7; break;
            case 6: filterQuality = ChordQuality::Min7; break;
            case 7: filterQuality = ChordQuality::Diminished; break;
            default: break;
        }

        for (const auto& v : all)
        {
            if (filterId == 8)
            {
                if (v.quality != ChordQuality::Major && v.quality != ChordQuality::Minor &&
                    v.quality != ChordQuality::Dom7 && v.quality != ChordQuality::Maj7 &&
                    v.quality != ChordQuality::Min7 && v.quality != ChordQuality::Diminished)
                    displayedVoicings.push_back (v);
            }
            else if (v.quality == filterQuality)
            {
                displayedVoicings.push_back (v);
            }
        }
    }

    // Step 2: Filter by folder
    int folderId = folderCombo.getSelectedId();
    if (folderId == 2) // "Unfiled"
    {
        std::vector<Voicing> filtered;
        for (const auto& v : displayedVoicings)
            if (v.folderId.isEmpty())
                filtered.push_back (v);
        displayedVoicings = std::move (filtered);
    }
    else if (folderId >= 3) // Named folder
    {
        auto folderIdStr = folderCombo.getItemText (folderCombo.indexOfItemId (folderId));
        // Map combo ID to folder UUID: IDs 3+ correspond to folders in order
        const auto& folders = processorRef.voicingLibrary.getFolders().getAllFolders();
        int folderIndex = folderId - 3;
        if (folderIndex >= 0 && folderIndex < static_cast<int> (folders.size()))
        {
            auto targetFolderId = folders[static_cast<size_t> (folderIndex)].id;
            std::vector<Voicing> filtered;
            for (const auto& v : displayedVoicings)
                if (v.folderId == targetFolderId)
                    filtered.push_back (v);
            displayedVoicings = std::move (filtered);
        }
    }
    // folderId == 1 ("All Items") = no folder filter

    // Step 3: Filter by search text
    auto searchText = searchEditor.getText().trim().toLowerCase();
    if (searchText.isNotEmpty())
    {
        std::vector<Voicing> filtered;
        for (const auto& v : displayedVoicings)
            if (v.name.toLowerCase().contains (searchText))
                filtered.push_back (v);
        displayedVoicings = std::move (filtered);
    }

    voicingList.updateContent();
    voicingList.repaint();
}

void VoicingLibraryPanel::onRecordToggle()
{
    if (recordState != RecordState::Idle)
    {
        cancelRecording();
        return;
    }

    if (onRecordStarted)
        onRecordStarted();

    recordState = RecordState::Waiting;
    capturedNotes.clear();
    recordButton.setButtonText ("Cancel");
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    recordingIndicator.setText ("REC - Play a chord...", juce::dontSendNotification);
    recordingIndicator.setVisible (true);
    repaint();
}

void VoicingLibraryPanel::finishRecording()
{
    if (capturedNotes.empty())
    {
        cancelRecording();
        return;
    }

    // Create pending voicing and transition to confirmation
    pendingVoicing = VoicingLibrary::createFromNotes (capturedNotes, "", capturedVelocities);
    capturedNotes.clear();
    capturedVelocities.clear();

    recordState = RecordState::Confirming;
    recordingIndicator.setVisible (false);

    // Hide normal UI, show confirm UI
    setNormalModeVisible (false);
    setConfirmModeVisible (true);
    populateConfirmFields();
    resized();
    repaint();
}

void VoicingLibraryPanel::enterConfirmingWithVoicing (const Voicing& v)
{
    pendingVoicing = v;
    recordState = RecordState::Confirming;
    recordingIndicator.setVisible (false);

    setNormalModeVisible (false);
    setConfirmModeVisible (true);
    populateConfirmFields();
    resized();
    repaint();
}

void VoicingLibraryPanel::selectVoicingById (const juce::String& id)
{
    updateDisplayedVoicings();
    for (int i = 0; i < static_cast<int> (displayedVoicings.size()); ++i)
    {
        if (displayedVoicings[static_cast<size_t> (i)].id == id)
        {
            voicingList.selectRow (i);
            return;
        }
    }
}

void VoicingLibraryPanel::populateConfirmFields()
{
    // If editing an existing voicing (has a name), preserve its fields
    bool isEdit = pendingVoicing.name.isNotEmpty();

    if (isEdit)
    {
        confirmNameEditor.setText (pendingVoicing.name);
        confirmAltEditor.setText (pendingVoicing.alterations);
    }
    else
    {
        // New voicing — auto-detect for pre-filling
        auto notes = VoicingLibrary::transposeToKey (pendingVoicing, pendingVoicing.octaveReference);
        auto detected = ChordDetector::detect (notes);
        if (detected.isValid())
            confirmNameEditor.setText (detected.displayName + " voicing");
        else
            confirmNameEditor.setText ("Voicing " + juce::String (processorRef.voicingLibrary.size() + 1));
        confirmAltEditor.clear();
    }

    // Pre-fill root and quality (always)
    confirmRootCombo.setSelectedId (pendingVoicing.rootPitchClass + 1, juce::dontSendNotification);
    confirmQualityCombo.setSelectedId (comboIdFromQuality (pendingVoicing.quality), juce::dontSendNotification);
}

void VoicingLibraryPanel::onConfirmSave()
{
    // Read user's choices
    juce::String name = confirmNameEditor.getText().trim();
    if (name.isEmpty())
        name = "Voicing " + juce::String (processorRef.voicingLibrary.size() + 1);

    pendingVoicing.name = name;
    pendingVoicing.rootPitchClass = confirmRootCombo.getSelectedId() - 1;
    pendingVoicing.quality = qualityFromComboId (confirmQualityCombo.getSelectedId());
    pendingVoicing.alterations = confirmAltEditor.getText().trim();

    auto savedId = pendingVoicing.id;
    // Remove old version if editing an existing voicing
    processorRef.voicingLibrary.removeVoicing (savedId);
    processorRef.voicingLibrary.addVoicing (pendingVoicing);
    processorRef.saveLibrariesToDisk();
    pendingVoicing = {};

    // Return to normal mode
    recordState = RecordState::Idle;
    setConfirmModeVisible (false);
    setNormalModeVisible (true);
    recordButton.setButtonText ("Record");
    recordButton.setColour (juce::TextButton::buttonColourId,
                            getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    resized();
    repaint();
    updateDisplayedVoicings();

    // Select and scroll to the newly saved voicing
    selectVoicingById (savedId);
}

void VoicingLibraryPanel::cancelRecording()
{
    bool wasConfirming = (recordState == RecordState::Confirming);

    recordState = RecordState::Idle;
    capturedNotes.clear();
    pendingVoicing = {};
    recordButton.setButtonText ("Record");
    recordButton.setColour (juce::TextButton::buttonColourId,
                            getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    recordingIndicator.setVisible (false);

    if (wasConfirming)
    {
        setConfirmModeVisible (false);
        setNormalModeVisible (true);
        resized();
    }

    repaint();
}

void VoicingLibraryPanel::onDelete()
{
    auto ids = getSelectedIds();
    if (ids.empty()) return;

    for (const auto& id : ids)
        processorRef.voicingLibrary.removeVoicing (id);
    processorRef.saveLibrariesToDisk();
    updateDisplayedVoicings();
}

// --- ListBoxModel ---

int VoicingLibraryPanel::getNumRows()
{
    return static_cast<int> (displayedVoicings.size());
}

void VoicingLibraryPanel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                             int width, int height,
                                             bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int> (displayedVoicings.size()))
        return;

    const auto& v = displayedVoicings[static_cast<size_t> (rowNumber)];

    if (rowIsSelected)
        g.fillAll (juce::Colour (ChordyTheme::bgSelected));
    else
        g.fillAll (juce::Colour (ChordyTheme::bgSurface));

    g.setColour (juce::Colour (ChordyTheme::textPrimary));
    g.setFont (15.0f);
    g.drawText (v.name, 8, 2, width - 80, 18, juce::Justification::centredLeft);

    // Date + quality on second line
    g.setColour (juce::Colour (ChordyTheme::textSecondary));
    g.setFont (12.0f);
    juce::String dateStr = v.createdAt > 0
        ? juce::Time (v.createdAt).formatted ("%b %d, %Y")
        : "";
    g.drawText (dateStr, 8, 18, width / 2, 14, juce::Justification::centredLeft);
    g.drawText (v.getQualityLabel(),
                width - 70, 0, 62, height, juce::Justification::centredRight);
}

void VoicingLibraryPanel::selectedRowsChanged (int /*lastRowClicked*/)
{
    int count = getSelectionCount();
    auto selectedId = getSelectedVoicingId(); // empty if count != 1

    if (selectedId.isNotEmpty())
        statsChart.setStats (processorRef.spacedRepetition.getStatsForVoicing (selectedId));
    else
        statsChart.clearStats();

    // Single-item operations disabled during multi-select
    playButton.setEnabled (count == 1);
    editButton.setEnabled (count == 1);

    if (onSelectionChanged)
        onSelectionChanged (selectedId);
}

void VoicingLibraryPanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu (row);
}

// --- Folder & Menu ---

void VoicingLibraryPanel::refreshFolderCombo()
{
    int currentId = folderCombo.getSelectedId();
    folderCombo.clear (juce::dontSendNotification);
    folderCombo.addItem ("All Items", 1);
    folderCombo.addItem ("Unfiled", 2);

    const auto& folders = processorRef.voicingLibrary.getFolders().getAllFolders();
    for (int i = 0; i < static_cast<int> (folders.size()); ++i)
        folderCombo.addItem (folders[static_cast<size_t> (i)].name, i + 3);

    if (currentId > 0 && folderCombo.indexOfItemId (currentId) >= 0)
        folderCombo.setSelectedId (currentId, juce::dontSendNotification);
    else
        folderCombo.setSelectedId (1, juce::dontSendNotification);
}

void VoicingLibraryPanel::showMoreMenu()
{
    juce::PopupMenu menu;
    menu.addItem (10, "Import MIDI...");
    menu.addItem (11, "Export as MIDI...", getSelectionCount() == 1);
    menu.addSeparator();
    menu.addItem (20, "Import Library (.chordy)...");
    menu.addItem (21, "Export Selected (.chordy)...", getSelectionCount() > 0);
    menu.addItem (22, "Export All Voicings (.chordy)...");
    menu.addSeparator();
    menu.addItem (30, "Export Sheet Music (PDF) [Experimental]...", getSelectionCount() == 1);
    menu.addSeparator();
    menu.addItem (1, "New Folder...");

    int folderId = folderCombo.getSelectedId();
    bool viewingFolder = (folderId >= 3);
    menu.addItem (2, "Rename Folder...", viewingFolder);
    menu.addItem (3, "Delete Folder", viewingFolder);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
        [this] (int result)
        {
            if (result == 10) // Import MIDI
            {
                juce::MessageManager::callAsync ([this] {
                    fileChooser = std::make_unique<juce::FileChooser> (
                        "Import MIDI file...",
                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                        "*.mid;*.midi");
                    auto flags = juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles;
                    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (! f.existsAsFile()) return;
                        auto imported = MidiFileUtils::importMidiFile (f);
                        if (! imported.success || imported.tracks.empty()) return;
                        // Use first track with notes
                        auto voicing = MidiFileUtils::midiToVoicing (imported.tracks[0]);
                        voicing.name = f.getFileNameWithoutExtension();
                        enterConfirmingWithVoicing (voicing);
                    });
                });
            }
            else if (result == 11) // Export as MIDI
            {
                auto selectedId = getSelectedVoicingId();
                if (selectedId.isEmpty()) return;
                const auto* v = processorRef.voicingLibrary.getVoicing (selectedId);
                if (v == nullptr) return;
                auto voicingCopy = *v;
                auto defaultName = v->name.isNotEmpty() ? v->name : "voicing";
                juce::MessageManager::callAsync ([this, voicingCopy, defaultName]
                {
                    fileChooser = std::make_unique<juce::FileChooser> (
                        "Export as MIDI...",
                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile (defaultName + ".mid"),
                        "*.mid");
                    auto flags = juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::warnAboutOverwriting;
                    fileChooser->launchAsync (flags, [voicingCopy] (const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (f == juce::File()) return;
                        MidiFileUtils::exportVoicingToMidi (voicingCopy, f);
                    });
                });
            }
            else if (result == 20) // Import Library (.chordy)
            {
                juce::MessageManager::callAsync ([this] {
                    fileChooser = std::make_unique<juce::FileChooser> (
                        "Import Library...",
                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                        "*.chordy");
                    auto flags = juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectFiles;
                    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (! f.existsAsFile()) return;
                        auto imported = LibraryExporter::parseCollection (f);
                        if (! imported.success) return;
                        auto merged = LibraryExporter::mergeIntoLibraries (
                            imported,
                            processorRef.voicingLibrary,
                            processorRef.progressionLibrary,
                            processorRef.melodyLibrary);
                        processorRef.saveLibrariesToDisk();
                        refreshFolderCombo();
                        updateDisplayedVoicings();
                        juce::AlertWindow::showMessageBoxAsync (
                            juce::MessageBoxIconType::InfoIcon,
                            "Import Complete",
                            "Added " + juce::String (merged.voicingsAdded) + " voicings, "
                            + juce::String (merged.progressionsAdded) + " progressions, "
                            + juce::String (merged.melodiesAdded) + " melodies.");
                    });
                });
            }
            else if (result == 21) // Export Selected (.chordy)
            {
                auto ids = getSelectedIds();
                if (ids.empty()) return;
                juce::MessageManager::callAsync ([this, ids] {
                    fileChooser = std::make_unique<juce::FileChooser> (
                        "Export Selected...",
                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("voicings.chordy"),
                        "*.chordy");
                    auto flags = juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::warnAboutOverwriting;
                    fileChooser->launchAsync (flags, [this, ids] (const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (f == juce::File()) return;
                        LibraryExporter::ExportOptions opts;
                        opts.collectionName = f.getFileNameWithoutExtension();
                        opts.voicingIds = ids;
                        LibraryExporter::exportCollection (opts,
                            processorRef.voicingLibrary,
                            processorRef.progressionLibrary,
                            processorRef.melodyLibrary, f);
                    });
                });
            }
            else if (result == 22) // Export All Voicings (.chordy)
            {
                juce::MessageManager::callAsync ([this] {
                    fileChooser = std::make_unique<juce::FileChooser> (
                        "Export All Voicings...",
                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("all_voicings.chordy"),
                        "*.chordy");
                    auto flags = juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::warnAboutOverwriting;
                    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (f == juce::File()) return;
                        LibraryExporter::ExportOptions opts;
                        opts.collectionName = f.getFileNameWithoutExtension();
                        opts.includeProgressions = false;
                        opts.includeMelodies = false;
                        LibraryExporter::exportCollection (opts,
                            processorRef.voicingLibrary,
                            processorRef.progressionLibrary,
                            processorRef.melodyLibrary, f);
                    });
                });
            }
            else if (result == 30) // Export Sheet Music (PDF)
            {
                auto selectedId = getSelectedVoicingId();
                if (selectedId.isEmpty()) return;
                const auto* v = processorRef.voicingLibrary.getVoicing (selectedId);
                if (v == nullptr) return;
                auto voicingCopy = *v;

                juce::MessageManager::callAsync ([this, voicingCopy] {
                    ExportSheetMusicDialog::show (
                        ExportSheetMusicDialog::ContentType::Voicing,
                        voicingCopy.name,
                        voicingCopy.rootPitchClass,
                        [this, voicingCopy] (LilyPondExporter::ExportOptions opts)
                        {
                            auto lyContent = LilyPondExporter::generateVoicingLy (voicingCopy, opts);
                            auto defaultName = opts.title.isNotEmpty() ? opts.title : voicingCopy.name;

                            juce::MessageManager::callAsync ([this, lyContent, defaultName] {
                                fileChooser = std::make_unique<juce::FileChooser> (
                                    "Save Sheet Music PDF...",
                                    juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                        .getChildFile (defaultName + ".pdf"),
                                    "*.pdf");
                                auto flags = juce::FileBrowserComponent::saveMode
                                           | juce::FileBrowserComponent::warnAboutOverwriting;
                                fileChooser->launchAsync (flags, [lyContent] (const juce::FileChooser& fc)
                                {
                                    auto f = fc.getResult();
                                    if (f == juce::File()) return;
                                    auto exportResult = LilyPondExporter::renderToPdf (lyContent, f);
                                    if (exportResult.success)
                                        juce::AlertWindow::showMessageBoxAsync (
                                            juce::MessageBoxIconType::InfoIcon,
                                            "Export Complete",
                                            "Sheet music saved to:\n" + f.getFullPathName());
                                    else
                                        juce::AlertWindow::showMessageBoxAsync (
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Export Failed",
                                            exportResult.errorMessage);
                                });
                            });
                        });
                });
            }
            else if (result == 1)
            {
                auto* aw = new juce::AlertWindow ("New Folder",
                                                   "Enter folder name:",
                                                   juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", "", "Name:");
                aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create (
                    [this, aw] (int r)
                    {
                        if (r == 1)
                        {
                            auto name = aw->getTextEditorContents ("name").trim();
                            if (name.isNotEmpty())
                            {
                                Folder f;
                                f.id = juce::Uuid().toString();
                                f.name = name;
                                f.sortOrder = processorRef.voicingLibrary.getFolders().size();
                                processorRef.voicingLibrary.getFolders().addFolder (f);
                                processorRef.saveLibrariesToDisk();
                                refreshFolderCombo();
                            }
                        }
                        delete aw;
                    }), true);
                if (auto* te = aw->getTextEditor ("name"))
                    te->grabKeyboardFocus();
            }
            else if (result == 2)
            {
                int fi = folderCombo.getSelectedId() - 3;
                const auto& folders = processorRef.voicingLibrary.getFolders().getAllFolders();
                if (fi >= 0 && fi < static_cast<int> (folders.size()))
                {
                    auto currentName = folders[static_cast<size_t> (fi)].name;
                    auto fId = folders[static_cast<size_t> (fi)].id;

                    auto* aw = new juce::AlertWindow ("Rename Folder",
                                                       "Enter new name:",
                                                       juce::MessageBoxIconType::NoIcon);
                    aw->addTextEditor ("name", currentName, "Name:");
                    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                    aw->enterModalState (true, juce::ModalCallbackFunction::create (
                        [this, aw, fId] (int r)
                        {
                            if (r == 1)
                            {
                                auto name = aw->getTextEditorContents ("name").trim();
                                if (name.isNotEmpty())
                                {
                                    auto* folder = processorRef.voicingLibrary.getFolders().getFolder (fId);
                                    if (folder != nullptr)
                                    {
                                        folder->name = name;
                                        processorRef.saveLibrariesToDisk();
                                        refreshFolderCombo();
                                    }
                                }
                            }
                            delete aw;
                        }), true);
                    if (auto* te = aw->getTextEditor ("name"))
                        te->grabKeyboardFocus();
                }
            }
            else if (result == 3)
            {
                int fi = folderCombo.getSelectedId() - 3;
                const auto& folders = processorRef.voicingLibrary.getFolders().getAllFolders();
                if (fi >= 0 && fi < static_cast<int> (folders.size()))
                {
                    auto fId = folders[static_cast<size_t> (fi)].id;
                    // Move all items in this folder to root
                    for (auto& v : const_cast<std::vector<Voicing>&> (processorRef.voicingLibrary.getAllVoicings()))
                        if (v.folderId == fId)
                            v.folderId = {};
                    processorRef.voicingLibrary.getFolders().removeFolder (fId);
                    processorRef.saveLibrariesToDisk();
                    refreshFolderCombo();
                    updateDisplayedVoicings();
                }
            }
        });
}

juce::PopupMenu VoicingLibraryPanel::buildFolderSubmenu (int baseId)
{
    juce::PopupMenu sub;
    sub.addItem (baseId, "New Folder...");
    sub.addSeparator();
    sub.addItem (baseId + 1, "Unfiled");
    const auto& folders = processorRef.voicingLibrary.getFolders().getAllFolders();
    for (int i = 0; i < static_cast<int> (folders.size()); ++i)
        sub.addItem (baseId + 2 + i, folders[static_cast<size_t> (i)].name);
    return sub;
}

void VoicingLibraryPanel::handleFolderSubmenuResult (int result, int baseId)
{
    if (result == baseId) // "New Folder..."
    {
        auto* aw = new juce::AlertWindow ("New Folder", "Enter folder name:",
                                           juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "", "Name:");
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, aw] (int r)
            {
                if (r == 1)
                {
                    auto name = aw->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty())
                    {
                        Folder f;
                        f.id = juce::Uuid().toString();
                        f.name = name;
                        f.sortOrder = processorRef.voicingLibrary.getFolders().size();
                        processorRef.voicingLibrary.getFolders().addFolder (f);
                        processorRef.saveLibrariesToDisk();
                        refreshFolderCombo();
                        moveSelectedToFolder (f.id);
                    }
                }
                delete aw;
            }), true);
        if (auto* te = aw->getTextEditor ("name"))
            te->grabKeyboardFocus();
    }
    else if (result == baseId + 1) // "Unfiled"
    {
        moveSelectedToFolder ({});
    }
    else if (result >= baseId + 2)
    {
        const auto& folders = processorRef.voicingLibrary.getFolders().getAllFolders();
        int fi = result - baseId - 2;
        if (fi >= 0 && fi < static_cast<int> (folders.size()))
            moveSelectedToFolder (folders[static_cast<size_t> (fi)].id);
    }
}

void VoicingLibraryPanel::showContextMenu (int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int> (displayedVoicings.size()))
        return;

    juce::PopupMenu menu;
    menu.addSubMenu ("Move to Folder", buildFolderSubmenu (100));
    menu.addItem (2, "Export as MIDI...", getSelectionCount() == 1);
    menu.addSeparator();
    menu.addItem (1, "Delete");

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [this] (int result)
        {
            if (result == 1)
                onDelete();
            else if (result == 2)
            {
                // Trigger export via the more menu's export handler
                auto selectedId = getSelectedVoicingId();
                if (selectedId.isEmpty()) return;
                const auto* v = processorRef.voicingLibrary.getVoicing (selectedId);
                if (v == nullptr) return;
                auto voicingCopy = *v;
                auto defaultName = v->name.isNotEmpty() ? v->name : juce::String ("voicing");
                juce::MessageManager::callAsync ([this, voicingCopy, defaultName] {
                    fileChooser = std::make_unique<juce::FileChooser> (
                        "Export as MIDI...",
                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile (defaultName + ".mid"),
                        "*.mid");
                    auto flags = juce::FileBrowserComponent::saveMode
                               | juce::FileBrowserComponent::warnAboutOverwriting;
                    fileChooser->launchAsync (flags, [voicingCopy] (const juce::FileChooser& fc)
                    {
                        auto f = fc.getResult();
                        if (f == juce::File()) return;
                        MidiFileUtils::exportVoicingToMidi (voicingCopy, f);
                    });
                });
            }
            else if (result >= 100)
                handleFolderSubmenuResult (result, 100);
        });
}

void VoicingLibraryPanel::moveSelectedToFolder (const juce::String& targetFolderId)
{
    auto ids = getSelectedIds();
    if (ids.empty()) return;

    for (auto& v : const_cast<std::vector<Voicing>&> (processorRef.voicingLibrary.getAllVoicings()))
        for (const auto& id : ids)
            if (v.id == id)
                v.folderId = targetFolderId;

    processorRef.saveLibrariesToDisk();
    updateDisplayedVoicings();
}

