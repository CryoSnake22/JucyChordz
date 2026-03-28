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
  bool keyPressed (const juce::KeyPress& key) override;

private:
  void timerCallback() override;

  ChordyLookAndFeel chordyLookAndFeel;
  AudioPluginAudioProcessor &processorRef;

  // Header
  juce::Label titleLabel;

  // Chord display
  juce::Label chordDisplayLabel;
  juce::Label nextRootLabel;
  juce::Label practiceRootLabel;   // practice target root (left side)
  juce::Label countdownLabel;      // countdown (below practice root)

  // Keyboard
  ChordyKeyboardComponent keyboard;

  // Panels — tabbed library on left, practice on right
  juce::TabbedComponent libraryTabs { juce::TabbedButtonBar::TabsAtTop };
  VoicingLibraryPanel voicingLibraryPanel;
  ProgressionLibraryPanel progressionLibraryPanel;
  MelodyLibraryPanel melodyLibraryPanel;
  PracticePanel practicePanel;

  // Tempo bar
  juce::TextButton settingsToggle { "Settings" };
  bool settingsExpanded = true;
  juce::Slider bpmSlider;
  juce::Label bpmLabel;
  juce::ToggleButton metronomeToggle { "Click" };
  juce::ToggleButton hostSyncToggle { "Sync" };
  BeatIndicatorComponent beatIndicator;

  // Instrument mode
  juce::ComboBox instrumentModeCombo;
  juce::ComboBox pluginSelector;
  juce::TextButton rescanButton { "Scan" };
  juce::TextButton editPluginButton { "Open" };
  juce::Slider synthVolumeSlider;
  juce::Slider metronomeVolumeSlider;
  bool isStandaloneMode = false;

  // Hosted plugin editor window
  class PluginEditorWindow : public juce::DocumentWindow
  {
  public:
      PluginEditorWindow (juce::AudioProcessorEditor* editor)
          : juce::DocumentWindow (editor->getAudioProcessor()->getName(),
                                   juce::Colour (0xFF1E1E1E),
                                   juce::DocumentWindow::closeButton)
      {
          setContentOwned (editor, true);
          setResizable (true, false);
          centreWithSize (getWidth(), getHeight());
          setVisible (true);
          setAlwaysOnTop (true);
      }

      void closeButtonPressed() override { setVisible (false); }
  };

  std::unique_ptr<PluginEditorWindow> pluginEditorWindow;
  void openPluginEditor();
  void closePluginEditor();

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> metronomeAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> synthVolumeAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> metronomeVolumeAttachment;

  // Tab tracking — deselect when switching tabs
  int lastTabIndex = 0;

  // Voicing preview playback
  std::vector<int> previewNotes;
  int previewFramesRemaining = 0;
  static constexpr int previewDurationFrames = 30; // ~0.5s hold, then release envelope fades

  void startVoicingPreview (const std::vector<int>& notes, const std::vector<int>& velocities = {});
  void stopVoicingPreview();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
