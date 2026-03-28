#include "PluginEditor.h"
#include "ChordDetector.h"
#include "ChordyTheme.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.keyboardState,
               juce::MidiKeyboardComponent::horizontalKeyboard),
      voicingLibraryPanel(p),
      progressionLibraryPanel(p),
      melodyLibraryPanel(p),
      practicePanel(p, keyboard) {

  setLookAndFeel(&chordyLookAndFeel);
  juce::LookAndFeel::setDefaultLookAndFeel(&chordyLookAndFeel);

  // Title
  titleLabel.setText("CHORDLAB", juce::dontSendNotification);
  titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
  titleLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textPrimary));
  addAndMakeVisible(titleLabel);

  // Chord display
  chordDisplayLabel.setText("...", juce::dontSendNotification);
  chordDisplayLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
  chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textPrimary));
  chordDisplayLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(chordDisplayLabel);

  nextRootLabel.setFont(juce::FontOptions(16.0f));
  nextRootLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textTertiary));
  nextRootLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(nextRootLabel);

  practiceRootLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
  practiceRootLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::accent));
  practiceRootLabel.setJustificationType(juce::Justification::centredLeft);
  practiceRootLabel.setVisible(false);
  addAndMakeVisible(practiceRootLabel);

  countdownLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
  countdownLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textTertiary));
  countdownLabel.setJustificationType(juce::Justification::centredLeft);
  countdownLabel.setVisible(false);
  addAndMakeVisible(countdownLabel);

  // Keyboard — wider keys, proper proportions
  keyboard.setAvailableRange(36, 96); // C2 to C7
  keyboard.setKeyWidth(28.0f);
  addAndMakeVisible(keyboard);

  // Voicing library panel
  voicingLibraryPanel.onSelectionChanged = [this](const juce::String &voicingId) {
    // Only switch practice type to voicing if the voicings tab is active
    if (libraryTabs.getCurrentTabIndex() == 0)
      practicePanel.setSelectedVoicingId(voicingId);

    if (voicingId.isNotEmpty() && practicePanel.isPracticing()) {
      // Switch to practicing the newly selected voicing
      practicePanel.stopPractice();
      // Turn metronome back on for the new voicing (only if timed mode)
      if (practicePanel.isTimedMode())
        if (auto* param = processorRef.apvts.getParameter("metronomeOn"))
          param->setValueNotifyingHost(1.0f);
      practicePanel.startPractice(voicingId);
    } else if (voicingId.isNotEmpty()) {
      // Preview: highlight original voicing notes on keyboard + play via synth
      keyboard.clearAllColours();
      const auto *v = processorRef.voicingLibrary.getVoicing(voicingId);
      if (v != nullptr) {
        auto vnotes = VoicingLibrary::transposeToKey(*v, v->octaveReference);
        for (int note : vnotes)
          keyboard.setKeyColour(note, KeyColour::Target);
        startVoicingPreview(vnotes, v->velocities);
        practicePanel.setClickedChordName(v->name, 40);
      }
      keyboard.repaint();
    } else {
      keyboard.clearAllColours();
      keyboard.repaint();
    }
  };
  // Stats chart key preview → play voicing in clicked key
  voicingLibraryPanel.onKeyPreview = [this](const std::vector<int>& vnotes, const std::vector<int>& velocities) {
    startVoicingPreview(vnotes, velocities);
    auto result = ChordDetector::detect(vnotes);
    if (result.isValid())
      practicePanel.setClickedChordName(result.displayName, 40);
  };

  // Progression chord preview → highlight keyboard
  progressionLibraryPanel.onChordPreview = [this](const std::vector<int>& midiNotes) {
    keyboard.clearAllColours();
    for (int note : midiNotes)
      keyboard.setKeyColour(note, KeyColour::Target);
    keyboard.repaint();
  };

  // Progression selection → practice panel
  progressionLibraryPanel.onSelectionChanged = [this](const juce::String& progressionId) {
    keyboard.clearAllColours();
    keyboard.repaint();
    if (libraryTabs.getCurrentTabIndex() == 1)
    {
      practicePanel.setSelectedProgressionId(progressionId);
      if (progressionId.isNotEmpty())
      {
        const auto* prog = processorRef.progressionLibrary.getProgression(progressionId);
        practicePanel.showProgressionPreview(prog);
      }
      else
      {
        practicePanel.clearChartPreview();
      }
    }
  };

  // Stats chart transposed playback → update preview to transposed version
  progressionLibraryPanel.onTransposedPreview = [this](const Progression& transposed) {
    practicePanel.showProgressionPreview(&transposed);
  };

  // Tabbed library panel
  libraryTabs.addTab("Voicings", juce::Colour(ChordyTheme::bgSurface), &voicingLibraryPanel, false);
  libraryTabs.addTab("Progressions", juce::Colour(ChordyTheme::bgSurface), &progressionLibraryPanel, false);
  // Melody selection → practice panel
  melodyLibraryPanel.onSelectionChanged = [this](const juce::String& melodyId) {
    keyboard.clearAllColours();
    keyboard.repaint();
    if (libraryTabs.getCurrentTabIndex() == 2)
    {
      practicePanel.setSelectedMelodyId(melodyId);
      if (melodyId.isNotEmpty())
      {
        const auto* mel = processorRef.melodyLibrary.getMelody(melodyId);
        practicePanel.showMelodyPreview(mel);
      }
      else
      {
        practicePanel.clearChartPreview();
      }
    }
  };

  melodyLibraryPanel.onTransposedPreview = [this](const Melody& transposed) {
    practicePanel.showMelodyPreview(&transposed);
  };

  // Melody note preview → highlight keyboard
  melodyLibraryPanel.onNotePreview = [this](const std::vector<int>& midiNotes) {
    keyboard.clearAllColours();
    for (int note : midiNotes)
      keyboard.setKeyColour(note, KeyColour::Target);
    keyboard.repaint();
  };

  libraryTabs.addTab("Melodies", juce::Colour(ChordyTheme::bgSurface), &melodyLibraryPanel, false);
  libraryTabs.setTabBarDepth(28);
  libraryTabs.setOutline(0);
  addAndMakeVisible(libraryTabs);

  // Record button on any panel → clear keyboard + stop playback
  auto onRecord = [this] {
    keyboard.clearAllColours();
    keyboard.repaint();
    if (processorRef.isPlayingProgression()) processorRef.stopProgressionPlayback();
    if (processorRef.isPlayingMelody()) processorRef.stopMelodyPlayback();
    stopVoicingPreview();
  };
  voicingLibraryPanel.onRecordStarted = onRecord;
  progressionLibraryPanel.onRecordStarted = onRecord;
  melodyLibraryPanel.onRecordStarted = onRecord;

  // Practice panel
  addAndMakeVisible(practicePanel);

  // Tempo bar — settings toggle
  settingsToggle.setButtonText("Hide Settings");
  settingsToggle.onClick = [this] {
    settingsExpanded = ! settingsExpanded;
    settingsToggle.setButtonText(settingsExpanded ? "Hide Settings" : "Settings");
    resized();
  };
  addAndMakeVisible(settingsToggle);

  bpmLabel.setText("BPM:", juce::dontSendNotification);
  bpmLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
  bpmLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textSecondary));
  addAndMakeVisible(bpmLabel);

  bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 24);
  bpmSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ChordyTheme::textSecondary));
  addAndMakeVisible(bpmSlider);
  bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      processorRef.apvts, "bpm", bpmSlider);

  // Toggle colors inherited from LookAndFeel
  addAndMakeVisible(metronomeToggle);
  metronomeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      processorRef.apvts, "metronomeOn", metronomeToggle);

  // Toggle colors inherited from LookAndFeel
  addAndMakeVisible(hostSyncToggle);
  hostSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      processorRef.apvts, "useHostSync", hostSyncToggle);

  addAndMakeVisible(beatIndicator);

  // Instrument mode combo
  isStandaloneMode = (processorRef.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

  instrumentModeCombo.addItem("Internal", 1);
  instrumentModeCombo.addItem("External", 2);
  instrumentModeCombo.setSelectedId(
      *processorRef.apvts.getRawParameterValue("synthEnabled") > 0.5f ? 1 : 2,
      juce::dontSendNotification);
  instrumentModeCombo.onChange = [this] {
    bool internal = (instrumentModeCombo.getSelectedId() == 1);
    if (auto* param = processorRef.apvts.getParameter("synthEnabled"))
      param->setValueNotifyingHost(internal ? 1.0f : 0.0f);

    // Show/hide plugin selector in external mode
    bool showPluginSelector = !internal;
    pluginSelector.setVisible(showPluginSelector);
    rescanButton.setVisible(showPluginSelector);
    editPluginButton.setVisible(showPluginSelector && processorRef.externalInstrument.isPluginLoaded());
    if (internal)
      closePluginEditor();
    resized();
  };
  addAndMakeVisible(instrumentModeCombo);

  // Plugin selector (visible when External mode)
  pluginSelector.setTextWhenNothingSelected("Select instrument...");
  pluginSelector.onChange = [this] {
    int selectedId = pluginSelector.getSelectedId();
    if (selectedId <= 0) return;

    auto instruments = processorRef.externalInstrument.getAvailableInstruments();
    int idx = selectedId - 1;
    if (idx >= 0 && idx < instruments.size())
      processorRef.externalInstrument.loadPlugin(instruments[idx]);
  };
  pluginSelector.setVisible(*processorRef.apvts.getRawParameterValue("synthEnabled") <= 0.5f);
  addAndMakeVisible(pluginSelector);

  // Edit plugin button (opens hosted plugin's own GUI)
  editPluginButton.onClick = [this] { openPluginEditor(); };
  editPluginButton.setVisible(false);
  addAndMakeVisible(editPluginButton);

  // Rescan button
  rescanButton.onClick = [this] {
    processorRef.externalInstrument.startScan();
    rescanButton.setButtonText("...");
    rescanButton.setEnabled(false);
  };
  rescanButton.setVisible(false);
  addAndMakeVisible(rescanButton);

  // Populate plugin selector from cached scan results
  processorRef.externalInstrument.onPluginListChanged = [this] {
    auto instruments = processorRef.externalInstrument.getAvailableInstruments();
    pluginSelector.clear(juce::dontSendNotification);
    for (int i = 0; i < instruments.size(); ++i)
      pluginSelector.addItem(instruments[i].name, i + 1);
    rescanButton.setButtonText("Scan");
    rescanButton.setEnabled(true);
  };

  processorRef.externalInstrument.onPluginLoaded = [this] {
    // Close stale editor window before updating (prevents dangling pointer)
    closePluginEditor();

    auto name = processorRef.externalInstrument.getPluginName();
    for (int i = 0; i < pluginSelector.getNumItems(); ++i)
    {
      if (pluginSelector.getItemText(i) == name)
      {
        pluginSelector.setSelectedItemIndex(i, juce::dontSendNotification);
        break;
      }
    }
    editPluginButton.setVisible(true);
    resized();
  };

  processorRef.externalInstrument.onPluginLoadFailed = [this](const juce::String& error) {
    juce::ignoreUnused(error);
    pluginSelector.setSelectedId(0, juce::dontSendNotification);
    editPluginButton.setVisible(false);
  };

  // Initial population from cache
  {
    auto instruments = processorRef.externalInstrument.getAvailableInstruments();
    for (int i = 0; i < instruments.size(); ++i)
      pluginSelector.addItem(instruments[i].name, i + 1);
  }

  synthVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  synthVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  synthVolumeSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ChordyTheme::textSecondary));
  addAndMakeVisible(synthVolumeSlider);
  synthVolumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      processorRef.apvts, "synthVolume", synthVolumeSlider);

  metronomeVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  metronomeVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  metronomeVolumeSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ChordyTheme::textSecondary));
  addAndMakeVisible(metronomeVolumeSlider);
  metronomeVolumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      processorRef.apvts, "metronomeVolume", metronomeVolumeSlider);

  setSize(1100, 740);
  startTimerHz(60);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
  closePluginEditor();
  juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
  setLookAndFeel(nullptr);
  stopTimer();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(ChordyTheme::bgDeepest));

  // Section separator lines
  auto separatorColour = juce::Colour(ChordyTheme::border).withAlpha(0.5f);
  g.setColour(separatorColour);

  int chordBottom = 40 + 80;
  int keyboardBottom = chordBottom + 140;
  int tempoBottom = keyboardBottom + 36;

  g.drawHorizontalLine(chordBottom, 0.0f, (float)getWidth());
  g.drawHorizontalLine(keyboardBottom, 0.0f, (float)getWidth());
  g.drawHorizontalLine(tempoBottom, 0.0f, (float)getWidth());
}

