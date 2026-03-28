#include "ProgressionLibraryPanel.h"
#include "PluginProcessor.h"
#include "ChordDetector.h"
#include "ChordyTheme.h"
#include "ProgressionRecorder.h"

static void addQualityItems (juce::ComboBox& combo)
{
    combo.addItem ("Major",    1);
    combo.addItem ("Minor",    2);
    combo.addItem ("Dom7",     3);
    combo.addItem ("Maj7",     4);
    combo.addItem ("Min7",     5);
    combo.addItem ("Dim",      6);
    combo.addItem ("Dim7",     7);
    combo.addItem ("Aug",      8);
    combo.addItem ("HalfDim7", 9);
    combo.addItem ("MinMaj7", 10);
    combo.addItem ("Sus2",    11);
    combo.addItem ("Sus4",    12);
    combo.addItem ("N/A",     13);
}

static ChordQuality qualityFromEditComboId (int id)
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
        case 11: return ChordQuality::Sus2;
        case 12: return ChordQuality::Sus4;
        case 13: return ChordQuality::Unknown;
        default: return ChordQuality::Unknown;
    }
}

static int editComboIdFromQuality (ChordQuality q)
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
        case ChordQuality::Sus2:        return 11;
        case ChordQuality::Sus4:        return 12;
        default:                        return 13;
    }
}

//==============================================================================

