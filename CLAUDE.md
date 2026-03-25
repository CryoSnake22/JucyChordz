# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Chordy is a JUCE C++17 audio plugin for chord voicing practice with spaced repetition. It receives MIDI input, displays a keyboard visualization, identifies chords, and lets users record voicings into a personal library and practice them across all 12 keys. Targets VST3, AU, and Standalone on macOS. Company: "JezzterInc".

## Build Commands

```bash
make build    # cmake -B cmake-build && cmake --build cmake-build
make run      # build + open Standalone app
```

Build-specific format:
```bash
cmake --build cmake-build --target Chordy_VST3
cmake --build cmake-build --target Chordy_AU
cmake --build cmake-build --target Chordy_Standalone
```

Artifacts: `cmake-build/Chordy_artefacts/Standalone/Chordy.app` (and VST3/AU equivalents).

`COPY_PLUGIN_AFTER_BUILD=TRUE` auto-installs built plugins to system directories.

## JUCE Dependency

JUCE is included as a **git submodule** at `./JUCE/` via `add_subdirectory(JUCE)`.

## Architecture

### Source Files (all in `Source/`)

| File | Purpose |
|---|---|
| `PluginProcessor.h/.cpp` | Audio/MIDI processing hub. Owns `MidiKeyboardState`, APVTS, `VoicingLibrary`, `SpacedRepetitionEngine`. Lock-free note sharing via atomic bitfield + mutex-protected `lastPlayedNotes`. |
| `PluginEditor.h/.cpp` | Top-level GUI. Hosts keyboard, chord label, voicing library panel, practice panel. 60Hz timer drives chord detection, recording state, and practice updates. |
| `ChordDetector.h/.cpp` | Pure-logic chord identification. Pitch-class template matching against known chord types (triads through 13ths, altered dominants, 6/9, add9). Bass note heavily prioritized as root (+30 score bonus). |
| `ChordyKeyboardComponent.h/.cpp` | `MidiKeyboardComponent` subclass with colored key overlays (green=correct, red=wrong, blue=target). Overrides `drawWhiteNote`/`drawBlackNote`. |
| `VoicingModel.h/.cpp` | `Voicing` struct (intervals from root, quality, alterations, rootPitchClass) + `VoicingLibrary` class with ValueTree serialization and `findByNotes()` for matching played notes against the library. |
| `SpacedRepetition.h/.cpp` | SM-2 spaced repetition engine. Tracks per-voicing per-key practice records. Prioritizes overdue and failed keys. |
| `VoicingLibraryPanel.h/.cpp` | GUI panel with 4-state recording flow (Idle→Waiting→Capturing→Confirming). Confirmation screen lets user set name, root, quality, and alterations before saving. |
| `PracticePanel.h/.cpp` | GUI panel for spaced repetition practice with visual feedback. Compares by pitch class. Auto-advances on success. |

### MIDI Data Flow

```
processBlock() [AUDIO THREAD]
  └─ keyboardState.processNextMidiBuffer() → bridges to GUI
  └─ Scan isNoteOn() for 128 notes → write to std::atomic<uint64_t>[2] bitfield
  └─ If notes active: update lastPlayedNotes (mutex-protected, persists after release)

timerCallback() [GUI THREAD, 60Hz]
  └─ Read atomic bitfield for live notes, fallback to lastPlayedNotes for display
  └─ Check voicingLibrary.findByNotes() for custom name match → else ChordDetector::detect()
  └─ Feed activeNotes to voicingLibraryPanel.updateRecording() (accumulates during capture)
  └─ If practicing: compare notes → color keyboard → update SR state
```

### Data Model

**Voicing struct** fields:
- `id` (UUID string), `name` (user-defined), `quality` (ChordQuality enum), `alterations` (free text like "#9#11b5")
- `rootPitchClass` (0-11, user-confirmed), `intervals` (semitones from root, always starts with 0)
- `octaveReference` (MIDI note of root when recorded)
- `getQualityLabel()` returns quality suffix + alterations for display

**ChordQuality enum** covers: Major, Minor, Dim, Aug, Maj6, Min6, Dom7, Maj7, Min7, MinMaj7, Dim7, HalfDim7, Dom7b5, Dom7#5, Dom7b9, Dom7#9, Dom9, Maj9, Min9, MinMaj9, Dom11, Min11, Maj7#11, Dom13, Maj13, Min13, Maj69, Min69, Add9, MinAdd9, Sus2, Sus4, Unknown

**Serialization:** All structured data (voicing library + SR state) serialized as ValueTree children of APVTS state in `getStateInformation()`/`setStateInformation()`.

### Recording Flow (VoicingLibraryPanel state machine)

1. **Idle** → Click Record
2. **Waiting** → Red indicator "Play a chord...", waiting for any note
3. **Capturing** → Notes **accumulate** (union of all notes pressed during session). Only finishes when ALL notes released. This allows building voicings one note at a time.
4. **Confirming** → Normal list UI replaced by confirmation form: Name, Root (dropdown C-B), Quality (dropdown with N/A option), Alterations (free text). Save commits to library, Cancel discards.

### Chord Display Priority

When displaying the detected chord name, the system checks in order:
1. **User voicing library** — `findByNotes()` matches exact interval pattern → shows `"Eb m7#9#11 (So What voicing)"`
2. **Built-in ChordDetector** — template matching with heavy bass-note-as-root preference (+30 bonus)

### GUI Layout (1000x660)

```
┌─────────────────────────────────────────────────────┐
│ CHORDY                                              │ 40px header
├─────────────────────────────────────────────────────┤
│         Detected: Cmaj7                             │ 60px chord display
├─────────────────────────────────────────────────────┤
│ ╔═══ ChordyKeyboardComponent (keyWidth=28) ═══════╗ │ 140px keyboard (C2-C7)
├────────────────────────┬────────────────────────────┤
│ VOICING LIBRARY        │ PRACTICE                   │
│ (or confirm form)      │ Target + feedback + stats   │ remaining space
└────────────────────────┴────────────────────────────┘
```

Clicking a voicing in the library (when not practicing) highlights its original notes on the keyboard in blue.

## Adding Source Files

All new `.cpp`/`.h` files **must** be added to the `SOURCE_FILES` variable in `CMakeLists.txt`.

## Plugin Configuration (CMakeLists.txt)

- MIDI Input: enabled | MIDI Output: enabled | IS_SYNTH: false
- Formats: VST3, AU, Standalone
- Manufacturer code: `Tap1` | Plugin code: `Chrd`
- `EDITOR_WANTS_KEYBOARD_FOCUS: TRUE` (for on-screen keyboard input)

## Key Development Notes

- Audio thread rules: no allocations, no blocking, no locks in `processBlock()` — exception: `lastPlayedNotes` uses a `std::mutex` but only writes when notes are active (not on every callback)
- `MidiKeyboardState` bridges audio↔GUI threads safely. Uses 1-indexed MIDI channels (1-16).
- Active notes shared via `std::atomic<uint64_t>` bitfield (lock-free reads from GUI thread)
- `lastPlayedNotes` persists the most recent chord after key release — used by Record and chord display so they work even after notes are lifted
- `ChordyKeyboardComponent` inherits from `juce::MidiKeyboardComponent` (in `juce_audio_utils`). The `drawWhiteNote`/`drawBlackNote` signatures come from `KeyboardComponentBase` in the same module.
- JUCE `MidiKeyboardComponent` default midiChannel = 1. Our APVTS `midiChannel` param also defaults to 1. Both are 1-indexed.
- No test infrastructure exists yet
- Planned future features: lick practice, chord progression practice, voicing selection for progressions