void AudioPluginAudioProcessorEditor::resized() {
  auto area = getLocalBounds();

  // Header
  auto headerArea = area.removeFromTop(40);
  titleLabel.setBounds(headerArea.reduced(10, 5));

  // Chord display — chord detection always centered, practice root overlaid on left
  auto chordArea = area.removeFromTop(80);
  auto mainChordArea = chordArea.removeFromTop(52);

  // Chord label spans full center
  chordDisplayLabel.setBounds(mainChordArea.reduced(10, 0));

  // Practice root + countdown overlaid on the left (don't shift center)
  practiceRootLabel.setBounds(mainChordArea.removeFromLeft(140).reduced(14, 0));
  countdownLabel.setBounds(chordArea.removeFromLeft(140).reduced(14, 0));
  nextRootLabel.setBounds(chordArea.removeFromLeft(200).reduced(10, 0));

  // Keyboard
  auto keyboardArea = area.removeFromTop(140);
  keyboard.setBounds(keyboardArea.reduced(4));

  // Tempo bar
  auto tempoArea = area.removeFromTop(36).reduced(8, 2);
  settingsToggle.setBounds(tempoArea.removeFromLeft(settingsExpanded ? 95 : 70));
  tempoArea.removeFromLeft(8);

  bool isExternal = *processorRef.apvts.getRawParameterValue("synthEnabled") <= 0.5f;

  if (settingsExpanded)
  {
    bpmLabel.setBounds(tempoArea.removeFromLeft(36));
    bpmSlider.setBounds(tempoArea.removeFromLeft(140));
    tempoArea.removeFromLeft(8);
    metronomeToggle.setBounds(tempoArea.removeFromLeft(60));
    metronomeVolumeSlider.setBounds(tempoArea.removeFromLeft(50));
    tempoArea.removeFromLeft(4);
    hostSyncToggle.setBounds(tempoArea.removeFromLeft(60));
    tempoArea.removeFromLeft(8);
    instrumentModeCombo.setBounds(tempoArea.removeFromLeft(100));
    tempoArea.removeFromLeft(4);
    if (isExternal)
    {
      pluginSelector.setVisible(true);
      pluginSelector.setBounds(tempoArea.removeFromLeft(140));
      tempoArea.removeFromLeft(4);
      rescanButton.setVisible(true);
      rescanButton.setBounds(tempoArea.removeFromLeft(50));
      tempoArea.removeFromLeft(4);
      editPluginButton.setVisible(processorRef.externalInstrument.isPluginLoaded());
      if (editPluginButton.isVisible())
      {
        editPluginButton.setBounds(tempoArea.removeFromLeft(55));
        tempoArea.removeFromLeft(4);
      }
    }
    else
    {
      pluginSelector.setVisible(false);
      rescanButton.setVisible(false);
      editPluginButton.setVisible(false);
    }
    synthVolumeSlider.setBounds(tempoArea.removeFromLeft(70));
    tempoArea.removeFromLeft(8);

    bpmLabel.setVisible(true);
    bpmSlider.setVisible(true);
    metronomeToggle.setVisible(true);
    metronomeVolumeSlider.setVisible(true);
    hostSyncToggle.setVisible(true);
    instrumentModeCombo.setVisible(true);
    synthVolumeSlider.setVisible(true);
  }
  else
  {
    bpmLabel.setVisible(false);
    bpmSlider.setVisible(false);
    metronomeToggle.setVisible(false);
    metronomeVolumeSlider.setVisible(false);
    hostSyncToggle.setVisible(false);
    instrumentModeCombo.setVisible(false);
    pluginSelector.setVisible(false);
    rescanButton.setVisible(false);
    editPluginButton.setVisible(false);
    synthVolumeSlider.setVisible(false);
  }
  beatIndicator.setBounds(tempoArea);

  // Bottom panels: library on left, practice on right
  auto bottomArea = area.reduced(10);
  auto libraryArea = bottomArea.removeFromLeft(getWidth() / 2 - 14);
  bottomArea.removeFromLeft(10);
  libraryTabs.setBounds(libraryArea);
  practicePanel.setBounds(bottomArea);
}