ProgressionLibraryPanel::ProgressionLibraryPanel (AudioPluginAudioProcessor& processor)
    : processorRef (processor)
{
    auto labelStyle = [](juce::Label& l) {
        l.setFont (juce::FontOptions (ChordyTheme::fontBody));
        l.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textSecondary));
    };

    // --- Idle ---
    headerLabel.setText ("PROGRESSIONS", juce::dontSendNotification);
    headerLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addAndMakeVisible (headerLabel);

    recordingIndicator.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    recordingIndicator.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
    recordingIndicator.setJustificationType (juce::Justification::centredRight);
    recordingIndicator.setVisible (false);
    addAndMakeVisible (recordingIndicator);

    progressionList.setModel (this);
    progressionList.setOutlineThickness (1);
    progressionList.setRowHeight (36);
    addAndMakeVisible (progressionList);

    searchEditor.setTextToShowWhenEmpty ("Search...", juce::Colour (ChordyTheme::textTertiary));
    searchEditor.onTextChange = [this] { updateDisplayedProgressions(); };
    addAndMakeVisible (searchEditor);

    updateDisplayedProgressions();

    recordButton.onClick = [this] {
        if (panelState == PanelState::Idle)
            enterCountIn();
        else if (panelState == PanelState::CountIn || panelState == PanelState::Waiting)
            enterIdle();
        else if (panelState == PanelState::Recording)
            onStopRecording();
    };
    addAndMakeVisible (recordButton);

    playButton.onClick = [this] { onPlayToggle(); };
    addAndMakeVisible (playButton);

    editButton.onClick = [this] { onEditExisting(); };
    addAndMakeVisible (editButton);

    deleteButton.onClick = [this] { onDelete(); };
    deleteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    addAndMakeVisible (deleteButton);

    statsChart.onKeyClicked = [this](int keyIndex) {
        if (panelState != PanelState::Idle) return;
        auto id = getSelectedProgressionId();
        const auto* prog = processorRef.progressionLibrary.getProgression (id);
        if (prog == nullptr) return;

        // Toggle off if same key clicked while playing
        if (processorRef.isPlayingProgression() && statsPlayingKey == keyIndex)
        {
            processorRef.stopProgressionPlayback();
            playButton.setButtonText ("Play");
            statsChart.setPlayingKey (-1);
            statsPlayingKey = -1;
            return;
        }

        // Stop any current playback
        if (processorRef.isPlayingProgression())
            processorRef.stopProgressionPlayback();

        int semitones = (keyIndex - prog->keyPitchClass + 12) % 12;
        if (semitones > 6) semitones -= 12;
        auto transposed = ProgressionLibrary::transposeProgression (*prog, semitones);
        processorRef.startProgressionPlayback (transposed);
        playButton.setButtonText ("Stop");
        statsChart.setPlayingKey (keyIndex);
        statsPlayingKey = keyIndex;
        if (onTransposedPreview)
            onTransposedPreview (transposed);
    };
    addAndMakeVisible (statsChart);

    // --- Editing ---
    editHeader.setText ("EDIT PROGRESSION", juce::dontSendNotification);
    editHeader.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    editHeader.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addChildComponent (editHeader);

    auto quantBtnSetup = [this](juce::TextButton& btn, double res) {
        btn.onClick = [this, res] { applyQuantize (res); };
        addChildComponent (btn);
    };
    quantRawBtn.onClick = [this] { applyQuantize (0.0); };
    addChildComponent (quantRawBtn);

    quantBtnSetup (quantBeatBtn, 1.0);
    quantBtnSetup (quantHalfBtn, 0.5);
    quantBtnSetup (quantQuarterBtn, 0.25);

    editPlayBtn.onClick = [this] { onEditPlayToggle(); };
    addChildComponent (editPlayBtn);

    transposeLabel.setText ("Transpose:", juce::dontSendNotification);
    transposeLabel.setFont (juce::FontOptions (ChordyTheme::fontSmall));
    transposeLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textSecondary));
    addChildComponent (transposeLabel);

    transposeDownBtn.onClick = [this] { onTranspose (-1); };
    addChildComponent (transposeDownBtn);

    transposeUpBtn.onClick = [this] { onTranspose (1); };
    addChildComponent (transposeUpBtn);

    editChart.onChordSelected = [this](int idx) { onEditChordSelected (idx); };
    editChart.setEditMode (true);
    addChildComponent (editChart);

    editChordLabel.setText ("Chord:", juce::dontSendNotification);
    labelStyle (editChordLabel);
    addChildComponent (editChordLabel);

    editChordNameEditor.setTextToShowWhenEmpty ("Name...", juce::Colour (ChordyTheme::textTertiary));
    editChordNameEditor.onReturnKey = [this] {
        int idx = editChart.getSelectedChord();
        if (idx >= 0 && idx < static_cast<int> (pendingProgression.chords.size()))
        {
            pendingProgression.chords[static_cast<size_t> (idx)].name =
                editChordNameEditor.getText().trim();
            editChart.repaint();
        }
    };
    addChildComponent (editChordNameEditor);

    editRootLabel.setText ("Root:", juce::dontSendNotification);
    labelStyle (editRootLabel);
    addChildComponent (editRootLabel);

    for (int i = 0; i < 12; ++i)
        editRootCombo.addItem (ChordDetector::noteNameFromPitchClass (i), i + 1);
    editRootCombo.onChange = [this] {
        int idx = editChart.getSelectedChord();
        if (idx >= 0 && idx < static_cast<int> (pendingProgression.chords.size()))
        {
            auto& chord = pendingProgression.chords[static_cast<size_t> (idx)];
            chord.rootPitchClass = editRootCombo.getSelectedId() - 1;
            chord.name = chord.getDisplayName();
            editChordNameEditor.setText (chord.name, juce::dontSendNotification);
            editChart.repaint();
        }
    };
    addChildComponent (editRootCombo);

    editQualityLabel.setText ("Quality:", juce::dontSendNotification);
    labelStyle (editQualityLabel);
    addChildComponent (editQualityLabel);

    addQualityItems (editQualityCombo);
    editQualityCombo.onChange = [this] {
        int idx = editChart.getSelectedChord();
        if (idx >= 0 && idx < static_cast<int> (pendingProgression.chords.size()))
        {
            auto& chord = pendingProgression.chords[static_cast<size_t> (idx)];
            chord.quality = qualityFromEditComboId (editQualityCombo.getSelectedId());
            chord.name = chord.getDisplayName();
            editChordNameEditor.setText (chord.name, juce::dontSendNotification);
            editChart.repaint();
        }
    };
    addChildComponent (editQualityCombo);

    editDeleteChordBtn.onClick = [this] { onEditDeleteChord(); };
    editDeleteChordBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    addChildComponent (editDeleteChordBtn);

    editDoneButton.onClick = [this] { onEditDone(); };
    addChildComponent (editDoneButton);

    editCancelButton.onClick = [this] { enterIdle(); };
    addChildComponent (editCancelButton);

    // --- Confirming ---
    confirmHeader.setText ("SAVE PROGRESSION", juce::dontSendNotification);
    confirmHeader.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    confirmHeader.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addChildComponent (confirmHeader);

    confirmNameLabel.setText ("Name:", juce::dontSendNotification);
    labelStyle (confirmNameLabel);
    addChildComponent (confirmNameLabel);

    confirmNameEditor.setTextToShowWhenEmpty ("Progression name...", juce::Colour (ChordyTheme::textTertiary));
    addChildComponent (confirmNameEditor);

    confirmKeyLabel.setText ("Key:", juce::dontSendNotification);
    labelStyle (confirmKeyLabel);
    addChildComponent (confirmKeyLabel);

    for (int i = 0; i < 12; ++i)
        confirmKeyCombo.addItem (ChordDetector::noteNameFromPitchClass (i), i + 1);
    addChildComponent (confirmKeyCombo);

    confirmModeLabel.setText ("Mode:", juce::dontSendNotification);
    labelStyle (confirmModeLabel);
    addChildComponent (confirmModeLabel);

    confirmModeCombo.addItem ("Major", 1);
    confirmModeCombo.addItem ("Minor", 2);
    confirmModeCombo.addItem ("Dorian", 3);
    confirmModeCombo.addItem ("Mixolydian", 4);
    confirmModeCombo.addItem ("Lydian", 5);
    confirmModeCombo.addItem ("Phrygian", 6);
    confirmModeCombo.addItem ("Locrian", 7);
    confirmModeCombo.addItem ("Blues", 8);
    confirmModeCombo.addItem ("Other", 9);
    addChildComponent (confirmModeCombo);

    confirmSaveButton.onClick = [this] { onConfirmSave(); };
    addChildComponent (confirmSaveButton);

    confirmCancelButton.onClick = [this] { enterIdle(); };
    addChildComponent (confirmCancelButton);
}

