#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

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

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"metronomeVolume", 1}, "Metronome Volume",
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
      apvts.getRawParameterValue("responseWindowBeats"),
      apvts.getRawParameterValue("metronomeVolume"));

  externalInstrument.initialize();

  // Load shared libraries from disk (if file exists from a previous session)
  loadLibrariesFromDisk();
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
  saveLibrariesToDisk();
}

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
  externalInstrument.prepare(sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources() {
  externalInstrument.release();
}

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

  // Capture note velocities before keyboardState processes the buffer
  for (const auto meta : midiMessages)
  {
    auto msg = meta.getMessage();
    if (msg.isNoteOn() && msg.getVelocity() > 0)
      noteVelocities[msg.getNoteNumber()].store(
          static_cast<uint8_t>(msg.getVelocity()), std::memory_order_relaxed);
  }

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

  // Forward MIDI to progression recorder if active
  if (progressionRecorder.isRecording())
    progressionRecorder.processBlock(midiMessages, buffer.getNumSamples());

  // Progression playback — replay raw MIDI directly
  if (playbackActive.load(std::memory_order_relaxed)) {
    double bpm = *apvts.getRawParameterValue("bpm");
    double samplesPerBeat = getSampleRate() * 60.0 / bpm;

    for (int s = 0; s < buffer.getNumSamples(); ++s) {
      double beat = playbackSamplePos / samplesPerBeat;
      playbackBeat.store(beat, std::memory_order_relaxed);

      // End of progression
      if (beat >= playbackProgression.totalBeats) {
        // Flush any remaining raw MIDI events at the end boundary
        int ch = channel;
        while (playbackCCIndex < playbackProgression.rawMidi.getNumEvents())
        {
          auto* evt = playbackProgression.rawMidi.getEventPointer(playbackCCIndex);
          auto msg = evt->message;
          midiMessages.addEvent(msg, s);
          if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            playbackActiveNotes.erase(
              std::remove(playbackActiveNotes.begin(), playbackActiveNotes.end(), msg.getNoteNumber()),
              playbackActiveNotes.end());
          playbackCCIndex++;
        }
        // Send pedal off + note-offs at the same sample position
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 0), s);
        for (int n : playbackActiveNotes)
          midiMessages.addEvent(juce::MidiMessage::noteOff(ch, n), s);
        playbackActiveNotes.clear();
        playbackActive.store(false, std::memory_order_relaxed);
        playbackNotesLow.store(0, std::memory_order_relaxed);
        playbackNotesHigh.store(0, std::memory_order_relaxed);
        break;
      }

      // Inject all raw MIDI events whose beat timestamp has been reached
      while (playbackCCIndex < playbackProgression.rawMidi.getNumEvents())
      {
        auto* evt = playbackProgression.rawMidi.getEventPointer(playbackCCIndex);
        if (evt->message.getTimeStamp() > beat)
          break;

        auto msg = evt->message;
        midiMessages.addEvent(msg, s);

        // Track active notes for keyboard highlighting
        if (msg.isNoteOn() && msg.getVelocity() > 0)
          playbackActiveNotes.push_back(msg.getNoteNumber());
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
          playbackActiveNotes.erase(
            std::remove(playbackActiveNotes.begin(), playbackActiveNotes.end(), msg.getNoteNumber()),
            playbackActiveNotes.end());

        playbackCCIndex++;
      }

      // Update keyboard highlighting from active notes
      uint64_t pLow = 0, pHigh = 0;
      for (int n : playbackActiveNotes) {
        if (n < 64) pLow |= (uint64_t(1) << n);
        else        pHigh |= (uint64_t(1) << (n - 64));
      }
      playbackNotesLow.store(pLow, std::memory_order_relaxed);
      playbackNotesHigh.store(pHigh, std::memory_order_relaxed);

      playbackSamplePos += 1.0;
    }
  }

  // Melody playback — replay raw MIDI directly
  if (melodyPlaybackActive.load(std::memory_order_relaxed)) {
    double bpm = *apvts.getRawParameterValue("bpm");
    double samplesPerBeat = getSampleRate() * 60.0 / bpm;

    for (int s = 0; s < buffer.getNumSamples(); ++s) {
      double beat = melodyPlaybackSamplePos / samplesPerBeat;
      melodyPlaybackBeat.store(beat, std::memory_order_relaxed);

      // End of melody
      if (beat >= melodyPlaybackData.totalBeats) {
        int ch = channel;
        // Flush remaining raw MIDI events at the end boundary
        while (melodyPlaybackCCIndex < melodyPlaybackData.rawMidi.getNumEvents())
        {
          auto* evt = melodyPlaybackData.rawMidi.getEventPointer(melodyPlaybackCCIndex);
          auto msg = evt->message;
          midiMessages.addEvent(msg, s);
          if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
          {
            if (msg.getNoteNumber() == melodyPlaybackActiveNote)
              melodyPlaybackActiveNote = -1;
          }
          melodyPlaybackCCIndex++;
        }
        // Send pedal off + note-off at the correct sample position
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 0), s);
        if (melodyPlaybackActiveNote >= 0) {
          midiMessages.addEvent(juce::MidiMessage::noteOff(ch, melodyPlaybackActiveNote), s);
          melodyPlaybackActiveNote = -1;
        }
        melodyPlaybackActive.store(false, std::memory_order_relaxed);
        playbackNotesLow.store(0, std::memory_order_relaxed);
        playbackNotesHigh.store(0, std::memory_order_relaxed);
        break;
      }

      // Inject all raw MIDI events whose beat timestamp has been reached
      while (melodyPlaybackCCIndex < melodyPlaybackData.rawMidi.getNumEvents())
      {
        auto* evt = melodyPlaybackData.rawMidi.getEventPointer(melodyPlaybackCCIndex);
        if (evt->message.getTimeStamp() > beat)
          break;

        auto msg = evt->message;
        midiMessages.addEvent(msg, s);

        // Track active note for keyboard highlighting
        if (msg.isNoteOn() && msg.getVelocity() > 0)
          melodyPlaybackActiveNote = msg.getNoteNumber();
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
        {
          if (msg.getNoteNumber() == melodyPlaybackActiveNote)
            melodyPlaybackActiveNote = -1;
        }

        melodyPlaybackCCIndex++;
      }

      // Update keyboard highlighting
      uint64_t pLow = 0, pHigh = 0;
      if (melodyPlaybackActiveNote >= 0) {
        if (melodyPlaybackActiveNote < 64) pLow |= (uint64_t(1) << melodyPlaybackActiveNote);
        else pHigh |= (uint64_t(1) << (melodyPlaybackActiveNote - 64));
      }
      playbackNotesLow.store(pLow, std::memory_order_relaxed);
      playbackNotesHigh.store(pHigh, std::memory_order_relaxed);

      melodyPlaybackSamplePos += 1.0;
    }
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

  // Instrument rendering — internal synth or external (MIDI pass-through / hosted plugin)
  bool useInternalSynth = *apvts.getRawParameterValue("synthEnabled") > 0.5f;

  if (useInternalSynth) {
    internalSynth.setVolume(*apvts.getRawParameterValue("synthVolume"));
    internalSynth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
  } else {
    // External mode: route to hosted plugin if available, otherwise silence
    if (externalInstrument.processBlock(buffer, midiMessages))
    {
      // Apply volume slider to hosted plugin output
      // Gain boost (3x) compensates for internal synth being much hotter
      // (additive polyphony with no output normalization)
      float vol = *apvts.getRawParameterValue("synthVolume");
      buffer.applyGain(vol * 3.0f);
    }
    else
    {
      buffer.clear();
    }
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

int AudioPluginAudioProcessor::getNoteVelocity (int noteNumber) const {
  if (noteNumber >= 0 && noteNumber < 128)
    return static_cast<int>(noteVelocities[noteNumber].load(std::memory_order_relaxed));
  return 100;
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

  // Append voicing library, SR state, and progression library as children
  state.removeChild(state.getChildWithName("VoicingLibrary"), nullptr);
  state.removeChild(state.getChildWithName("SpacedRepetition"), nullptr);
  state.removeChild(state.getChildWithName("ProgressionLibrary"), nullptr);
  state.removeChild(state.getChildWithName("MelodyLibrary"), nullptr);
  state.removeChild(state.getChildWithName("ExternalInstrument"), nullptr);
  state.appendChild(voicingLibrary.toValueTree(), nullptr);
  state.appendChild(spacedRepetition.toValueTree(), nullptr);
  state.appendChild(progressionLibrary.toValueTree(), nullptr);
  state.appendChild(melodyLibrary.toValueTree(), nullptr);

  // Save external instrument state
  auto extXml = externalInstrument.createStateXml();
  if (extXml != nullptr)
    state.appendChild(juce::ValueTree::fromXml(*extXml), nullptr);

  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);

  // Also sync to shared file so other instances pick up changes
  saveLibrariesToDisk();
}

