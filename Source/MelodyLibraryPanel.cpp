#include "MelodyLibraryPanel.h"
#include "PluginProcessor.h"
#include "ChordDetector.h"
#include "ChordyTheme.h"
#include "ProgressionRecorder.h"

static void addQualityItemsMelody (juce::ComboBox& combo)
{
    combo.addItem ("Major",    1);
    combo.addItem ("Minor",    2);
    combo.addItem ("Dom7",     3);
    combo.addItem ("Maj7",     4);
    combo.addItem ("Min7",     5);
    combo.addItem ("Dim",      6);
    combo.addItem ("Dim7",     7);
    combo.addItem ("Aug",      8);
    combo.addItem ("m7b5",     9);
    combo.addItem ("MinMaj7", 10);
    combo.addItem ("Dom9",    11);
    combo.addItem ("Dom7b9",  12);
    combo.addItem ("Dom7#9",  13);
    combo.addItem ("Maj9",    14);
    combo.addItem ("Min9",    15);
    combo.addItem ("Dom13",   16);
    combo.addItem ("Sus4",    17);
    combo.addItem ("N/A",     18);
}

static ChordQuality qualityFromMelodyComboId (int id)
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
        case 11: return ChordQuality::Dom9;
        case 12: return ChordQuality::Dom7b9;
        case 13: return ChordQuality::Dom7sharp9;
        case 14: return ChordQuality::Maj9;
        case 15: return ChordQuality::Min9;
        case 16: return ChordQuality::Dom13;
        case 17: return ChordQuality::Sus4;
        case 18: return ChordQuality::Unknown;
        default: return ChordQuality::Unknown;
    }
}

static int melodyComboIdFromQuality (ChordQuality q)
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
        case ChordQuality::Dom9:        return 11;
        case ChordQuality::Dom7b9:      return 12;
        case ChordQuality::Dom7sharp9:  return 13;
        case ChordQuality::Maj9:        return 14;
        case ChordQuality::Min9:        return 15;
        case ChordQuality::Dom13:       return 16;
        case ChordQuality::Sus4:        return 17;
        default:                        return 18;
    }
}

//==============================================================================

