# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Chordy is a JUCE C++17 audio plugin for chord voicing practice with spaced repetition and tempo-based drilling. It receives MIDI input, displays a keyboard visualization, identifies chords, and lets users record voicings into a personal library and practice them across all 12 keys with timed or untimed modes. Targets VST3, AU, and Standalone on macOS. Company: "JezzterInc".

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
| `PluginProcessor.h/.cpp` | Audio/MIDI processing hub. Owns `MidiKeyboardState`, APVTS, `VoicingLibrary`, `SpacedRepetitionEngine`, `TempoEngine`. Lock-free note sharing via atomic bitfield + mutex-protected `lastPlayedNotes`. |
| `PluginEditor.h/.cpp` | Top-level GUI. Hosts keyboard, chord display (big root + "Up next..." preview), tabbed library panel, practice panel. 60Hz timer drives chord detection, recording, practice, beat indicator, and live stats refresh. |
| `TempoEngine.h/.cpp` | Audio-thread tempo engine. Internal BPM clock with optional DAW sync via `AudioPlayHead`. Generates sample-accurate metronome click (sine burst). Exposes beat position via atomics for lock-free GUI reads. Challenge timing API for measuring response time. |
| `BeatIndicatorComponent.h/.cpp` | Visual 4-dot beat indicator with BPM display and pulse animation on the active beat. |
| `ChordDetector.h/.cpp` | Pure-logic chord identification. Pitch-class template matching against known chord types (triads through 13ths, altered dominants, 6/9, add9). Bass note heavily prioritized as root (+30 score bonus). |
| `ChordyKeyboardComponent.h/.cpp` | `MidiKeyboardComponent` subclass with colored key overlays (green=correct, red=wrong, bright blue=target). Overrides `drawWhiteNote`/`drawBlackNote`. |
| `VoicingModel.h/.cpp` | `Voicing` struct (intervals from root, quality, alterations, rootPitchClass) + `VoicingLibrary` class with ValueTree serialization and `findByNotes()` for matching played notes against the library. |
| `SpacedRepetition.h/.cpp` | SM-2 spaced repetition engine with quality-based scoring (0-5). Tracks per-voicing per-key practice records (successes, failures, easeFactor, lastResponseQuality). `getStatsForVoicing()` returns per-key accuracy for the bar chart. |
| `VoicingStatsChart.h/.cpp` | Bar chart component showing 12 vertical bars (C through B) with accuracy for a selected voicing. Green/yellow/red color coding. Refreshes live during practice. |
| `VoicingLibraryPanel.h/.cpp` | GUI panel with 4-state recording flow (Idle→Waiting→Capturing→Confirming). Includes voicing list, quality filter, stats chart, and confirmation form. |
| `PracticePanel.h/.cpp` | Practice GUI with timed (4-beat musical cycle) and untimed modes. Custom practice with key selection (12 toggles + All/None) and root order (Chromatic/Random). Start/Stop toggles metronome. Displays root names with color feedback. |
| `PlaceholderPanel.h` | Header-only placeholder component for future Progressions and Melodies tabs. |

### MIDI Data Flow

```
processBlock() [AUDIO THREAD]
  └─ keyboardState.processNextMidiBuffer() → bridges to GUI
  └─ Scan isNoteOn() for 128 notes → write to std::atomic<uint64_t>[2] bitfield
  └─ If notes active: update lastPlayedNotes (mutex-protected, persists after release)
  └─ tempoEngine.process() → advance clock, render metronome click

timerCallback() [GUI THREAD, 60Hz]
  └─ Read atomic bitfield for live notes, fallback to lastPlayedNotes for display
  └─ During practice: override chord display with big root + "Up next..." preview
  └─ Normal mode: check voicingLibrary.findByNotes() → else ChordDetector::detect()
  └─ Feed activeNotes to voicingLibraryPanel.updateRecording()
  └─ If practicing: updatePractice() + refreshStatsChart() (live bar chart updates)
  └─ Update beat indicator with tempo engine state
```

### Tempo Engine