void AudioPluginAudioProcessorEditor::timerCallback() {
  auto notes = processorRef.getActiveNotes();

  // --- Practice root label (left side) ---
  if (practicePanel.isPracticing()) {
    auto rootText = practicePanel.getCurrentRootText();
    if (rootText.isNotEmpty())
      practiceRootLabel.setText("Key: " + rootText, juce::dontSendNotification);
    practiceRootLabel.setColour(juce::Label::textColourId, practicePanel.getCurrentRootColour());
    practiceRootLabel.setVisible(rootText.isNotEmpty());

    auto countdownStr = practicePanel.getCountdownText();
    if (countdownStr.isNotEmpty()) {
      countdownLabel.setText(countdownStr, juce::dontSendNotification);
      countdownLabel.setVisible(true);
    } else {
      countdownLabel.setVisible(false);
    }

    auto nextText = practicePanel.getNextRootText();
    if (nextText.isNotEmpty()) {
      nextRootLabel.setText("Up next: " + nextText, juce::dontSendNotification);
      nextRootLabel.setVisible(true);
    } else {
      nextRootLabel.setVisible(false);
    }
  } else {
    practiceRootLabel.setVisible(false);
    countdownLabel.setVisible(false);
    nextRootLabel.setVisible(false);
  }

  // --- Center chord display — ALWAYS shows what's being played/previewed ---
  chordDisplayLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
  chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textPrimary));

  // Priority: playback chord > clicked chord > keyboard notes > idle
  auto playbackChord = practicePanel.getPlaybackChordName();
  auto clickedChord = practicePanel.getClickedChordName();
  if (playbackChord.isNotEmpty()) {
    chordDisplayLabel.setText(playbackChord, juce::dontSendNotification);
    chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::accent));
  } else if (clickedChord.isNotEmpty()) {
    chordDisplayLabel.setText(clickedChord, juce::dontSendNotification);
    chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::accent));
  } else {
    // Detect from keyboard notes (played by user or highlighted by playback)
    uint64_t pLow = processorRef.playbackNotesLow.load(std::memory_order_relaxed);
    uint64_t pHigh = processorRef.playbackNotesHigh.load(std::memory_order_relaxed);
    bool hasPlaybackNotes = (pLow != 0 || pHigh != 0)
        && (processorRef.isPlayingProgression() || processorRef.isPlayingMelody());

    auto displayNotes = notes.empty() ? processorRef.getLastPlayedNotes() : notes;

    // During playback with no user input, use playback notes for detection
    if (displayNotes.empty() && hasPlaybackNotes) {
      for (int i = 0; i < 64; ++i)
        if (pLow & (uint64_t(1) << i)) displayNotes.push_back(i);
      for (int i = 0; i < 64; ++i)
        if (pHigh & (uint64_t(1) << i)) displayNotes.push_back(i + 64);
    }

    if (displayNotes.empty()) {
      chordDisplayLabel.setText("...", juce::dontSendNotification);
    } else {
      juce::String customName;
      if (processorRef.voicingLibrary.findByNotes(displayNotes, customName) != nullptr) {
        chordDisplayLabel.setText(customName, juce::dontSendNotification);
      } else {
        auto result = ChordDetector::detect(displayNotes);
        if (result.isValid())
          chordDisplayLabel.setText(result.displayName, juce::dontSendNotification);
        else
          chordDisplayLabel.setText("...", juce::dontSendNotification);
      }
    }
  }

  // Tick down clicked chord display timer
  practicePanel.tickClickedChord();

  // Update recording state machines
  voicingLibraryPanel.updateRecording(notes);
  progressionLibraryPanel.updateRecording(notes);
  progressionLibraryPanel.updateTimerCallback();
  melodyLibraryPanel.updateRecording(notes);
  melodyLibraryPanel.updateTimerCallback();

  // Highlight keyboard during progression/melody playback
  if ((processorRef.isPlayingProgression() || processorRef.isPlayingMelody()) && !practicePanel.isPracticing() && previewNotes.empty()) {
    uint64_t pLow = processorRef.playbackNotesLow.load(std::memory_order_relaxed);
    uint64_t pHigh = processorRef.playbackNotesHigh.load(std::memory_order_relaxed);
    keyboard.clearAllColours();
    for (int i = 0; i < 64; ++i)
      if (pLow & (uint64_t(1) << i))
        keyboard.setKeyColour(i, KeyColour::Target);
    for (int i = 0; i < 64; ++i)
      if (pHigh & (uint64_t(1) << i))
        keyboard.setKeyColour(i + 64, KeyColour::Target);
    keyboard.repaint();
  }

  // Update preview chart cursor during playback (non-practice)
  if (!practicePanel.isPracticing()) {
    if (processorRef.isPlayingProgression())
      practicePanel.showProgressionCursor(processorRef.getPlaybackBeatPosition());
    else if (processorRef.isPlayingMelody())
      practicePanel.showMelodyCursor(processorRef.getMelodyPlaybackBeat());
    else {
      practicePanel.showProgressionCursor(-1.0);
      practicePanel.showMelodyCursor(-1.0);
    }
  }

  // Update practice mode
  bool isPracticing = practicePanel.isPracticing();
  if (isPracticing) {
    practicePanel.updatePractice(notes);
    voicingLibraryPanel.refreshStatsChart();
    progressionLibraryPanel.refreshStatsChart();
    melodyLibraryPanel.refreshStatsChart();
  }

  // Disable library panel buttons during practice
  voicingLibraryPanel.setButtonsEnabled(! isPracticing);
  progressionLibraryPanel.setButtonsEnabled(! isPracticing);
  melodyLibraryPanel.setButtonsEnabled(! isPracticing);

  // Voicing preview auto-off
  if (previewFramesRemaining > 0) {
    if (--previewFramesRemaining == 0)
      stopVoicingPreview();
  }

  // Update beat indicator
  beatIndicator.setBeatInfo(
      processorRef.tempoEngine.getBeatNumber(),
      processorRef.tempoEngine.getBeatPhase(),
      processorRef.tempoEngine.getEffectiveBpm());
  beatIndicator.repaint();

  // Detect tab changes — deselect previous tab's selection and stop playback
  int currentTab = libraryTabs.getCurrentTabIndex();
  if (currentTab != lastTabIndex) {
    lastTabIndex = currentTab;
    if (practicePanel.isPracticing())
      practicePanel.stopPractice();
    stopVoicingPreview();
    if (processorRef.isPlayingProgression())
      processorRef.stopProgressionPlayback();
    if (processorRef.isPlayingMelody())
      processorRef.stopMelodyPlayback();
    keyboard.clearAllColours();
    keyboard.repaint();

    if (currentTab == 0) {
      // Switched to Voicings — no chart preview
      auto vid = voicingLibraryPanel.getSelectedVoicingId();
      practicePanel.setSelectedVoicingId(vid);
      practicePanel.clearChartPreview();
    } else if (currentTab == 1) {
      // Switched to Progressions — show chart preview
      auto pid = progressionLibraryPanel.getSelectedProgressionId();
      practicePanel.setSelectedProgressionId(pid);
      const auto* prog = processorRef.progressionLibrary.getProgression(pid);
      practicePanel.showProgressionPreview(prog);
    } else if (currentTab == 2) {
      // Switched to Melodies — show chart preview
      auto mid = melodyLibraryPanel.getSelectedMelodyId();
      practicePanel.setSelectedMelodyId(mid);
      const auto* mel = processorRef.melodyLibrary.getMelody(mid);
      practicePanel.showMelodyPreview(mel);
    }
  }

  // Disable BPM slider when syncing to host
  bpmSlider.setEnabled(! hostSyncToggle.getToggleState());
}