void AudioPluginAudioProcessor::setStateInformation(const void *data,
                                                    int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (xml != nullptr && xml->hasTagName(apvts.state.getType())) {
    auto state = juce::ValueTree::fromXml(*xml);

    // Restore voicing library and SR state from APVTS state (backward compat)
    auto vlTree = state.getChildWithName("VoicingLibrary");
    if (vlTree.isValid())
      voicingLibrary.fromValueTree(vlTree);

    auto srTree = state.getChildWithName("SpacedRepetition");
    if (srTree.isValid())
      spacedRepetition.fromValueTree(srTree);

    auto plTree = state.getChildWithName("ProgressionLibrary");
    if (plTree.isValid())
      progressionLibrary.fromValueTree(plTree);

    auto mlTree = state.getChildWithName("MelodyLibrary");
    if (mlTree.isValid())
      melodyLibrary.fromValueTree(mlTree);

    auto extTree = state.getChildWithName("ExternalInstrument");
    if (extTree.isValid())
    {
      auto extXml = extTree.createXml();
      if (extXml != nullptr)
        externalInstrument.restoreFromStateXml(*extXml, getSampleRate(),
                                                getBlockSize());
    }

    apvts.replaceState(state);

    // Override with shared library file if it exists (syncs between DAW and Standalone)
    auto libFile = getLibrariesFile();
    if (libFile.existsAsFile())
      loadLibrariesFromDisk();
    else
      saveLibrariesToDisk();  // Seed the shared file from this instance's state
  }
}