- **Internal clock**: BPM parameter in APVTS (30-300, default 120). Sample-accurate beat tracking via sample counting.
- **DAW sync**: Optional sync to host via `AudioPlayHead::PositionInfo` (getBpm, getPpqPosition).
- **Metronome click**: Decaying sine burst — downbeat 1000Hz/30ms, upbeats 800Hz/20ms. Rendered additively in `processBlock()`.
- **Beat state sharing**: Atomics (`beatNumber`, `beatPhase`, `beatTick`, `currentBeatPosition`) for lock-free GUI reads.
- **Challenge timing**: `markChallengeStart()` / `getBeatsSinceChallengeStart()` for measuring response time in beats.
- **Reset**: `resetBeatPosition()` zeros the clock for count-in sync when practice starts.

### Practice Modes

**Untimed mode:**
- Show root in grey above keyboard → turns green on correct → advances on note release
- Uses SR engine's `getNextChallenge()` (most overdue key) or custom key sequence

**Timed mode (4-beat musical cycle):**
```
Count-in:  [1] [2] [3 root shows] [4]
Cycle N:   [1 PLAY!] [2 play] [3 Up next: root] [4 prep]
```
- Count-in = 4 beats, first chord root appears on beats 3-4
- Play phase (beats 1-2): root shown in bright green, user must play correct pitch classes
- Prep phase (beats 3-4): show next root dimmed, pre-load next challenge
- Quality scoring within 2-beat play window: < 0.5 beats = Q5 (perfect), 0.5-1.0 = Q4, 1.0-1.5 = Q3, 1.5-2.0 = Q2
- Playing early during prep = rewarded as "Early - Perfect!" (Q5)
- Partial chord building (subset of target notes) = fine, only extra wrong notes penalize
- Timeout at prep transition = Q0

**Custom practice:**
- 12 key toggle buttons (C through B) with All/None shortcuts
- Root order: Random (shuffled each cycle) or Chromatic (C, Db, D, ...)
- Custom mode drives its own key sequence instead of SR engine's overdue scoring
- SR engine still records attempts for stats tracking

### Data Model

**Voicing struct** fields:
- `id` (UUID string), `name` (user-defined), `quality` (ChordQuality enum), `alterations` (free text like "#9#11b5")
- `rootPitchClass` (0-11, user-confirmed), `intervals` (semitones from root, always starts with 0)
- `octaveReference` (MIDI note of root when recorded)
- `getQualityLabel()` returns quality suffix + alterations for display

**PracticeRecord** fields:
- `voicingId`, `keyIndex` (0-11), `successes`, `failures`, `lastAttemptTime`, `intervalDays`, `easeFactor`, `lastResponseQuality` (0-5 SM-2 quality)

**KeyStats** struct: per-key accuracy view — `accuracy()` returns successes/(successes+failures), -1 for no data.

**ChordQuality enum** covers: Major, Minor, Dim, Aug, Maj6, Min6, Dom7, Maj7, Min7, MinMaj7, Dim7, HalfDim7, Dom7b5, Dom7#5, Dom7b9, Dom7#9, Dom9, Maj9, Min9, MinMaj9, Dom11, Min11, Maj7#11, Dom13, Maj13, Min13, Maj69, Min69, Add9, MinAdd9, Sus2, Sus4, Unknown

**Serialization:** All structured data (voicing library + SR state) serialized as ValueTree children of APVTS state in `getStateInformation()`/`setStateInformation()`. APVTS parameters (bpm, metronomeOn, useHostSync, timedPractice, responseWindowBeats, practiceMode, midiChannel) persist automatically.

### Recording Flow (VoicingLibraryPanel state machine)

1. **Idle** → Click Record
2. **Waiting** → Red indicator "Play a chord...", waiting for any note
3. **Capturing** → Notes **accumulate** (union of all notes pressed during session). Only finishes when ALL notes released. This allows building voicings one note at a time.
4. **Confirming** → Normal list UI replaced by confirmation form: Name, Root (dropdown C-B), Quality (dropdown with N/A option), Alterations (free text). Save commits to library, Cancel discards.

### Chord Display Priority

When NOT practicing, the system checks in order:
1. **User voicing library** — `findByNotes()` matches exact interval pattern → shows `"Eb m7#9#11 (So What voicing)"`
2. **Built-in ChordDetector** — template matching with heavy bass-note-as-root preference (+30 bonus)

When practicing, the chord display shows:
- **Big root name** (48pt, centered) — grey when waiting, bright green when playing, green flash on correct
- **"Up next..." line** (16pt, below) — shows next root during prep phase (timed mode)