MelodyLibraryPanel::MelodyLibraryPanel (AudioPluginAudioProcessor& processor)
    : processorRef (processor)
{
    auto labelStyle = [](juce::Label& l) {
        l.setFont (juce::FontOptions (ChordyTheme::fontBody));
        l.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textSecondary));
    };

    // --- Idle ---
    headerLabel.setText ("MELODIES", juce::dontSendNotification);
    headerLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addAndMakeVisible (headerLabel);

    recordingIndicator.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    recordingIndicator.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
    recordingIndicator.setJustificationType (juce::Justification::centredRight);
    recordingIndicator.setVisible (false);
    addAndMakeVisible (recordingIndicator);

    melodyList.setModel (this);
    melodyList.setOutlineThickness (1);
    melodyList.setRowHeight (36);
    addAndMakeVisible (melodyList);

    searchEditor.setTextToShowWhenEmpty ("Search...", juce::Colour (ChordyTheme::textTertiary));
    searchEditor.onTextChange = [this] { updateDisplayedMelodies(); };
    addAndMakeVisible (searchEditor);

    updateDisplayedMelodies();

    addChildComponent (chartPreview);

    recordButton.onClick = [this] {
        if (panelState == PanelState::Idle)
            enterCountIn();
        else if (panelState == PanelState::CountIn)
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
        auto id = getSelectedMelodyId();
        const auto* mel = processorRef.melodyLibrary.getMelody (id);
        if (mel == nullptr || mel->notes.empty()) return;

        // Toggle off if same key clicked while playing
        if (processorRef.isPlayingMelody() && statsPlayingKey == keyIndex)
        {
            processorRef.stopMelodyPlayback();
            playButton.setButtonText ("Play");
            statsChart.setPlayingKey (-1);
            statsPlayingKey = -1;
            return;
        }

        if (processorRef.isPlayingMelody())
            processorRef.stopMelodyPlayback();

        int semitones = (keyIndex - mel->keyPitchClass + 12) % 12;
        if (semitones > 6) semitones -= 12;
        auto transposed = MelodyLibrary::transposeMelody (*mel, semitones);
        int keyRoot = 60 + transposed.keyPitchClass;
        processorRef.startMelodyPlayback (transposed, keyRoot);
        playButton.setButtonText ("Stop");
        statsChart.setPlayingKey (keyIndex);
        statsPlayingKey = keyIndex;
        if (onTransposedPreview)
            onTransposedPreview (transposed);
    };
    addAndMakeVisible (statsChart);

    // --- Editing ---
    editHeader.setText ("EDIT MELODY", juce::dontSendNotification);
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

    editPlayButton.onClick = [this] { onEditPlayToggle(); };
    addChildComponent (editPlayButton);

    transposeLabel.setText ("Transpose:", juce::dontSendNotification);
    transposeLabel.setFont (juce::FontOptions (ChordyTheme::fontSmall));
    transposeLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textSecondary));
    addChildComponent (transposeLabel);

    transposeDownBtn.onClick = [this] { onTranspose (-1); };
    addChildComponent (transposeDownBtn);

    transposeUpBtn.onClick = [this] { onTranspose (1); };
    addChildComponent (transposeUpBtn);

    editChart.setEditMode (true);
    editChart.onChordContextSelected = [this](int idx) { onChordContextSelected (idx); };
    editChartViewport.setViewedComponent (&editChart, false);
    editChartViewport.setScrollBarsShown (true, false);
    addChildComponent (editChartViewport);

    // Chord context editing controls
    addChordBtn.onClick = [this] { onAddChordContext(); };
    addChildComponent (addChordBtn);

    delChordBtn.onClick = [this] { onDeleteChordContext(); };
    delChordBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    addChildComponent (delChordBtn);

    ccRootLabel.setText ("Root:", juce::dontSendNotification);
    labelStyle (ccRootLabel);
    addChildComponent (ccRootLabel);

    for (int i = 0; i < 12; ++i)
        ccRootCombo.addItem (ChordDetector::noteNameFromPitchClass (i), i + 1);
    ccRootCombo.onChange = [this] { updateChordContextFromCombos(); };
    addChildComponent (ccRootCombo);

    ccQualityLabel.setText ("Quality:", juce::dontSendNotification);
    labelStyle (ccQualityLabel);
    addChildComponent (ccQualityLabel);

    addQualityItemsMelody (ccQualityCombo);
    ccQualityCombo.onChange = [this] { updateChordContextFromCombos(); };
    addChildComponent (ccQualityCombo);

    ccAlterationsEditor.setTextToShowWhenEmpty ("alt...", juce::Colour (ChordyTheme::textTertiary));
    ccAlterationsEditor.onReturnKey = [this] { updateChordContextFromCombos(); };
    addChildComponent (ccAlterationsEditor);

    editDoneButton.onClick = [this] { onEditDone(); };
    addChildComponent (editDoneButton);

    editCancelButton.onClick = [this] { enterIdle(); };
    addChildComponent (editCancelButton);

    // --- Confirming ---
    confirmHeader.setText ("SAVE MELODY", juce::dontSendNotification);
    confirmHeader.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    confirmHeader.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addChildComponent (confirmHeader);

    confirmNameLabel.setText ("Name:", juce::dontSendNotification);
    labelStyle (confirmNameLabel);
    addChildComponent (confirmNameLabel);

    confirmNameEditor.setTextToShowWhenEmpty ("Melody name...", juce::Colour (ChordyTheme::textTertiary));
    addChildComponent (confirmNameEditor);

    confirmKeyLabel.setText ("Key:", juce::dontSendNotification);
    labelStyle (confirmKeyLabel);
    addChildComponent (confirmKeyLabel);

    for (int i = 0; i < 12; ++i)
        confirmKeyCombo.addItem (ChordDetector::noteNameFromPitchClass (i), i + 1);
    addChildComponent (confirmKeyCombo);

    confirmSaveButton.onClick = [this] { onConfirmSave(); };
    addChildComponent (confirmSaveButton);

    confirmCancelButton.onClick = [this] { enterIdle(); };
    addChildComponent (confirmCancelButton);
}

