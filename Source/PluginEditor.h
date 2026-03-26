#pragma once

#include "PluginProcessor.h"
#include "ChordyKeyboardComponent.h"
#include "VoicingLibraryPanel.h"
#include "PracticePanel.h"
#include "BeatIndicatorComponent.h"
#include "PlaceholderPanel.h"
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

  AudioPluginAudioProcessor &processorRef;

  // Header
  juce::Label titleLabel;

  // Chord display
  juce::Label chordDisplayLabel;

  // Keyboard
  ChordyKeyboardComponent keyboard;

  // Panels — tabbed library on left, practice on right
  juce::TabbedComponent libraryTabs { juce::TabbedButtonBar::TabsAtTop };
  VoicingLibraryPanel voicingLibraryPanel;
  PlaceholderPanel progressionsPanel { "Progressions — coming soon" };
  PlaceholderPanel melodiesPanel { "Melodies — coming soon" };
  PracticePanel practicePanel;

  // Tempo bar
  juce::Slider bpmSlider;
  juce::Label bpmLabel;
  juce::ToggleButton metronomeToggle { "Click" };
  juce::ToggleButton hostSyncToggle { "Sync" };
  BeatIndicatorComponent beatIndicator;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> metronomeAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncAttachment;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