void AudioPluginAudioProcessorEditor::startVoicingPreview (const std::vector<int>& notes,
                                                            const std::vector<int>& velocities)
{
  stopVoicingPreview();

  int channel = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));

  // Send MIDI directly to synth with recorded velocities
  for (size_t i = 0; i < notes.size(); ++i)
  {
    float vel = (i < velocities.size())
        ? static_cast<float>(velocities[i]) / 127.0f
        : 0.8f;
    processorRef.addPreviewMidi (juce::MidiMessage::noteOn (channel, notes[i], vel));
  }

  // Show green highlights on keyboard
  keyboard.clearAllColours();
  for (int note : notes)
    keyboard.setKeyColour (note, KeyColour::Target);
  keyboard.repaint();

  previewNotes = notes;
  previewFramesRemaining = previewDurationFrames;
}

void AudioPluginAudioProcessorEditor::stopVoicingPreview()
{
  if (previewNotes.empty())
    return;

  int channel = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));

  // Send note-offs directly to synth
  for (int note : previewNotes)
    processorRef.addPreviewMidi (juce::MidiMessage::noteOff (channel, note, 0.0f));

  // Keep light green highlights after playback
  keyboard.clearAllColours();
  for (int note : previewNotes)
    keyboard.setKeyColour (note, KeyColour::Target);
  keyboard.repaint();

  previewNotes.clear();
  previewFramesRemaining = 0;
}

void AudioPluginAudioProcessorEditor::openPluginEditor()
{
  auto* plugin = processorRef.externalInstrument.getHostedPlugin();
  if (plugin == nullptr)
    return;

  // Close existing window if open
  closePluginEditor();

  auto* editor = plugin->createEditorIfNeeded();
  if (editor != nullptr)
    pluginEditorWindow = std::make_unique<PluginEditorWindow> (editor);
}

void AudioPluginAudioProcessorEditor::closePluginEditor()
{
  pluginEditorWindow.reset();
}