//==============================================================================

void ProgressionLibraryPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (ChordyTheme::bgSurface));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), ChordyTheme::cornerRadius);

    if (panelState == PanelState::CountIn || panelState == PanelState::Waiting
        || panelState == PanelState::Recording)
    {
        g.setColour (juce::Colour (ChordyTheme::danger));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f),
                                ChordyTheme::cornerRadius, 2.0f);
    }
    else if (panelState == PanelState::Confirming)
    {
        g.setColour (juce::Colour (ChordyTheme::accent));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f),
                                ChordyTheme::cornerRadius, 2.0f);
    }
}

void ProgressionLibraryPanel::resized()
{
    auto area = getLocalBounds().reduced (ChordyTheme::panelPadding);

    switch (panelState)
    {
        case PanelState::Editing:    layoutEditMode (area); break;
        case PanelState::Confirming: layoutConfirmMode (area); break;
        default:                     layoutIdleMode (area); break;
    }
}

void ProgressionLibraryPanel::layoutIdleMode (juce::Rectangle<int> area)
{
    auto headerRow = area.removeFromTop (24);
    headerLabel.setBounds (headerRow.removeFromLeft (headerRow.getWidth() / 2));
    recordingIndicator.setBounds (headerRow);
    area.removeFromTop (4);

    auto bottomRow = area.removeFromBottom (30);
    int bw = (bottomRow.getWidth() - 12) / 4;
    recordButton.setBounds (bottomRow.removeFromLeft (bw));
    bottomRow.removeFromLeft (4);
    playButton.setBounds (bottomRow.removeFromLeft (bw));
    bottomRow.removeFromLeft (4);
    editButton.setBounds (bottomRow.removeFromLeft (bw));
    bottomRow.removeFromLeft (4);
    deleteButton.setBounds (bottomRow);
    area.removeFromBottom (4);

    // Stats chart
    auto statsArea = area.removeFromBottom (60);
    area.removeFromBottom (4);
    statsChart.setBounds (statsArea);

    // Search bar
    auto searchRow = area.removeFromTop (24);
    searchEditor.setBounds (searchRow);
    area.removeFromTop (4);

    // List fills remaining space
    progressionList.setBounds (area);
}