### GUI Layout (1000x660)

```
┌─────────────────────────────────────────────────────┐
│ CHORDY                                              │ 40px header
├─────────────────────────────────────────────────────┤
│              Db                                     │ 52px big root display
│         Up next...  E                               │ 28px next root preview
├─────────────────────────────────────────────────────┤
│ ╔═══ ChordyKeyboardComponent (keyWidth=28) ═══════╗ │ 140px keyboard (C2-C7)
├─────────────────────────────────────────────────────┤
│ BPM: [===120===] [x Click] [x Sync] ● ● ● ●  120 │ 36px tempo bar
├──────────────────────┬──────────────────────────────┤
│ [Voicings|Prog|Mel]  │ PRACTICE / Practicing: X     │
│ Voicing list         │ [Start|Next|Play|Custom]      │
│ ████ Stats chart ████│ [x Timed]                     │
│ [Record] [Delete]    │ [C][Db]...[All][None][Order]  │
│                      │ Feedback + timing + accuracy   │
└──────────────────────┴──────────────────────────────┘
```

Clicking a voicing in the library (when not practicing) highlights its original notes on the keyboard in bright blue. Highlight clears when user plays any notes.

## Adding Source Files

All new `.cpp`/`.h` files **must** be added to the `SOURCE_FILES` variable in `CMakeLists.txt`. Header-only files (like `PlaceholderPanel.h`) don't need to be listed.

## Plugin Configuration (CMakeLists.txt)

- MIDI Input: enabled | MIDI Output: enabled | IS_SYNTH: false
- Formats: VST3, AU, Standalone
- Manufacturer code: `Tap1` | Plugin code: `Chrd`
- `EDITOR_WANTS_KEYBOARD_FOCUS: TRUE` (for on-screen keyboard input)

## APVTS Parameters

| ID | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `practiceMode` | Bool | — | false | Practice mode flag |
| `midiChannel` | Int | 1-16 | 1 | MIDI channel (1-indexed) |
| `bpm` | Float | 30-300 | 120 | Internal metronome tempo |
| `metronomeOn` | Bool | — | false | Enable/disable click (auto-toggled by practice Start/Stop) |
| `useHostSync` | Bool | — | false | Sync to DAW transport |
| `timedPractice` | Bool | — | false | Enable timed scoring |
| `responseWindowBeats` | Float | 1-8 | 4 | Beats before timeout |

## Key Development Notes

- Audio thread rules: no allocations, no blocking, no locks in `processBlock()` — exception: `lastPlayedNotes` uses a `std::mutex` but only writes when notes are active (not on every callback). `TempoEngine::process()` is fully lock-free (sine synthesis + atomic writes).
- `MidiKeyboardState` bridges audio↔GUI threads safely. Uses 1-indexed MIDI channels (1-16).
- Active notes shared via `std::atomic<uint64_t>` bitfield (lock-free reads from GUI thread)
- `lastPlayedNotes` persists the most recent chord after key release — used by Record and chord display so they work even after notes are lifted
- `ChordyKeyboardComponent` inherits from `juce::MidiKeyboardComponent` (in `juce_audio_utils`). The `drawWhiteNote`/`drawBlackNote` signatures come from `KeyboardComponentBase` in the same module.
- JUCE `MidiKeyboardComponent` default midiChannel = 1. Our APVTS `midiChannel` param also defaults to 1. Both are 1-indexed.
- JUCE `Optional` uses `hasValue()` not `has_value()` (custom wrapper, not std::optional).
- The bottom-left panel uses `juce::TabbedComponent` with three tabs: Voicings (functional), Progressions (placeholder), Melodies (placeholder).
- Practice Start/Stop auto-controls the metronome. `resetBeatPosition()` syncs count-in with beat 1.
- Partial chord building (pressing notes one at a time) is NOT counted as wrong — only notes outside the target set trigger `hasWrongAttempt`.
- Playing the correct chord early during the prep phase is rewarded as quality 5 ("Early - Perfect!").
- Custom practice drives its own key sequence (chromatic or random shuffle) instead of SR engine's overdue scoring. The sequence cycles and reshuffles each round.
- No test infrastructure exists yet
- Planned future features: chord progression practice, melody/lick practice (time-series MIDI), MIDI file import, comping rhythm templates
