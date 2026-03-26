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
      practicePanel(p, keyboard) {

  setLookAndFeel(&chordyLookAndFeel);
  juce::LookAndFeel::setDefaultLookAndFeel(&chordyLookAndFeel);

  // Title
  titleLabel.setText("CHORDY", juce::dontSendNotification);
  titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
  titleLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textPrimary));
  addAndMakeVisible(titleLabel);

  // Chord display
  chordDisplayLabel.setText("Play something...", juce::dontSendNotification);
  chordDisplayLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
  chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textPrimary));
  chordDisplayLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(chordDisplayLabel);

  nextRootLabel.setFont(juce::FontOptions(16.0f));
  nextRootLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textTertiary));
  nextRootLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(nextRootLabel);

  // Keyboard — wider keys, proper proportions
  keyboard.setAvailableRange(36, 96); // C2 to C7
  keyboard.setKeyWidth(28.0f);
  addAndMakeVisible(keyboard);

  // Voicing library panel
  voicingLibraryPanel.onSelectionChanged = [this](const juce::String &voicingId) {
    // Always track the selection so Start uses the right voicing
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
        auto notes = VoicingLibrary::transposeToKey(*v, v->octaveReference);
        for (int note : notes)
          keyboard.setKeyColour(note, KeyColour::Target);
        startVoicingPreview(notes);
      }
      keyboard.repaint();
    } else {
      keyboard.clearAllColours();
      keyboard.repaint();
    }
  };
  // Tabbed library panel
  libraryTabs.addTab("Voicings", juce::Colour(ChordyTheme::bgSurface), &voicingLibraryPanel, false);
  libraryTabs.addTab("Progressions", juce::Colour(ChordyTheme::bgSurface), &progressionLibraryPanel, false);
  libraryTabs.addTab("Melodies", juce::Colour(ChordyTheme::bgSurface), &melodiesPanel, false);
  libraryTabs.setTabBarDepth(28);
  libraryTabs.setOutline(0);
  addAndMakeVisible(libraryTabs);

  // Practice panel
  addAndMakeVisible(practicePanel);

  // Tempo bar
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

  // Synth toggle + volume
  addAndMakeVisible(synthToggle);
  synthToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      processorRef.apvts, "synthEnabled", synthToggle);

  synthVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  synthVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  synthVolumeSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ChordyTheme::textSecondary));
  addAndMakeVisible(synthVolumeSlider);
  synthVolumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      processorRef.apvts, "synthVolume", synthVolumeSlider);

  setSize(1000, 660);
  startTimerHz(60);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
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

  // Chord display — big current root + smaller "up next"
  auto chordArea = area.removeFromTop(80);
  auto mainChordArea = chordArea.removeFromTop(52);
  chordDisplayLabel.setBounds(mainChordArea.reduced(10, 0));
  nextRootLabel.setBounds(chordArea.reduced(10, 0));

  // Keyboard
  auto keyboardArea = area.removeFromTop(140);
  keyboard.setBounds(keyboardArea.reduced(4));

  // Tempo bar
  auto tempoArea = area.removeFromTop(36).reduced(8, 2);
  bpmLabel.setBounds(tempoArea.removeFromLeft(36));
  bpmSlider.setBounds(tempoArea.removeFromLeft(140));
  tempoArea.removeFromLeft(8);
  metronomeToggle.setBounds(tempoArea.removeFromLeft(80));
  tempoArea.removeFromLeft(4);
  hostSyncToggle.setBounds(tempoArea.removeFromLeft(75));
  tempoArea.removeFromLeft(8);
  synthToggle.setBounds(tempoArea.removeFromLeft(75));
  tempoArea.removeFromLeft(4);
  synthVolumeSlider.setBounds(tempoArea.removeFromLeft(70));
  tempoArea.removeFromLeft(8);
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

  // During practice, override chord display with current + next root
  if (practicePanel.isPracticing()) {
    auto rootText = practicePanel.getCurrentRootText();
    chordDisplayLabel.setText(rootText, juce::dontSendNotification);
    chordDisplayLabel.setColour(juce::Label::textColourId,
                                 practicePanel.getCurrentRootColour());
    chordDisplayLabel.setFont(juce::FontOptions(48.0f, juce::Font::bold));

    auto nextText = practicePanel.getNextRootText();
    if (nextText.isNotEmpty()) {
      nextRootLabel.setText("Up next...  " + nextText, juce::dontSendNotification);
      nextRootLabel.setVisible(true);
    } else {
      nextRootLabel.setText("", juce::dontSendNotification);
      nextRootLabel.setVisible(false);
    }
  } else {
    chordDisplayLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
    nextRootLabel.setVisible(false);
    // Normal chord display — use last-played notes when keys are released
    chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(ChordyTheme::textPrimary));
    auto displayNotes = notes.empty() ? processorRef.getLastPlayedNotes() : notes;

    if (displayNotes.empty()) {
      chordDisplayLabel.setText("Play something...", juce::dontSendNotification);
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

  // Clear voicing preview highlight when user plays notes (not during practice/preview)
  if (!notes.empty() && !practicePanel.isPracticing() && previewNotes.empty()) {
    keyboard.clearAllColours();
    keyboard.repaint();
  }

  // Update recording state machines
  voicingLibraryPanel.updateRecording(notes);
  progressionLibraryPanel.updateRecording(notes);
  progressionLibraryPanel.updateTimerCallback();

  // Highlight keyboard during progression playback
  if (processorRef.isPlayingProgression() && !practicePanel.isPracticing() && previewNotes.empty()) {
    uint64_t pLow = processorRef.playbackNotesLow.load(std::memory_order_relaxed);
    uint64_t pHigh = processorRef.playbackNotesHigh.load(std::memory_order_relaxed);
    keyboard.clearAllColours();
    for (int i = 0; i < 64; ++i)
      if (pLow & (uint64_t(1) << i))
        keyboard.setKeyColour(i, KeyColour::Correct);
    for (int i = 0; i < 64; ++i)
      if (pHigh & (uint64_t(1) << i))
        keyboard.setKeyColour(i + 64, KeyColour::Correct);
    keyboard.repaint();
  }

  // Update practice mode
  if (practicePanel.isPracticing()) {
    practicePanel.updatePractice(notes);
    voicingLibraryPanel.refreshStatsChart();
  }

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

  // Disable BPM slider when syncing to host
  bpmSlider.setEnabled(! hostSyncToggle.getToggleState());
}

void AudioPluginAudioProcessorEditor::startVoicingPreview (const std::vector<int>& notes)
{
  stopVoicingPreview();

  int channel = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));

  // Send MIDI directly to synth (bypasses keyboardState — no keyboard flash)
  for (int note : notes)
    processorRef.addPreviewMidi (juce::MidiMessage::noteOn (channel, note, 0.8f));

  // Show green highlights on keyboard
  keyboard.clearAllColours();
  for (int note : notes)
    keyboard.setKeyColour (note, KeyColour::Correct);
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
    keyboard.setKeyColour (note, KeyColour::Correct);
  keyboard.repaint();

  previewNotes.clear();
  previewFramesRemaining = 0;
}