void ProgressionLibraryPanel::layoutEditMode (juce::Rectangle<int> area)
{
    editHeader.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);

    // Quantize + play buttons row
    auto quantRow = area.removeFromTop (24);
    int btnW = (quantRow.getWidth() - 20) / 5;
    quantRawBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    quantBeatBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    quantHalfBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    quantQuarterBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    editPlayBtn.setBounds (quantRow);
    area.removeFromTop (4);

    // Transpose row
    auto transposeRow = area.removeFromTop (24);
    transposeLabel.setBounds (transposeRow.removeFromLeft (70));
    transposeRow.removeFromLeft (4);
    transposeDownBtn.setBounds (transposeRow.removeFromLeft (36));
    transposeRow.removeFromLeft (4);
    transposeUpBtn.setBounds (transposeRow.removeFromLeft (36));
    area.removeFromTop (4);

    // Buttons at bottom
    auto buttonRow = area.removeFromBottom (28);
    int bw = (buttonRow.getWidth() - 16) / 3;
    editDoneButton.setBounds (buttonRow.removeFromLeft (bw));
    buttonRow.removeFromLeft (8);
    editDeleteChordBtn.setBounds (buttonRow.removeFromLeft (bw));
    buttonRow.removeFromLeft (8);
    editCancelButton.setBounds (buttonRow);
    area.removeFromBottom (4);

    // Chord edit controls (bottom of remaining area)
    auto editArea = area.removeFromBottom (80);
    area.removeFromBottom (4);

    auto row = [&]() -> juce::Rectangle<int> {
        auto r = editArea.removeFromTop (24);
        editArea.removeFromTop (2);
        return r;
    };

    auto nameRow = row();
    editChordLabel.setBounds (nameRow.removeFromLeft (50));
    editChordNameEditor.setBounds (nameRow);

    auto rootQualRow = row();
    editRootLabel.setBounds (rootQualRow.removeFromLeft (35));
    editRootCombo.setBounds (rootQualRow.removeFromLeft (60));
    rootQualRow.removeFromLeft (8);
    editQualityLabel.setBounds (rootQualRow.removeFromLeft (50));
    editQualityCombo.setBounds (rootQualRow);

    // Chart takes remaining space
    editChart.setBounds (area);
}

void ProgressionLibraryPanel::layoutConfirmMode (juce::Rectangle<int> area)
{
    confirmHeader.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto row = [&]() -> juce::Rectangle<int> {
        auto r = area.removeFromTop (26);
        area.removeFromTop (4);
        return r;
    };

    auto nameRow = row();
    confirmNameLabel.setBounds (nameRow.removeFromLeft (50));
    confirmNameEditor.setBounds (nameRow);

    auto keyRow = row();
    confirmKeyLabel.setBounds (keyRow.removeFromLeft (50));
    confirmKeyCombo.setBounds (keyRow);

    auto modeRow = row();
    confirmModeLabel.setBounds (modeRow.removeFromLeft (50));
    confirmModeCombo.setBounds (modeRow);

    area.removeFromTop (8);
    auto buttonRow = area.removeFromTop (30);
    int bw = (buttonRow.getWidth() - 8) / 2;
    confirmSaveButton.setBounds (buttonRow.removeFromLeft (bw));
    buttonRow.removeFromLeft (8);
    confirmCancelButton.setBounds (buttonRow);
}

//==============================================================================

void ProgressionLibraryPanel::setIdleModeVisible (bool v)
{
    headerLabel.setVisible (v);
    searchEditor.setVisible (v);
    progressionList.setVisible (v);
    statsChart.setVisible (v);
    recordButton.setVisible (v);
    playButton.setVisible (v);
    editButton.setVisible (v);
    deleteButton.setVisible (v);
}

void ProgressionLibraryPanel::setEditModeVisible (bool v)
{
    editHeader.setVisible (v);
    editChart.setVisible (v);
    quantRawBtn.setVisible (v);
    quantBeatBtn.setVisible (v);
    quantHalfBtn.setVisible (v);
    quantQuarterBtn.setVisible (v);
    editPlayBtn.setVisible (v);
    transposeLabel.setVisible (v);
    transposeDownBtn.setVisible (v);
    transposeUpBtn.setVisible (v);
    editChordLabel.setVisible (v);
    editChordNameEditor.setVisible (v);
    editRootLabel.setVisible (v);
    editRootCombo.setVisible (v);
    editQualityLabel.setVisible (v);
    editQualityCombo.setVisible (v);
    editDeleteChordBtn.setVisible (v);
    editDoneButton.setVisible (v);
    editCancelButton.setVisible (v);
}

void ProgressionLibraryPanel::setConfirmModeVisible (bool v)
{
    confirmHeader.setVisible (v);
    confirmNameLabel.setVisible (v);
    confirmNameEditor.setVisible (v);
    confirmKeyLabel.setVisible (v);
    confirmKeyCombo.setVisible (v);
    confirmModeLabel.setVisible (v);
    confirmModeCombo.setVisible (v);
    confirmSaveButton.setVisible (v);
    confirmCancelButton.setVisible (v);
}

//==============================================================================