//==============================================================================
juce::File AudioPluginAudioProcessor::getLibrariesFile()
{
    auto appDataDir = juce::File::getSpecialLocation (
        juce::File::userApplicationDataDirectory)
        .getChildFile ("Chordy");
    appDataDir.createDirectory();
    return appDataDir.getChildFile ("libraries.xml");
}

void AudioPluginAudioProcessor::saveLibrariesToDisk()
{
    juce::ValueTree root ("ChordyLibraries");
    root.appendChild (voicingLibrary.toValueTree(), nullptr);
    root.appendChild (progressionLibrary.toValueTree(), nullptr);
    root.appendChild (melodyLibrary.toValueTree(), nullptr);
    root.appendChild (spacedRepetition.toValueTree(), nullptr);

    auto xml = root.createXml();
    if (xml != nullptr)
        xml->writeTo (getLibrariesFile());
}

void AudioPluginAudioProcessor::loadLibrariesFromDisk()
{
    auto file = getLibrariesFile();
    if (! file.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("ChordyLibraries"))
        return;

    auto root = juce::ValueTree::fromXml (*xml);

    auto vlTree = root.getChildWithName ("VoicingLibrary");
    if (vlTree.isValid())
        voicingLibrary.fromValueTree (vlTree);

    auto plTree = root.getChildWithName ("ProgressionLibrary");
    if (plTree.isValid())
        progressionLibrary.fromValueTree (plTree);

    auto mlTree = root.getChildWithName ("MelodyLibrary");
    if (mlTree.isValid())
        melodyLibrary.fromValueTree (mlTree);

    auto srTree = root.getChildWithName ("SpacedRepetition");
    if (srTree.isValid())
        spacedRepetition.fromValueTree (srTree);
}

