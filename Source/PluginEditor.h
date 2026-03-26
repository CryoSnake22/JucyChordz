#pragma once

#include "PluginProcessor.h"
#include "ChordyLookAndFeel.h"
#include "ChordyKeyboardComponent.h"
#include "VoicingLibraryPanel.h"
#include "PracticePanel.h"
#include "BeatIndicatorComponent.h"
#include "PlaceholderPanel.h"
#include "ProgressionLibraryPanel.h"
#include "MelodyLibraryPanel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class AudioPluginAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer {
public:
  explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor &);
  ~AudioPluginAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override;

  ChordyLookAndFeel chordyLookAndFeel;
  AudioPluginAudioProcessor &processorRef;

  // Header
  juce::Label titleLabel;

  // Chord display
  juce::Label chordDisplayLabel;
  juce::Label nextRootLabel;

  // Keyboard
  ChordyKeyboardComponent keyboard;

  // Panels — tabbed library on left, practice on right
  juce::TabbedComponent libraryTabs { juce::TabbedButtonBar::TabsAtTop };
  VoicingLibraryPanel voicingLibraryPanel;
  ProgressionLibraryPanel progressionLibraryPanel;
  MelodyLibraryPanel melodyLibraryPanel;
  PracticePanel practicePanel;

  // Tempo bar
  juce::Slider bpmSlider;
  juce::Label bpmLabel;
  juce::ToggleButton metronomeToggle { "Click" };
  juce::ToggleButton hostSyncToggle { "Sync" };
  BeatIndicatorComponent beatIndicator;

  // Synth controls
  juce::ToggleButton synthToggle { "Synth" };
  juce::Slider synthVolumeSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> metronomeAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> synthToggleAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> synthVolumeAttachment;

  // Tab tracking — deselect when switching tabs
  int lastTabIndex = 0;

  // Voicing preview playback
  std::vector<int> previewNotes;
  int previewFramesRemaining = 0;
  static constexpr int previewDurationFrames = 30; // ~0.5s hold, then release envelope fades

  void startVoicingPreview (const std::vector<int>& notes);
  void stopVoicingPreview();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
