#include "VoicingLibraryPanel.h"
#include "PluginProcessor.h"
#include "ChordDetector.h"
#include "ChordyTheme.h"

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
    // List colors inherited from LookAndFeel
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
    searchEditor.setVisible (visible);
    qualityFilter.setVisible (visible);
    voicingList.setVisible (visible);
    recordButton.setVisible (visible);
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
    headerLabel.setBounds (headerRow.removeFromLeft (headerRow.getWidth() / 2));
    recordingIndicator.setBounds (headerRow);
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
    recordButton.setBounds (bottomRow.removeFromRight (70));
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
    int bw = (buttonRow.getWidth() - 8) / 2;
    confirmSaveButton.setBounds (buttonRow.removeFromLeft (bw));
    buttonRow.removeFromLeft (8);
    confirmCancelButton.setBounds (buttonRow);
}

void VoicingLibraryPanel::refresh()
{
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
    int row = voicingList.getSelectedRow();
    if (row >= 0 && row < static_cast<int> (displayedVoicings.size()))
        return displayedVoicings[static_cast<size_t> (row)].id;
    return {};
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
    deleteButton.setEnabled (enabled);
}

void VoicingLibraryPanel::updateDisplayedVoicings()
{
    int filterId = qualityFilter.getSelectedId();
    const auto& all = processorRef.voicingLibrary.getAllVoicings();

    displayedVoicings.clear();

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

    // Filter by search text
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

void VoicingLibraryPanel::populateConfirmFields()
{
    // Auto-detect chord for pre-filling
    auto notes = VoicingLibrary::transposeToKey (pendingVoicing, pendingVoicing.octaveReference);
    auto detected = ChordDetector::detect (notes);

    // Pre-fill name
    if (detected.isValid())
        confirmNameEditor.setText (detected.displayName + " voicing");
    else
        confirmNameEditor.setText ("Voicing " + juce::String (processorRef.voicingLibrary.size() + 1));

    // Pre-fill root
    confirmRootCombo.setSelectedId (pendingVoicing.rootPitchClass + 1, juce::dontSendNotification);

    // Pre-fill quality
    confirmQualityCombo.setSelectedId (comboIdFromQuality (pendingVoicing.quality), juce::dontSendNotification);

    // Clear alterations
    confirmAltEditor.clear();
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
    auto id = getSelectedVoicingId();
    if (id.isNotEmpty())
    {
        processorRef.voicingLibrary.removeVoicing (id);
        processorRef.saveLibrariesToDisk();
        updateDisplayedVoicings();
    }
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
    g.setFont (14.0f);
    g.drawText (v.name, 8, 0, width - 80, height, juce::Justification::centredLeft);

    // Show quality + alterations badge
    g.setColour (juce::Colour (ChordyTheme::textSecondary));
    g.setFont (11.0f);
    g.drawText (v.getQualityLabel(),
                width - 70, 0, 62, height, juce::Justification::centredRight);
}

void VoicingLibraryPanel::selectedRowsChanged (int /*lastRowClicked*/)
{
    auto selectedId = getSelectedVoicingId();

    if (selectedId.isNotEmpty())
        statsChart.setStats (processorRef.spacedRepetition.getStatsForVoicing (selectedId));
    else
        statsChart.clearStats();

    if (onSelectionChanged)
        onSelectionChanged (selectedId);
}