//==============================================================================

void MelodyLibraryPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (ChordyTheme::bgSurface));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), ChordyTheme::cornerRadius);

    if (panelState == PanelState::CountIn || panelState == PanelState::Recording)
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

void MelodyLibraryPanel::resized()
{
    auto area = getLocalBounds().reduced (ChordyTheme::panelPadding);

    switch (panelState)
    {
        case PanelState::Editing:    layoutEditMode (area); break;
        case PanelState::Confirming: layoutConfirmMode (area); break;
        default:                     layoutIdleMode (area); break;
    }
}

void MelodyLibraryPanel::layoutIdleMode (juce::Rectangle<int> area)
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

    auto statsArea = area.removeFromBottom (60);
    area.removeFromBottom (4);
    statsChart.setBounds (statsArea);

    // Search bar
    auto searchRow = area.removeFromTop (24);
    searchEditor.setBounds (searchRow);
    area.removeFromTop (4);

    melodyList.setBounds (area);
}

void MelodyLibraryPanel::layoutEditMode (juce::Rectangle<int> area)
{
    editHeader.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);

    // Quantize buttons row + Play
    auto quantRow = area.removeFromTop (24);
    int btnW = (quantRow.getWidth() - 16) / 5;
    quantRawBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    quantBeatBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    quantHalfBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    quantQuarterBtn.setBounds (quantRow.removeFromLeft (btnW));
    quantRow.removeFromLeft (4);
    editPlayButton.setBounds (quantRow);
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
    int bwBtn = (buttonRow.getWidth() - 8) / 2;
    editDoneButton.setBounds (buttonRow.removeFromLeft (bwBtn));
    buttonRow.removeFromLeft (8);
    editCancelButton.setBounds (buttonRow);
    area.removeFromBottom (4);

    // Chord context editing area at bottom
    auto ccArea = area.removeFromBottom (54);
    area.removeFromBottom (4);

    auto ccRow1 = ccArea.removeFromTop (24);
    addChordBtn.setBounds (ccRow1.removeFromLeft (80));
    ccRow1.removeFromLeft (4);
    delChordBtn.setBounds (ccRow1.removeFromLeft (36));
    ccRow1.removeFromLeft (8);
    ccRootLabel.setBounds (ccRow1.removeFromLeft (35));
    ccRootCombo.setBounds (ccRow1.removeFromLeft (55));
    ccArea.removeFromTop (4);

    auto ccRow2 = ccArea.removeFromTop (24);
    ccQualityLabel.setBounds (ccRow2.removeFromLeft (50));
    ccQualityCombo.setBounds (ccRow2.removeFromLeft (90));
    ccRow2.removeFromLeft (4);
    ccAlterationsEditor.setBounds (ccRow2);

    // Chart takes remaining space (viewport scrollable, chart sizes to content)
    editChartViewport.setBounds (area);
    editChart.setViewportHeight (area.getHeight());
    int chartW = area.getWidth() - editChartViewport.getScrollBarThickness();
    editChart.setSize (chartW, juce::jmax (area.getHeight(), editChart.getIdealHeight()));
}

void MelodyLibraryPanel::layoutConfirmMode (juce::Rectangle<int> area)
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

    area.removeFromTop (8);
    auto buttonRow = area.removeFromTop (30);
    int bwBtn = (buttonRow.getWidth() - 8) / 2;
    confirmSaveButton.setBounds (buttonRow.removeFromLeft (bwBtn));
    buttonRow.removeFromLeft (8);
    confirmCancelButton.setBounds (buttonRow);
}

//==============================================================================

