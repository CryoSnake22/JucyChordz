#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"practiceMode", 1}, "Practice Mode", false));

  layout.add(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID{"midiChannel", 1}, "MIDI Channel", 1, 16, 1));

  // Tempo engine parameters
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"bpm", 1}, "BPM",
      juce::NormalisableRange<float>(30.0f, 300.0f, 0.1f), 120.0f));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"metronomeOn", 1}, "Metronome", false));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"useHostSync", 1}, "Sync to Host", false));

  // Timed practice parameters
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"timedPractice", 1}, "Timed Practice", false));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"responseWindowBeats", 1}, "Response Window (beats)",
      juce::NormalisableRange<float>(1.0f, 8.0f, 0.5f), 4.0f));

  // Internal synth parameters
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"synthEnabled", 1}, "Synth", true));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"synthVolume", 1}, "Synth Volume",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

  return layout;
}

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      apvts(*this, nullptr, "CHORDY_STATE", createParameterLayout()) {
  tempoEngine.connectParameters(
      apvts.getRawParameterValue("bpm"),
      apvts.getRawParameterValue("metronomeOn"),
      apvts.getRawParameterValue("useHostSync"),
      apvts.getRawParameterValue("responseWindowBeats"));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.5; }

int AudioPluginAudioProcessor::getNumPrograms() {
  return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index,
                                                  const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate,
                                              int samplesPerBlock) {
  tempoEngine.prepare(sampleRate, samplesPerBlock);
  internalSynth.setCurrentPlaybackSampleRate(sampleRate);
}

void AudioPluginAudioProcessor::releaseResources() {}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  // Feed MIDI into keyboard state (bridges audio thread → GUI thread).
  // injectIndirectEvents=true allows on-screen keyboard clicks to generate MIDI output.
  keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(),
                                      true);

  // Update atomic note bitfield for lock-free GUI reads
  int channel = static_cast<int>(*apvts.getRawParameterValue("midiChannel"));
  uint64_t low = 0, high = 0;
  for (int note = 0; note < 128; ++note) {
    if (keyboardState.isNoteOn(channel, note)) {
      if (note < 64)
        low |= (uint64_t(1) << note);
      else
        high |= (uint64_t(1) << (note - 64));
    }
  }
  activeNotesLow.store(low, std::memory_order_relaxed);
  activeNotesHigh.store(high, std::memory_order_relaxed);

  // Persist last-played notes (only overwrite when notes are actually held)
  if (low != 0 || high != 0) {
    std::lock_guard<std::mutex> guard(lastPlayedNotesMutex);
    lastPlayedNotes.clear();
    for (int i = 0; i < 64; ++i)
      if (low & (uint64_t(1) << i))
        lastPlayedNotes.push_back(i);
    for (int i = 0; i < 64; ++i)
      if (high & (uint64_t(1) << i))
        lastPlayedNotes.push_back(i + 64);
  }

  // Merge preview MIDI (voicing preview from GUI thread)
  {
    juce::SpinLock::ScopedLockType lock(previewMidiLock);
    if (! previewMidiBuffer.isEmpty()) {
      for (const auto meta : previewMidiBuffer)
        midiMessages.addEvent(meta.getMessage(), meta.samplePosition);
      previewMidiBuffer.clear();
    }
  }

  // Internal synth — render MIDI as audio (Rhodes-like FM)
  if (*apvts.getRawParameterValue("synthEnabled") > 0.5f) {
    internalSynth.setVolume(*apvts.getRawParameterValue("synthVolume"));
    internalSynth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
  }

  // Advance tempo engine — renders metronome click into buffer
  tempoEngine.process(buffer, getPlayHead(), buffer.getNumSamples());
}

std::vector<int> AudioPluginAudioProcessor::getActiveNotes() const {
  std::vector<int> notes;
  uint64_t low = activeNotesLow.load(std::memory_order_relaxed);
  uint64_t high = activeNotesHigh.load(std::memory_order_relaxed);

  for (int i = 0; i < 64; ++i)
    if (low & (uint64_t(1) << i))
      notes.push_back(i);

  for (int i = 0; i < 64; ++i)
    if (high & (uint64_t(1) << i))
      notes.push_back(i + 64);

  return notes;
}

std::vector<int> AudioPluginAudioProcessor::getLastPlayedNotes() const {
  std::lock_guard<std::mutex> guard(lastPlayedNotesMutex);
  return lastPlayedNotes;
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
  return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  auto state = apvts.copyState();

  // Append voicing library and SR state as children
  state.removeChild(state.getChildWithName("VoicingLibrary"), nullptr);
  state.removeChild(state.getChildWithName("SpacedRepetition"), nullptr);
  state.appendChild(voicingLibrary.toValueTree(), nullptr);
  state.appendChild(spacedRepetition.toValueTree(), nullptr);

  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data,
                                                    int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (xml != nullptr && xml->hasTagName(apvts.state.getType())) {
    auto state = juce::ValueTree::fromXml(*xml);

    // Restore voicing library and SR state from children
    auto vlTree = state.getChildWithName("VoicingLibrary");
    if (vlTree.isValid())
      voicingLibrary.fromValueTree(vlTree);

    auto srTree = state.getChildWithName("SpacedRepetition");
    if (srTree.isValid())
      spacedRepetition.fromValueTree(srTree);

    apvts.replaceState(state);
  }
}

void AudioPluginAudioProcessor::addPreviewMidi (const juce::MidiMessage& msg) {
  juce::SpinLock::ScopedLockType lock(previewMidiLock);
  previewMidiBuffer.addEvent(msg, 0);
}

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new AudioPluginAudioProcessor();
}
