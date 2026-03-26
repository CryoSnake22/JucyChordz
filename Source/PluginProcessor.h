#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VoicingModel.h"
#include "SpacedRepetition.h"
#include "TempoEngine.h"
#include "ChordySynth.h"
#include <atomic>
#include <mutex>

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor {
public:
  //==============================================================================
  AudioPluginAudioProcessor();
  ~AudioPluginAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
  using AudioProcessor::processBlock;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  //==============================================================================
  // MIDI keyboard state — bridges audio thread and GUI thread
  juce::MidiKeyboardState keyboardState;

  // Lock-free active notes bitfield for GUI thread reads
  std::atomic<uint64_t> activeNotesLow{0};  // notes 0-63
  std::atomic<uint64_t> activeNotesHigh{0}; // notes 64-127

  // Convenience: read active notes as a vector (call from GUI thread)
  std::vector<int> getActiveNotes() const;

  // Last-played notes: persists after key release so Record can capture them
  std::vector<int> getLastPlayedNotes() const;

  // APVTS for automatable parameters
  juce::AudioProcessorValueTreeState apvts;

  // Voicing library and spaced repetition state
  VoicingLibrary voicingLibrary;
  SpacedRepetitionEngine spacedRepetition;

  // Tempo engine (metronome + beat tracking)
  TempoEngine tempoEngine;

  // Internal synth (Rhodes-like FM, for audible playback)
  ChordySynth internalSynth;

  // Preview MIDI injection — GUI thread pushes, audio thread drains into synth
  void addPreviewMidi (const juce::MidiMessage& msg);

private:
  juce::SpinLock previewMidiLock;
  juce::MidiBuffer previewMidiBuffer;
  mutable std::mutex lastPlayedNotesMutex;
  std::vector<int> lastPlayedNotes;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