void MelodyLibraryPanel::setIdleModeVisible (bool v)
{
    headerLabel.setVisible (v);
    searchEditor.setVisible (v);
    melodyList.setVisible (v);
    statsChart.setVisible (v);
    recordButton.setVisible (v);
    playButton.setVisible (v);
    editButton.setVisible (v);
    deleteButton.setVisible (v);
}

void MelodyLibraryPanel::setEditModeVisible (bool v)
{
    editHeader.setVisible (v);
    editChartViewport.setVisible (v);
    quantRawBtn.setVisible (v);
    quantBeatBtn.setVisible (v);
    quantHalfBtn.setVisible (v);
    quantQuarterBtn.setVisible (v);
    editPlayButton.setVisible (v);
    transposeLabel.setVisible (v);
    transposeDownBtn.setVisible (v);
    transposeUpBtn.setVisible (v);
    addChordBtn.setVisible (v);
    delChordBtn.setVisible (v);
    ccRootLabel.setVisible (v);
    ccRootCombo.setVisible (v);
    ccQualityLabel.setVisible (v);
    ccQualityCombo.setVisible (v);
    ccAlterationsEditor.setVisible (v);
    editDoneButton.setVisible (v);
    editCancelButton.setVisible (v);
}

void MelodyLibraryPanel::setConfirmModeVisible (bool v)
{
    confirmHeader.setVisible (v);
    confirmNameLabel.setVisible (v);
    confirmNameEditor.setVisible (v);
    confirmKeyLabel.setVisible (v);
    confirmKeyCombo.setVisible (v);
    confirmSaveButton.setVisible (v);
    confirmCancelButton.setVisible (v);
}

//==============================================================================

bool MelodyLibraryPanel::isEditing() const
{
    return panelState != PanelState::Idle;
}

void MelodyLibraryPanel::enterIdle()
{
    if (panelState == PanelState::Recording)
        processorRef.progressionRecorder.stopRecording();

    if (processorRef.isPlayingMelody())
        processorRef.stopMelodyPlayback();

    panelState = PanelState::Idle;
    pendingMelody = {};
    countInBeatsElapsed = 0;
    lastCountInBeat = -1;

    setEditModeVisible (false);
    setConfirmModeVisible (false);
    setIdleModeVisible (true);
    recordingIndicator.setVisible (false);

    recordButton.setButtonText ("Record");
    recordButton.setColour (juce::TextButton::buttonColourId,
                            getLookAndFeel().findColour (juce::TextButton::buttonColourId));

    resized();
    repaint();
    updateDisplayedMelodies();
}

void MelodyLibraryPanel::setButtonsEnabled (bool enabled)
{
    recordButton.setEnabled (enabled);
    playButton.setEnabled (enabled);
    editButton.setEnabled (enabled);
    deleteButton.setEnabled (enabled);
}

void MelodyLibraryPanel::enterCountIn()
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

    if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
        param->setValueNotifyingHost (1.0f);
    processorRef.tempoEngine.resetBeatPosition();

    repaint();
}