void AudioPluginAudioProcessor::startProgressionPlayback (const Progression& prog) {
  stopProgressionPlayback();
  playbackProgression = prog;

  // Apply quantization to rawMidi for playback if not Raw mode
  if (prog.quantizeResolution > 0.0)
    playbackProgression.rawMidi = ProgressionRecorder::quantizeMidi (
        prog.rawMidi, prog.quantizeResolution);

  playbackChordIndex = -1;
  playbackActiveNotes.clear();
  playbackSamplePos = 0.0;
  playbackCCIndex = 0;
  playbackBeat.store(0.0, std::memory_order_relaxed);

  // Sync to metronome: turn it on and reset beat position to 0
  if (auto* param = apvts.getParameter("metronomeOn"))
    param->setValueNotifyingHost(1.0f);
  tempoEngine.resetBeatPosition();

  playbackActive.store(true, std::memory_order_relaxed);
}

void AudioPluginAudioProcessor::stopProgressionPlayback() {
  if (playbackActive.load(std::memory_order_relaxed)) {
    playbackActive.store(false, std::memory_order_relaxed);
    int ch = static_cast<int>(*apvts.getRawParameterValue("midiChannel"));
    // Release pedal first, then notes
    addPreviewMidi(juce::MidiMessage::controllerEvent(ch, 64, 0));
    for (int n : playbackActiveNotes)
      addPreviewMidi(juce::MidiMessage::noteOff(ch, n));
    playbackActiveNotes.clear();
    playbackChordIndex = -1;
    playbackNotesLow.store(0, std::memory_order_relaxed);
    playbackNotesHigh.store(0, std::memory_order_relaxed);

    if (auto* param = apvts.getParameter("metronomeOn"))
      param->setValueNotifyingHost(0.0f);
  }
}

void AudioPluginAudioProcessor::startMelodyPlayback (const Melody& melody, int keyRootMidiNote) {
  stopMelodyPlayback();
  melodyPlaybackData = melody;

  // Apply quantization to rawMidi for playback if not Raw mode
  if (melody.quantizeResolution > 0.0)
    melodyPlaybackData.rawMidi = ProgressionRecorder::quantizeMidi (
        melody.rawMidi, melody.quantizeResolution);

  melodyPlaybackKeyRoot = keyRootMidiNote;
  melodyPlaybackNoteIndex = -1;
  melodyPlaybackActiveNote = -1;
  melodyPlaybackSamplePos = 0.0;
  melodyPlaybackCCIndex = 0;
  melodyPlaybackBeat.store(0.0, std::memory_order_relaxed);

  // Sync to metronome: turn it on and reset beat position to 0
  if (auto* param = apvts.getParameter("metronomeOn"))
    param->setValueNotifyingHost(1.0f);
  tempoEngine.resetBeatPosition();

  melodyPlaybackActive.store(true, std::memory_order_relaxed);
}

void AudioPluginAudioProcessor::stopMelodyPlayback() {
  if (melodyPlaybackActive.load(std::memory_order_relaxed)) {
    melodyPlaybackActive.store(false, std::memory_order_relaxed);
    int ch = static_cast<int>(*apvts.getRawParameterValue("midiChannel"));
    addPreviewMidi(juce::MidiMessage::controllerEvent(ch, 64, 0));
    if (melodyPlaybackActiveNote >= 0)
      addPreviewMidi(juce::MidiMessage::noteOff(ch, melodyPlaybackActiveNote));
    melodyPlaybackActiveNote = -1;
    melodyPlaybackNoteIndex = -1;
    playbackNotesLow.store(0, std::memory_order_relaxed);
    playbackNotesHigh.store(0, std::memory_order_relaxed);

    if (auto* param = apvts.getParameter("metronomeOn"))
      param->setValueNotifyingHost(0.0f);
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
