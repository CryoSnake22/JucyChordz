#include "PluginEditor.h"
#include "ChordDetector.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p),
      keyboard(p.keyboardState,
               juce::MidiKeyboardComponent::horizontalKeyboard),
      voicingLibraryPanel(p),
      practicePanel(p, keyboard) {

  // Title
  titleLabel.setText("CHORDY", juce::dontSendNotification);
  titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(titleLabel);

  // Chord display
  chordDisplayLabel.setText("Play something...", juce::dontSendNotification);
  chordDisplayLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
  chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  chordDisplayLabel.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(chordDisplayLabel);

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
      // Turn metronome back on for the new voicing
      if (auto* param = processorRef.apvts.getParameter("metronomeOn"))
        param->setValueNotifyingHost(1.0f);
      practicePanel.startPractice(voicingId);
    } else if (voicingId.isNotEmpty()) {
      // Preview: highlight original voicing notes on keyboard
      keyboard.clearAllColours();
      const auto *v = processorRef.voicingLibrary.getVoicing(voicingId);
      if (v != nullptr) {
        auto notes = VoicingLibrary::transposeToKey(*v, v->octaveReference);
        for (int note : notes)
          keyboard.setKeyColour(note, KeyColour::Target);
      }
      keyboard.repaint();
    } else {
      keyboard.clearAllColours();
      keyboard.repaint();
    }
  };
  addAndMakeVisible(voicingLibraryPanel);

  // Practice panel
  addAndMakeVisible(practicePanel);

  // Tempo bar
  bpmLabel.setText("BPM:", juce::dontSendNotification);
  bpmLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
  bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAABBCC));
  addAndMakeVisible(bpmLabel);

  bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 24);
  bpmSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFAABBCC));
  addAndMakeVisible(bpmSlider);
  bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      processorRef.apvts, "bpm", bpmSlider);

  metronomeToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFFAABBCC));
  metronomeToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF44CC88));
  addAndMakeVisible(metronomeToggle);
  metronomeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      processorRef.apvts, "metronomeOn", metronomeToggle);

  hostSyncToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFFAABBCC));
  hostSyncToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF44CC88));
  addAndMakeVisible(hostSyncToggle);
  hostSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
      processorRef.apvts, "useHostSync", hostSyncToggle);

  addAndMakeVisible(beatIndicator);

  setSize(1000, 660);
  startTimerHz(60);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
  stopTimer();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xFF1A1A2E));
}

void AudioPluginAudioProcessorEditor::resized() {
  auto area = getLocalBounds();

  // Header
  auto headerArea = area.removeFromTop(40);
  titleLabel.setBounds(headerArea.reduced(10, 5));

  // Chord display
  auto chordArea = area.removeFromTop(60);
  chordDisplayLabel.setBounds(chordArea.reduced(10, 5));

  // Keyboard
  auto keyboardArea = area.removeFromTop(140);
  keyboard.setBounds(keyboardArea.reduced(8));

  // Tempo bar
  auto tempoArea = area.removeFromTop(36).reduced(8, 2);
  bpmLabel.setBounds(tempoArea.removeFromLeft(36));
  bpmSlider.setBounds(tempoArea.removeFromLeft(140));
  tempoArea.removeFromLeft(8);
  metronomeToggle.setBounds(tempoArea.removeFromLeft(65));
  tempoArea.removeFromLeft(4);
  hostSyncToggle.setBounds(tempoArea.removeFromLeft(60));
  tempoArea.removeFromLeft(8);
  beatIndicator.setBounds(tempoArea);

  // Bottom panels: library on left, practice on right
  auto bottomArea = area.reduced(8);
  auto libraryArea = bottomArea.removeFromLeft(getWidth() / 2 - 12);
  bottomArea.removeFromLeft(8);
  voicingLibraryPanel.setBounds(libraryArea);
  practicePanel.setBounds(bottomArea);
}

void AudioPluginAudioProcessorEditor::timerCallback() {
  auto notes = processorRef.getActiveNotes();

  // During timed practice, override chord display with practice target
  if (practicePanel.isTimedActive()) {
    auto text = practicePanel.getPracticeDisplayText();
    if (text.isNotEmpty()) {
      chordDisplayLabel.setText(text, juce::dontSendNotification);
      chordDisplayLabel.setColour(juce::Label::textColourId,
                                   practicePanel.getPracticeDisplayColour());
    } else {
      chordDisplayLabel.setText("", juce::dontSendNotification);
    }
  } else {
    // Normal chord display — use last-played notes when keys are released
    chordDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
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

  // Clear voicing preview highlight when user plays notes (not during practice)
  if (!notes.empty() && !practicePanel.isPracticing()) {
    keyboard.clearAllColours();
    keyboard.repaint();
  }

  // Update recording state machine
  voicingLibraryPanel.updateRecording(notes);

  // Update practice mode
  if (practicePanel.isPracticing())
    practicePanel.updatePractice(notes);

  // Update beat indicator
  beatIndicator.setBeatInfo(
      processorRef.tempoEngine.getBeatNumber(),
      processorRef.tempoEngine.getBeatPhase(),
      processorRef.tempoEngine.getEffectiveBpm());
  beatIndicator.repaint();

  // Disable BPM slider when syncing to host
  bpmSlider.setEnabled(! hostSyncToggle.getToggleState());
}