void MelodyLibraryPanel::enterRecording()
{
    panelState = PanelState::Recording;

    double bpm = *processorRef.apvts.getRawParameterValue ("bpm");
    int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    processorRef.progressionRecorder.startRecording (bpm, processorRef.getSampleRate());

    // Inject any currently held notes at beat 0 with their actual velocities
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

void MelodyLibraryPanel::enterEditing()
{
    panelState = PanelState::Editing;

    if (onEditStarted)
        onEditStarted();

    setIdleModeVisible (false);
    setEditModeVisible (true);
    recordingIndicator.setVisible (false);

    if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
        param->setValueNotifyingHost (0.0f);

    quantRawBtn.setToggleState (currentQuantizeResolution == 0.0, juce::dontSendNotification);
    quantBeatBtn.setToggleState (currentQuantizeResolution == 1.0, juce::dontSendNotification);
    quantHalfBtn.setToggleState (currentQuantizeResolution == 0.5, juce::dontSendNotification);
    quantQuarterBtn.setToggleState (currentQuantizeResolution == 0.25, juce::dontSendNotification);

    editPlayButton.setButtonText ("Play");

    editChart.setQuantizeGrid (currentQuantizeResolution);
    editChart.setMelody (&pendingMelody);

    // Clear chord context editing
    ccRootCombo.setSelectedId (0, juce::dontSendNotification);
    ccQualityCombo.setSelectedId (0, juce::dontSendNotification);
    ccAlterationsEditor.clear();

    resized();
    repaint();
}

void MelodyLibraryPanel::enterConfirming()
{
    panelState = PanelState::Confirming;

    setEditModeVisible (false);
    setConfirmModeVisible (true);

    if (pendingMelody.name.isNotEmpty())
        confirmNameEditor.setText (pendingMelody.name);
    else
        confirmNameEditor.setText ("Melody " +
            juce::String (processorRef.melodyLibrary.size() + 1));

    confirmKeyCombo.setSelectedId (pendingMelody.keyPitchClass + 1, juce::dontSendNotification);

    resized();
    repaint();
}

//==============================================================================

void MelodyLibraryPanel::updateRecording (const std::vector<int>& activeNotes)
{
    juce::ignoreUnused (activeNotes);
}

void MelodyLibraryPanel::updateTimerCallback()
{
    // Count-in logic
    if (panelState == PanelState::CountIn)
    {
        int currentBeat = processorRef.tempoEngine.getBeatNumber();
        if (currentBeat != lastCountInBeat)
        {
            lastCountInBeat = currentBeat;
            countInBeatsElapsed++;

            if (countInBeatsElapsed >= 5)
                enterRecording();
            else
                recordingIndicator.setText (juce::String (countInBeatsElapsed) + "...",
                                           juce::dontSendNotification);
        }
    }

    // Melody playback cursor
    if (processorRef.isPlayingMelody())
    {
        double beat = processorRef.getMelodyPlaybackBeat();

        if (panelState == PanelState::Editing)
            editChart.setCursorBeat (beat);
    }
    else
    {
        if (panelState == PanelState::Editing)
            editChart.setCursorBeat (-1.0);

        if (panelState == PanelState::Idle)
        {
            playButton.setButtonText ("Play");
            statsChart.setPlayingKey (-1);
            statsPlayingKey = -1;
        }
        else if (panelState == PanelState::Editing)
            editPlayButton.setButtonText ("Play");
    }

    // Note preview note-off
    if (notePreviewFrames > 0)
    {
        if (--notePreviewFrames == 0 && ! notePreviewNotes.empty())
        {
            int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
            for (int note : notePreviewNotes)
                processorRef.addPreviewMidi (juce::MidiMessage::noteOff (ch, note, 0.0f));
            notePreviewNotes.clear();
        }
    }
}

void MelodyLibraryPanel::onStopRecording()
{
    processorRef.progressionRecorder.stopRecording();

    auto rawMidi = processorRef.progressionRecorder.getRawMidi();
    double totalBeats = processorRef.progressionRecorder.getTotalBeats();
    double bpm = *processorRef.apvts.getRawParameterValue ("bpm");

    // Analyze notes with C4 (60) as initial reference
    analysisKeyRootMidi = 60;
    auto melodyNotes = MelodyLibrary::analyzeMelodyNotes (rawMidi, analysisKeyRootMidi);

    if (melodyNotes.empty())
    {
        enterIdle();
        return;
    }

    pendingMelody = {};
    pendingMelody.id = juce::Uuid().toString();
    pendingMelody.bpm = bpm;
    pendingMelody.totalBeats = totalBeats;
    pendingMelody.rawMidi = rawMidi;
    pendingMelody.keyPitchClass = 0; // C initially, user adjusts in confirming

    // Default to raw (no quantization) — user can quantize in editing step
    currentQuantizeResolution = 0.0;
    pendingMelody.notes = melodyNotes;

    // Round totalBeats up to the nearest bar (4 beats) to preserve trailing silence
    pendingMelody.totalBeats = std::ceil (totalBeats / 4.0) * 4.0;
    if (pendingMelody.totalBeats < 4.0)
        pendingMelody.totalBeats = 4.0;

    // Default chord context: one chord spanning full duration
    MelodyChordContext defaultCC;
    defaultCC.intervalFromKeyRoot = 0;
    defaultCC.quality = ChordQuality::Unknown;
    defaultCC.startBeat = 0.0;
    defaultCC.durationBeats = pendingMelody.totalBeats;
    defaultCC.name = "";
    pendingMelody.chordContexts.push_back (defaultCC);

    enterEditing();
}

void MelodyLibraryPanel::applyQuantize (double resolution)
{
    currentQuantizeResolution = resolution;

    // Always re-analyze from the original raw MIDI recording.
    auto freshNotes = MelodyLibrary::analyzeMelodyNotes (pendingMelody.rawMidi, analysisKeyRootMidi);

    // Raw (0.0) = keep exact timing, otherwise quantize to grid
    if (resolution > 0.0)
        pendingMelody.notes = MelodyLibrary::quantizeMelodyNotes (freshNotes, resolution);
    else
        pendingMelody.notes = freshNotes;

    double maxEnd = 0.0;
    for (const auto& n : pendingMelody.notes)
        maxEnd = juce::jmax (maxEnd, n.startBeat + n.durationBeats);
    pendingMelody.totalBeats = juce::jmax (pendingMelody.totalBeats, maxEnd);

    quantRawBtn.setToggleState (resolution == 0.0, juce::dontSendNotification);
    quantBeatBtn.setToggleState (resolution == 1.0, juce::dontSendNotification);
    quantHalfBtn.setToggleState (resolution == 0.5, juce::dontSendNotification);
    quantQuarterBtn.setToggleState (resolution == 0.25, juce::dontSendNotification);

    editChart.setQuantizeGrid (resolution);
    editChart.setMelody (&pendingMelody);
    editChart.repaint();
}

void MelodyLibraryPanel::onAddChordContext()
{
    if (pendingMelody.chordContexts.empty())
    {
        MelodyChordContext cc;
        cc.intervalFromKeyRoot = 0;
        cc.quality = ChordQuality::Dom7;
        cc.startBeat = 0.0;
        cc.durationBeats = pendingMelody.totalBeats;
        pendingMelody.chordContexts.push_back (cc);
    }
    else
    {
        // Split the last chord in half
        auto& last = pendingMelody.chordContexts.back();
        double midpoint = last.startBeat + last.durationBeats / 2.0;
        double oldDur = last.durationBeats;
        last.durationBeats = midpoint - last.startBeat;

        MelodyChordContext newCC;
        newCC.intervalFromKeyRoot = 0;
        newCC.quality = ChordQuality::Dom7;
        newCC.startBeat = midpoint;
        newCC.durationBeats = (last.startBeat + oldDur) - midpoint;
        pendingMelody.chordContexts.push_back (newCC);
    }

    editChart.setMelody (&pendingMelody);
    editChart.setSelectedChordContext (static_cast<int> (pendingMelody.chordContexts.size()) - 1);
    onChordContextSelected (static_cast<int> (pendingMelody.chordContexts.size()) - 1);
    editChart.repaint();
}

void MelodyLibraryPanel::onDeleteChordContext()
{
    int idx = editChart.getSelectedChordContext();
    if (idx < 0 || idx >= static_cast<int> (pendingMelody.chordContexts.size()))
        return;

    // Don't delete the last chord context
    if (pendingMelody.chordContexts.size() <= 1)
        return;

    pendingMelody.chordContexts.erase (
        pendingMelody.chordContexts.begin() + static_cast<std::ptrdiff_t> (idx));

    editChart.setSelectedChordContext (-1);
    editChart.setMelody (&pendingMelody);
    editChart.repaint();

    ccRootCombo.setSelectedId (0, juce::dontSendNotification);
    ccQualityCombo.setSelectedId (0, juce::dontSendNotification);
    ccAlterationsEditor.clear();
}

void MelodyLibraryPanel::onChordContextSelected (int index)
{
    if (index < 0 || index >= static_cast<int> (pendingMelody.chordContexts.size()))
    {
        ccRootCombo.setSelectedId (0, juce::dontSendNotification);
        ccQualityCombo.setSelectedId (0, juce::dontSendNotification);
        ccAlterationsEditor.clear();
        return;
    }

    const auto& cc = pendingMelody.chordContexts[static_cast<size_t> (index)];
    int rootPc = (pendingMelody.keyPitchClass + cc.intervalFromKeyRoot + 120) % 12;
    ccRootCombo.setSelectedId (rootPc + 1, juce::dontSendNotification);
    ccQualityCombo.setSelectedId (melodyComboIdFromQuality (cc.quality), juce::dontSendNotification);
    ccAlterationsEditor.setText (cc.alterations, juce::dontSendNotification);
}

void MelodyLibraryPanel::updateChordContextFromCombos()
{
    int idx = editChart.getSelectedChordContext();
    if (idx < 0 || idx >= static_cast<int> (pendingMelody.chordContexts.size()))
        return;

    auto& cc = pendingMelody.chordContexts[static_cast<size_t> (idx)];

    int rootPc = ccRootCombo.getSelectedId() - 1;
    if (rootPc >= 0)
        cc.intervalFromKeyRoot = (rootPc - pendingMelody.keyPitchClass + 12) % 12;

    cc.quality = qualityFromMelodyComboId (ccQualityCombo.getSelectedId());
    cc.alterations = ccAlterationsEditor.getText().trim();

    // Rebuild display name
    cc.name = cc.getDisplayName (pendingMelody.keyPitchClass);

    editChart.repaint();
}

void MelodyLibraryPanel::onEditDone()
{
    enterConfirming();
}

void MelodyLibraryPanel::onConfirmSave()
{
    juce::String name = confirmNameEditor.getText().trim();
    if (name.isEmpty())
        name = "Melody " + juce::String (processorRef.melodyLibrary.size() + 1);

    pendingMelody.name = name;

    int newKeyPc = confirmKeyCombo.getSelectedId() - 1;
    if (newKeyPc < 0) newKeyPc = 0;

    // Adjust intervals if key changed from initial analysis reference
    int oldKeyPc = pendingMelody.keyPitchClass;
    if (newKeyPc != oldKeyPc)
    {
        int shift = newKeyPc - oldKeyPc;
        for (auto& note : pendingMelody.notes)
            note.intervalFromKeyRoot -= shift;
        for (auto& cc : pendingMelody.chordContexts)
            cc.intervalFromKeyRoot = (cc.intervalFromKeyRoot - shift + 12) % 12;
    }
    pendingMelody.keyPitchClass = newKeyPc;

    // Rebuild chord context display names
    for (auto& cc : pendingMelody.chordContexts)
        cc.name = cc.getDisplayName (pendingMelody.keyPitchClass);

    // Store the quantize resolution for playback
    pendingMelody.quantizeResolution = currentQuantizeResolution;

    // Remove old version if editing an existing melody (same ID)
    processorRef.melodyLibrary.removeMelody (pendingMelody.id);
    processorRef.melodyLibrary.addMelody (pendingMelody);
    processorRef.saveLibrariesToDisk();
    enterIdle();
}

void MelodyLibraryPanel::onDelete()
{
    auto id = getSelectedMelodyId();
    if (id.isNotEmpty())
    {
        processorRef.melodyLibrary.removeMelody (id);
        processorRef.saveLibrariesToDisk();
        updateDisplayedMelodies();
        repaint();
    }
}

void MelodyLibraryPanel::onEditExisting()
{
    auto id = getSelectedMelodyId();
    const auto* mel = processorRef.melodyLibrary.getMelody (id);
    if (mel == nullptr)
        return;

    pendingMelody = *mel;
    currentQuantizeResolution = 0.0;
    enterEditing();
}

void MelodyLibraryPanel::onTranspose (int semitones)
{
    pendingMelody = MelodyLibrary::transposeMelody (pendingMelody, semitones);
    editChart.setMelody (&pendingMelody);
    editChart.repaint();
}

void MelodyLibraryPanel::onPlayToggle()
{
    if (processorRef.isPlayingMelody())
    {
        processorRef.stopMelodyPlayback();
        playButton.setButtonText ("Play");
        statsChart.setPlayingKey (-1);
        statsPlayingKey = -1;
        return;
    }

    auto id = getSelectedMelodyId();
    const auto* mel = processorRef.melodyLibrary.getMelody (id);
    if (mel == nullptr || mel->notes.empty())
        return;

    int keyRoot = 60 + mel->keyPitchClass;
    processorRef.startMelodyPlayback (*mel, keyRoot);
    playButton.setButtonText ("Stop");
}

void MelodyLibraryPanel::onEditPlayToggle()
{
    if (processorRef.isPlayingMelody())
    {
        processorRef.stopMelodyPlayback();
        editPlayButton.setButtonText ("Play");
        return;
    }

    if (pendingMelody.notes.empty())
        return;

    pendingMelody.quantizeResolution = currentQuantizeResolution;
    int keyRoot = 60 + pendingMelody.keyPitchClass;
    processorRef.startMelodyPlayback (pendingMelody, keyRoot);
    editPlayButton.setButtonText ("Stop");
}

void MelodyLibraryPanel::refresh()
{
    updateDisplayedMelodies();
}

void MelodyLibraryPanel::updateDisplayedMelodies()
{
    auto searchText = searchEditor.getText().trim().toLowerCase();
    const auto& all = processorRef.melodyLibrary.getAllMelodies();
    displayedMelodies.clear();
    for (const auto& m : all)
    {
        if (searchText.isEmpty() || m.name.toLowerCase().contains (searchText))
            displayedMelodies.push_back (m);
    }
    melodyList.updateContent();
    melodyList.repaint();
}

juce::String MelodyLibraryPanel::getSelectedMelodyId() const
{
    int row = melodyList.getSelectedRow();
    if (row >= 0 && row < static_cast<int> (displayedMelodies.size()))
        return displayedMelodies[static_cast<size_t> (row)].id;
    return {};
}

//==============================================================================
// ListBoxModel
//==============================================================================

int MelodyLibraryPanel::getNumRows()
{
    return static_cast<int> (displayedMelodies.size());
}

void MelodyLibraryPanel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                            int width, int height,
                                            bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int> (displayedMelodies.size()))
        return;

    const auto& m = displayedMelodies[static_cast<size_t> (rowNumber)];

    if (rowIsSelected)
        g.fillAll (juce::Colour (ChordyTheme::bgSelected));
    else
        g.fillAll (juce::Colour (ChordyTheme::bgSurface));

    // Name on top line
    g.setColour (juce::Colour (ChordyTheme::textPrimary));
    g.setFont (14.0f);
    g.drawText (m.name, 8, 2, width - 16, 16, juce::Justification::centredLeft);

    // Chord context summary on second line
    auto ctx = m.getChordContextLabel();
    if (ctx.isNotEmpty())
    {
        g.setColour (juce::Colour (ChordyTheme::textTertiary));
        g.setFont (11.0f);
        g.drawText (ctx, 8, 18, width - 16, 14, juce::Justification::centredLeft);
    }
}

void MelodyLibraryPanel::refreshStatsChart()
{
    auto id = getSelectedMelodyId();
    if (id.isNotEmpty())
        statsChart.setStats (processorRef.spacedRepetition.getStatsForVoicing (id));
    else
        statsChart.clearStats();
}

void MelodyLibraryPanel::selectedRowsChanged (int)
{
    auto id = getSelectedMelodyId();
    const auto* mel = processorRef.melodyLibrary.getMelody (id);
    refreshStatsChart();

    if (onSelectionChanged)
        onSelectionChanged (id);
}