void ProgressionLibraryPanel::enterIdle()
{
    if (panelState == PanelState::Recording)
        processorRef.progressionRecorder.stopRecording();

    processorRef.stopProgressionPlayback();

    panelState = PanelState::Idle;
    pendingProgression = {};
    countInBeatsElapsed = 0;
    lastCountInBeat = -1;

    setEditModeVisible (false);
    setConfirmModeVisible (false);
    setIdleModeVisible (true);
    recordingIndicator.setVisible (false);

    recordButton.setButtonText ("Record");
    recordButton.setColour (juce::TextButton::buttonColourId,
                            getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    playButton.setButtonText ("Play");

    resized();
    repaint();
    updateDisplayedProgressions();
}

void ProgressionLibraryPanel::setButtonsEnabled (bool enabled)
{
    recordButton.setEnabled (enabled);
    playButton.setEnabled (enabled);
    editButton.setEnabled (enabled);
    deleteButton.setEnabled (enabled);
}

void ProgressionLibraryPanel::enterCountIn()
{
    if (onRecordStarted)
        onRecordStarted();

    panelState = PanelState::CountIn;
    countInBeatsElapsed = 0;
    lastCountInBeat = -1;

    recordButton.setButtonText ("Cancel");
    recordButton.setColour (juce::TextButton::buttonColourId,
                            juce::Colour (ChordyTheme::dangerMuted));
    recordingIndicator.setText ("Count-in...", juce::dontSendNotification);
    recordingIndicator.setVisible (true);

    // Turn on metronome and reset beat position
    if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
        param->setValueNotifyingHost (1.0f);
    processorRef.tempoEngine.resetBeatPosition();

    repaint();
}

void ProgressionLibraryPanel::enterRecording()
{
    panelState = PanelState::Recording;

    double bpm = *processorRef.apvts.getRawParameterValue ("bpm");
    int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    processorRef.progressionRecorder.startRecording (bpm, processorRef.getSampleRate());

    // Inject any currently held notes as note-ons at beat 0 with their actual velocities
    auto heldNotes = processorRef.getActiveNotes();
    for (int note : heldNotes)
    {
        int vel = processorRef.getNoteVelocity (note);
        if (vel <= 0) vel = 80;
        auto msg = juce::MidiMessage::noteOn (ch, note, static_cast<juce::uint8> (vel));
        msg.setTimeStamp (0.0);
        processorRef.progressionRecorder.injectEvent (msg);
    }

    recordButton.setButtonText ("Stop");
    recordingIndicator.setText ("REC - Recording...", juce::dontSendNotification);
    repaint();
}

void ProgressionLibraryPanel::enterEditing()
{
    panelState = PanelState::Editing;

    setIdleModeVisible (false);
    setEditModeVisible (true);
    recordingIndicator.setVisible (false);

    // Turn off metronome after recording
    if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
        param->setValueNotifyingHost (0.0f);

    quantRawBtn.setToggleState (currentQuantizeResolution == 0.0, juce::dontSendNotification);
    quantBeatBtn.setToggleState (currentQuantizeResolution == 1.0, juce::dontSendNotification);
    quantHalfBtn.setToggleState (currentQuantizeResolution == 0.5, juce::dontSendNotification);
    quantQuarterBtn.setToggleState (currentQuantizeResolution == 0.25, juce::dontSendNotification);

    editPlayBtn.setButtonText ("Play");
    editChart.setQuantizeGrid (currentQuantizeResolution);
    editChart.setProgression (&pendingProgression);

    resized();
    repaint();
}

void ProgressionLibraryPanel::enterConfirming()
{
    panelState = PanelState::Confirming;
    processorRef.stopProgressionPlayback();

    setEditModeVisible (false);
    setConfirmModeVisible (true);

    // Pre-fill with existing values if editing, otherwise defaults
    if (pendingProgression.name.isNotEmpty())
        confirmNameEditor.setText (pendingProgression.name);
    else
        confirmNameEditor.setText ("Progression " +
            juce::String (processorRef.progressionLibrary.size() + 1));

    confirmKeyCombo.setSelectedId (pendingProgression.keyPitchClass + 1, juce::dontSendNotification);

    // Match mode string to combo
    int modeId = 1;
    if (pendingProgression.mode == "Minor") modeId = 2;
    else if (pendingProgression.mode == "Dorian") modeId = 3;
    else if (pendingProgression.mode == "Mixolydian") modeId = 4;
    else if (pendingProgression.mode == "Lydian") modeId = 5;
    else if (pendingProgression.mode == "Phrygian") modeId = 6;
    else if (pendingProgression.mode == "Locrian") modeId = 7;
    else if (pendingProgression.mode == "Blues") modeId = 8;
    else if (pendingProgression.mode == "Other") modeId = 9;
    confirmModeCombo.setSelectedId (modeId, juce::dontSendNotification);

    resized();
    repaint();
}

//==============================================================================

void ProgressionLibraryPanel::updateRecording (const std::vector<int>& activeNotes)
{
    juce::ignoreUnused (activeNotes);
    // Count-in and recording are now driven by updateTimerCallback
}

void ProgressionLibraryPanel::updateTimerCallback()
{
    // --- Count-in logic ---
    if (panelState == PanelState::CountIn)
    {
        int currentBeat = processorRef.tempoEngine.getBeatNumber();
        if (currentBeat != lastCountInBeat)
        {
            lastCountInBeat = currentBeat;
            countInBeatsElapsed++;

            if (countInBeatsElapsed >= 5)
            {
                // Count-in done → start recording
                enterRecording();
            }
            else
            {
                recordingIndicator.setText (juce::String (countInBeatsElapsed) + "...",
                                           juce::dontSendNotification);
            }
        }
    }

    // --- Chord preview note-off ---
    if (chordPreviewFrames > 0)
    {
        if (--chordPreviewFrames == 0 && ! chordPreviewNotes.empty())
        {
            int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
            for (int note : chordPreviewNotes)
                processorRef.addPreviewMidi (juce::MidiMessage::noteOff (ch, note, 0.0f));
            chordPreviewNotes.clear();
        }
    }

    // --- Playback cursor ---
    if (processorRef.isPlayingProgression())
    {
        double beat = processorRef.getPlaybackBeatPosition();

        if (panelState == PanelState::Editing)
            editChart.setCursorBeat (beat);
    }
    else
    {
        if (panelState == PanelState::Editing)
            editChart.setCursorBeat (-1.0);

        // Reset play button text if playback ended naturally
        if (panelState == PanelState::Idle)
        {
            playButton.setButtonText ("Play");
            statsChart.setPlayingKey (-1);
            statsPlayingKey = -1;
        }
        else if (panelState == PanelState::Editing)
            editPlayBtn.setButtonText ("Play");
    }
}

void ProgressionLibraryPanel::onStopRecording()
{
    processorRef.progressionRecorder.stopRecording();

    auto rawMidi = processorRef.progressionRecorder.getRawMidi();
    double totalBeats = processorRef.progressionRecorder.getTotalBeats();
    double bpm = *processorRef.apvts.getRawParameterValue ("bpm");

    auto chords = ProgressionRecorder::analyzeChordChanges (
        rawMidi, &processorRef.voicingLibrary);

    if (chords.empty())
    {
        enterIdle();
        return;
    }

    pendingProgression = {};
    pendingProgression.id = juce::Uuid().toString();
    pendingProgression.bpm = bpm;
    pendingProgression.totalBeats = totalBeats;
    pendingProgression.rawMidi = rawMidi;
    pendingProgression.chords = chords;

    currentQuantizeResolution = 0.0;

    double maxEnd = 0.0;
    for (const auto& c : pendingProgression.chords)
        maxEnd = juce::jmax (maxEnd, c.startBeat + c.durationBeats);
    pendingProgression.totalBeats = maxEnd;

    enterEditing();
}

void ProgressionLibraryPanel::applyQuantize (double resolution)
{
    processorRef.stopProgressionPlayback();
    currentQuantizeResolution = resolution;

    if (resolution > 0.0)
    {
        // Re-analyze from raw MIDI then quantize (non-destructive)
        auto freshChords = ProgressionRecorder::analyzeChordChanges (
            pendingProgression.rawMidi, &processorRef.voicingLibrary);
        pendingProgression.chords = ProgressionRecorder::quantize (
            freshChords, resolution, pendingProgression.totalBeats);
    }
    else
    {
        // Raw: re-analyze from raw MIDI with no quantization
        pendingProgression.chords = ProgressionRecorder::analyzeChordChanges (
            pendingProgression.rawMidi, &processorRef.voicingLibrary);
    }

    double maxEnd = 0.0;
    for (const auto& c : pendingProgression.chords)
        maxEnd = juce::jmax (maxEnd, c.startBeat + c.durationBeats);
    pendingProgression.totalBeats = juce::jmax (pendingProgression.totalBeats, maxEnd);

    quantRawBtn.setToggleState (resolution == 0.0, juce::dontSendNotification);
    quantBeatBtn.setToggleState (resolution == 1.0, juce::dontSendNotification);
    quantHalfBtn.setToggleState (resolution == 0.5, juce::dontSendNotification);
    quantQuarterBtn.setToggleState (resolution == 0.25, juce::dontSendNotification);

    editChart.setQuantizeGrid (resolution);
    editChart.setSelectedChord (-1);
    editChart.setProgression (&pendingProgression);
    editChart.repaint();
}

void ProgressionLibraryPanel::onEditChordSelected (int index)
{
    if (index < 0 || index >= static_cast<int> (pendingProgression.chords.size()))
    {
        editChordNameEditor.clear();
        editRootCombo.setSelectedId (0, juce::dontSendNotification);
        editQualityCombo.setSelectedId (0, juce::dontSendNotification);
        return;
    }

    const auto& chord = pendingProgression.chords[static_cast<size_t> (index)];
    editChordNameEditor.setText (chord.name, juce::dontSendNotification);
    editRootCombo.setSelectedId (chord.rootPitchClass + 1, juce::dontSendNotification);
    editQualityCombo.setSelectedId (editComboIdFromQuality (chord.quality), juce::dontSendNotification);

    // Play the chord via synth preview + highlight keyboard
    playChordPreview (chord);
    if (onChordPreview)
        onChordPreview (chord.midiNotes);
}

void ProgressionLibraryPanel::playChordPreview (const ProgressionChord& chord)
{
    // Release any previously previewing notes
    if (! chordPreviewNotes.empty())
    {
        int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
        for (int note : chordPreviewNotes)
            processorRef.addPreviewMidi (juce::MidiMessage::noteOff (ch, note, 0.0f));
        chordPreviewNotes.clear();
    }

    int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    for (int note : chord.midiNotes)
        processorRef.addPreviewMidi (juce::MidiMessage::noteOn (ch, note, 0.7f));

    chordPreviewNotes = chord.midiNotes;
    chordPreviewFrames = chordPreviewHoldFrames;
}

void ProgressionLibraryPanel::onEditDeleteChord()
{
    int idx = editChart.getSelectedChord();
    if (idx < 0 || idx >= static_cast<int> (pendingProgression.chords.size()))
        return;

    processorRef.stopProgressionPlayback();
    pendingProgression.chords.erase (
        pendingProgression.chords.begin() + static_cast<std::ptrdiff_t> (idx));

    // Recompute total beats
    double maxEnd = 0.0;
    for (const auto& c : pendingProgression.chords)
        maxEnd = juce::jmax (maxEnd, c.startBeat + c.durationBeats);
    pendingProgression.totalBeats = maxEnd;

    editChart.setSelectedChord (-1);
    editChart.setProgression (&pendingProgression);
    editChart.repaint();
    editChordNameEditor.clear();
}

void ProgressionLibraryPanel::onEditDone()
{
    processorRef.stopProgressionPlayback();

    int idx = editChart.getSelectedChord();
    if (idx >= 0 && idx < static_cast<int> (pendingProgression.chords.size()))
    {
        auto name = editChordNameEditor.getText().trim();
        if (name.isNotEmpty())
            pendingProgression.chords[static_cast<size_t> (idx)].name = name;
    }

    enterConfirming();
}

void ProgressionLibraryPanel::onConfirmSave()
{
    juce::String name = confirmNameEditor.getText().trim();
    if (name.isEmpty())
        name = "Progression " + juce::String (processorRef.progressionLibrary.size() + 1);

    pendingProgression.name = name;
    pendingProgression.keyPitchClass = confirmKeyCombo.getSelectedId() - 1;

    int modeId = confirmModeCombo.getSelectedId();
    switch (modeId)
    {
        case 1: pendingProgression.mode = "Major"; break;
        case 2: pendingProgression.mode = "Minor"; break;
        case 3: pendingProgression.mode = "Dorian"; break;
        case 4: pendingProgression.mode = "Mixolydian"; break;
        case 5: pendingProgression.mode = "Lydian"; break;
        case 6: pendingProgression.mode = "Phrygian"; break;
        case 7: pendingProgression.mode = "Locrian"; break;
        case 8: pendingProgression.mode = "Blues"; break;
        case 9: pendingProgression.mode = "Other"; break;
        default: pendingProgression.mode = "Major"; break;
    }

    // Store the quantize resolution for playback
    pendingProgression.quantizeResolution = currentQuantizeResolution;

    // Remove old version if editing an existing progression (same ID)
    processorRef.progressionLibrary.removeProgression (pendingProgression.id);
    processorRef.progressionLibrary.addProgression (pendingProgression);
    processorRef.saveLibrariesToDisk();
    enterIdle();
}

void ProgressionLibraryPanel::onDelete()
{
    auto id = getSelectedProgressionId();
    if (id.isNotEmpty())
    {
        processorRef.progressionLibrary.removeProgression (id);
        processorRef.saveLibrariesToDisk();
        updateDisplayedProgressions();
        repaint();
    }
}

void ProgressionLibraryPanel::onEditExisting()
{
    auto id = getSelectedProgressionId();
    const auto* prog = processorRef.progressionLibrary.getProgression (id);
    if (prog == nullptr)
        return;

    // Copy into pending for editing
    pendingProgression = *prog;
    currentQuantizeResolution = 0.0;
    enterEditing();
}

void ProgressionLibraryPanel::onTranspose (int semitones)
{
    processorRef.stopProgressionPlayback();
    pendingProgression = ProgressionLibrary::transposeProgression (pendingProgression, semitones);

    editChart.setSelectedChord (0);
    editChart.setProgression (&pendingProgression);
    editChart.repaint();

    // Preview the first chord so the user hears the new key
    if (! pendingProgression.chords.empty())
    {
        const auto& firstChord = pendingProgression.chords[0];
        playChordPreview (firstChord);
        if (onChordPreview)
            onChordPreview (firstChord.midiNotes);
        onEditChordSelected (0);
    }
}

void ProgressionLibraryPanel::onPlayToggle()
{
    if (processorRef.isPlayingProgression())
    {
        processorRef.stopProgressionPlayback();
        playButton.setButtonText ("Play");
        statsChart.setPlayingKey (-1);
        statsPlayingKey = -1;
    }
    else
    {
        auto id = getSelectedProgressionId();
        const auto* prog = processorRef.progressionLibrary.getProgression (id);
        if (prog != nullptr)
        {
            processorRef.startProgressionPlayback (*prog);
            playButton.setButtonText ("Stop");
        }
    }
}

void ProgressionLibraryPanel::onEditPlayToggle()
{
    if (processorRef.isPlayingProgression())
    {
        processorRef.stopProgressionPlayback();
        editPlayBtn.setButtonText ("Play");
    }
    else
    {
        pendingProgression.quantizeResolution = currentQuantizeResolution;
        processorRef.startProgressionPlayback (pendingProgression);
        editPlayBtn.setButtonText ("Stop");
    }
}

void ProgressionLibraryPanel::refresh()
{
    updateDisplayedProgressions();
}

juce::String ProgressionLibraryPanel::getSelectedProgressionId() const
{
    int row = progressionList.getSelectedRow();
    if (row >= 0 && row < static_cast<int> (displayedProgressions.size()))
        return displayedProgressions[static_cast<size_t> (row)].id;
    return {};
}

void ProgressionLibraryPanel::updateDisplayedProgressions()
{
    auto searchText = searchEditor.getText().trim().toLowerCase();
    const auto& all = processorRef.progressionLibrary.getAllProgressions();
    displayedProgressions.clear();
    for (const auto& p : all)
    {
        if (searchText.isEmpty() || p.name.toLowerCase().contains (searchText))
            displayedProgressions.push_back (p);
    }
    progressionList.updateContent();
    progressionList.repaint();
}

//==============================================================================
// ListBoxModel
//==============================================================================

int ProgressionLibraryPanel::getNumRows()
{
    return static_cast<int> (displayedProgressions.size());
}

void ProgressionLibraryPanel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                 int width, int height,
                                                 bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int> (displayedProgressions.size()))
        return;

    const auto& p = displayedProgressions[static_cast<size_t> (rowNumber)];

    if (rowIsSelected)
        g.fillAll (juce::Colour (ChordyTheme::bgSelected));
    else
        g.fillAll (juce::Colour (ChordyTheme::bgSurface));

    g.setColour (juce::Colour (ChordyTheme::textPrimary));
    g.setFont (14.0f);
    g.drawText (p.name, 8, 0, width - 80, height, juce::Justification::centredLeft);

    juce::String badge = ChordDetector::noteNameFromPitchClass (p.keyPitchClass)
                         + " " + p.mode.substring (0, 3)
                         + " (" + juce::String (p.chords.size()) + ")";
    g.setColour (juce::Colour (ChordyTheme::textSecondary));
    g.setFont (11.0f);
    g.drawText (badge, width - 90, 0, 82, height, juce::Justification::centredRight);
}

void ProgressionLibraryPanel::refreshStatsChart()
{
    auto id = getSelectedProgressionId();
    if (id.isNotEmpty())
        statsChart.setStats (processorRef.spacedRepetition.getStatsForVoicing (id));
    else
        statsChart.clearStats();
}

void ProgressionLibraryPanel::selectedRowsChanged (int)
{
    auto id = getSelectedProgressionId();
    refreshStatsChart();

    if (onSelectionChanged)
        onSelectionChanged (id);
}
