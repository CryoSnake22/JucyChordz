# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Chordy is a JUCE C++17 audio plugin for chord voicing, progression, and melody/lick practice with spaced repetition and tempo-based drilling. It receives MIDI input, displays a keyboard visualization, identifies chords, lets users record voicings, chord progressions, and melodies/licks into personal libraries, and practice them across all 12 keys with timed or untimed modes. Melodies support multi-chord context tagging (e.g., "ii-V-I lick: Dm7 > G7 > Cmaj7") with a backing pad that plays the chord changes underneath during practice. Includes a built-in piano synth for audible playback. Targets VST3, AU, and Standalone on macOS. Company: "JezzterInc".

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
| `PluginProcessor.h/.cpp` | Audio/MIDI processing hub. Owns `MidiKeyboardState`, APVTS, `VoicingLibrary`, `ProgressionLibrary`, `MelodyLibrary`, `ProgressionRecorder`, `SpacedRepetitionEngine`, `TempoEngine`, `ChordySynth`. Lock-free note sharing via atomic bitfield. Preview MIDI injection via SpinLock+MidiBuffer. Progression playback engine (sample-accurate chord injection). Melody playback engine (sample-accurate note-by-note injection). |
| `PluginEditor.h/.cpp` | Top-level GUI. Hosts keyboard, chord display, tabbed library panel (Voicings/Progressions/Melodies), practice panel. 60Hz timer drives chord detection, recording, practice, playback cursor, beat indicator, voicing preview, and live stats refresh. Tab-change detection clears selections. |
| `ChordyTheme.h` | Header-only constants namespace — all colors, font sizes, spacing, corner radii, chart colors. Every visual value lives here. |
| `ChordyLookAndFeel.h/.cpp` | Custom `LookAndFeel_V4` subclass. Flat rounded buttons, pill toggles, thin-track sliders, underline tabs. Avenir Next font. |
| `ChordySynth.h/.cpp` | Built-in piano synth. 2-operator FM synthesis (carrier + modulator at 1:1 ratio) with velocity-sensitive dynamics, pitch-dependent decay, and an octave partial for brightness. 12-voice polyphony. Renders additively in `processBlock()`. Enabled by default. |
| `TempoEngine.h/.cpp` | Audio-thread tempo engine. Internal BPM clock with optional DAW sync. Metronome click (sine burst). Beat position via atomics. Challenge timing API. |
| `BeatIndicatorComponent.h/.cpp` | Visual 4-dot beat indicator with BPM display and pulse animation. |
| `ChordDetector.h/.cpp` | Pure-logic chord identification. Pitch-class template matching (triads through 13ths). Bass note prioritized as root (+30 bonus). `getChordTones()` returns interval template for a quality (used for melody backing pad). |
| `ChordyKeyboardComponent.h/.cpp` | `MidiKeyboardComponent` subclass with colored key overlays (green=correct, red=wrong, teal=target). |
| `VoicingModel.h/.cpp` | `Voicing` struct + `VoicingLibrary` class with ValueTree serialization and `findByNotes()`. |
| `SpacedRepetition.h/.cpp` | SM-2 spaced repetition engine. Per-voicing/progression/melody per-key records. Generic item ID + keyIndex tracking. |
| `VoicingStatsChart.h/.cpp` | 12-bar chart (C-B) showing per-key accuracy. Used by voicing, progression, and melody panels. |
| `VoicingLibraryPanel.h/.cpp` | Voicing recording panel with 4-state flow (Idle->Waiting->Capturing->Confirming). |
| `ProgressionModel.h/.cpp` | `ProgressionChord` struct (intervals, root, quality, alterations, name, linkedVoicingId, startBeat, durationBeats, midiNotes) + `Progression` struct (id, name, key, mode, chords, totalBeats, bpm, timeSig, rawMidi) + `ProgressionLibrary` class. `transposeProgression()` shifts notes by semitones, regenerates chord names including slash notation, preserves voice leading. ValueTree serialization including raw MIDI as compact string. |
| `ProgressionRecorder.h/.cpp` | MIDI recording with beat-relative timestamps (sample counting + BPM). `analyzeChordChanges()` detects chord boundaries from MIDI. `quantize()` snaps to beat/half-beat/quarter-beat grid. `injectEvent()` for pre-held notes. Also reused by MelodyLibraryPanel for melody recording. |
| `ProgressionChartComponent.h/.cpp` | Lead-sheet chord chart renderer. Chords as rounded rects on a beat grid with bar lines, row wrapping (4 bars/row). Edit mode: click to select, drag edges to resize (snaps to quantize grid), draggable end marker (amber triangle). Cursor support for playback/practice. |
| `ProgressionLibraryPanel.h/.cpp` | Progression management panel with 6-state machine (Idle->CountIn->Recording->Editing->Confirming). 4-beat count-in synced to metronome. Quantize picker (Beat/1/2/1/4). Transpose +1/-1 buttons. Chord editing (name, root, quality). Delete chord button. Play/Stop playback. Stats bar chart. Click chord to hear+highlight. |
| `MelodyModel.h/.cpp` | `MelodyNote` struct (intervalFromKeyRoot, startBeat, durationBeats, velocity) + `MelodyChordContext` struct (intervalFromKeyRoot, quality, alterations, startBeat, durationBeats) + `Melody` struct (id, name, keyPitchClass, chordContexts, notes, totalBeats, bpm, timeSig, rawMidi) + `MelodyLibrary` class. `transposeMelody()` shifts keyPitchClass, regenerates names. `analyzeMelodyNotes()` converts raw MIDI to interval-based notes. `quantizeMelodyNotes()` always re-analyzes from original recording (non-destructive). ValueTree serialization. |
| `MelodyChartComponent.h/.cpp` | Note-name-on-beat-grid renderer. Notes as rounded rects with pitch on Y axis, beat on X axis. Chord context bar along bottom of each row. Row wrapping (8 beats/row). Note states: Default (grey), Target (amber outline), Correct (green), Missed (red). Edit mode: click notes, click/drag chord context edges. Cursor support for playback/practice. |
| `MelodyLibraryPanel.h/.cpp` | Melody management panel with 5-state machine (Idle->CountIn->Recording->Editing->Confirming). Reuses ProgressionRecorder for MIDI capture. Chord context editor in editing state (add/remove/edit chords on timeline with root+quality combos). Quantize always re-analyzes from original (non-destructive). Play/Stop via melody playback engine. |
| `PracticePanel.h/.cpp` | Practice GUI supporting voicing, progression, and melody practice. Voicing practice: timed (4-beat cycle) and untimed. Progression practice: chart with moving cursor, per-chord scoring, key transposition. Melody practice: sequential note-by-note pitch-class matching (timed and untimed), melody chart with note states, chord backing pad (toggleable). Custom key selection. Play button (hear answer), Next button (skip + record miss). Backing toggle plays chord context as sustained pad underneath during melody practice. Start button blocked when nothing is selected for current tab. |
| `PlaceholderPanel.h` | Header-only placeholder (no longer used for Melodies tab). |

### MIDI Data Flow

```
processBlock() [AUDIO THREAD]
  |-- keyboardState.processNextMidiBuffer() -> bridges to GUI
  |-- Scan isNoteOn() for 128 notes -> write to atomic bitfield
  |-- If notes active: update lastPlayedNotes (mutex-protected)
  |-- Forward MIDI to ProgressionRecorder if recording
  |-- Progression playback: inject chord note-on/off events by beat position
  |   |-- Update playbackNotesLow/High atomics for keyboard highlighting
  |-- Melody playback: inject note-on/off events by beat position
  |   |-- Update playbackNotesLow/High atomics for keyboard highlighting
  |-- Merge preview MIDI buffer (SpinLock-protected, from GUI thread)
  |-- If synthEnabled: ChordySynth.renderNextBlock() -> FM audio
  |-- tempoEngine.process() -> advance clock, render metronome click

timerCallback() [GUI THREAD, 60Hz]
  |-- Read atomic bitfield for live notes
  |-- Chord display (practice override or library match or ChordDetector)
  |-- Clear highlights on tab switch (tab-change detection)
  |-- Keyboard highlighting during progression/melody playback (from playbackNotes atomics)
  |-- Update voicing + progression + melody recording state machines
  |-- Progression panel timer (count-in beats, playback cursor, chord preview note-offs)
  |-- Melody panel timer (count-in beats, playback cursor, note preview note-offs)
  |-- Practice update (voicing, progression, or melody - timed or untimed)
  |-- Refresh stats charts live during practice
  |-- Voicing preview auto-off (frame counter)
  |-- Beat indicator update
```

### Built-in Synth (ChordySynth)

- **Architecture**: `juce::Synthesiser` with 12 `ChordyVoice` instances + 1 `ChordySound`
- **Sound**: 2-operator FM. Carrier at note frequency, modulator at 1:1 ratio (warm fundamental modulation). Octave partial (2x freq) for brightness.
- **Envelopes**: Percussive piano -- no sustain. Pitch-dependent decay (2.5s bass -> 1.0s treble). Mod envelope decays faster than amplitude (bright attack -> pure tone).
- **Velocity**: Square root curve at 50% level. Harder hits = more FM modulation sustain.
- **APVTS params**: `synthEnabled` (bool, default true), `synthVolume` (float 0-1, default 0.7)
- **Preview MIDI**: GUI thread injects via `addPreviewMidi()` -> SpinLock buffer -> merged in processBlock. Used for voicing click preview, chord click preview, Play button in practice, and melody backing pad.
- **Voicing preview**: Click voicing in library -> teal keyboard highlights + synth plays for ~0.5s. Bypasses keyboardState (no flash). Notes auto-release via frame counter.

### Progression Playback Engine

- **Start**: `startProgressionPlayback(prog)` copies progression, resets beat counter, turns on metronome, resets beat position to sync with beat 1
- **processBlock**: advances `playbackSamplePos`, computes beat position, finds current chord, injects note-on/off at correct sample offsets. Updates `playbackNotesLow/High` atomics for GUI keyboard highlighting.
- **Stop**: `stopProgressionPlayback()` sends note-offs via preview MIDI, clears atomics, turns off metronome.
- **GUI**: chart cursor follows beat position (60Hz updates), keyboard shows current chord notes in teal.

### Melody Playback Engine

- **Start**: `startMelodyPlayback(melody, keyRootMidiNote)` copies melody data, resets sample counter.
- **processBlock**: advances `melodyPlaybackSamplePos`, computes beat position, finds which note should be active, injects note-on/off at sample-accurate positions. Updates `playbackNotesLow/High` atomics for keyboard highlighting.
- **Stop**: `stopMelodyPlayback()` sends note-off for active note, clears atomics.
- **GUI**: chart cursor follows beat position, keyboard shows current note in teal.

### Melody Recording Flow

1. **Idle** -> Click Record
2. **CountIn** -> 4-beat count-in synced to metronome (reuses ProgressionRecorder)
3. **Recording** -> ProgressionRecorder captures MIDI with beat timestamps. Stop button ends recording.
4. **Editing** -> Raw MIDI analyzed into MelodyNote intervals via `analyzeMelodyNotes()`. Quantize to beat/half/quarter (always re-analyzes from original raw MIDI -- non-destructive). Chart displayed with:
   - Chord context bar: add/remove/edit chords on timeline
   - Each chord has root (ComboBox), quality (ComboBox), alterations (TextEditor)
   - Drag chord context edges to resize (snaps to grid)
   - Transpose +1/-1 semitones
   - Default: one chord spanning full duration
5. **Confirming** -> Name, Key form. Key change adjusts all note intervals and chord context intervals. Save updates existing melody (same ID) or creates new.

### Melody Practice

**Untimed mode:**
- Chart displayed with notes, current target has amber outline
- Target label: "Key: Eb - Play: G"
- Sequential note-by-note pitch-class matching (any octave accepted)
- Correct notes turn green on chart, advance to next note
- Wrong notes: text feedback only, don't advance
- After all notes -> quality scored from correct/total ratio -> advances to next key
- Next button skips note (records miss), Play button plays whole melody via synth
- Backing pad: synth plays current chord context as sustained notes (toggleable)
- Keyboard coloring: continuous refresh every frame via `lastCorrectPC` tracking. Held correct notes stay green, stale colors clear on release.

**Timed mode:**
- 4-beat count-in, then cursor follows metronome through the melody chart
- Each note scored by timing: < 0.5 beats = Perfect (Q5), 0.5-1.0 = Good (Q4), 1.0-1.5 = OK (Q3), 1.5-2.0 = Slow (Q2), > 2.0 = Late (Q1)
- Missed notes (cursor passes without correct pitch) = Q0
- Notes that cursor passes without input auto-marked as missed (red on chart)
- Overall quality = average of per-note qualities
- Shows "Complete! X/Y notes" with score on key completion

**Transposition:**
- All notes stored as `intervalFromKeyRoot`, so transposing by N semitones just shifts `keyPitchClass`. Chord context intervals also stay the same -- chord roots shift uniformly preserving relationships (e.g., ii-V-I stays ii-V-I in any key).

### Progression Recording Flow

1. **Idle** -> Click Record
2. **CountIn** -> Metronome starts, beat position resets. Shows "4... 3... 2... 1..." synced to metronome beats.
3. **Recording** -> `ProgressionRecorder.processBlock()` captures MIDI with beat timestamps. Pre-held notes injected at beat 0. Stop button ends recording.
4. **Editing** -> Raw MIDI analyzed into chord changes. Quantize to beat/half/quarter. Chart displayed with:
   - Click chord to select + hear + highlight keyboard
   - Edit name, root, quality of selected chord
   - Delete chord (red button)
   - Drag chord edges to resize (snaps to grid)
   - Drag end marker to set progression duration
   - Transpose +1/-1 semitones (regenerates names, plays first chord preview)
   - Play button for full playback with cursor
   - Quantize changes re-snap existing edited chords (doesn't re-analyze from raw MIDI)
5. **Confirming** -> Name, Key, Mode form. Pre-fills from existing values when editing. Save updates existing progression (same ID) or creates new.

### Progression Practice

**Untimed mode:**
- Chart displayed with highlighted current chord
- Target label: "Key: Eb - Fm7"
- User plays correct pitch classes -> "Correct!" -> advances to next chord on note release
- After all chords -> quality scored from correct/total ratio -> advances to next key
- Next button skips chord (records miss), Play button just plays the sound

**Timed mode:**
- 4-beat count-in, then cursor follows metronome through the chart
- Each chord scored by timing: < 0.5 beats = Perfect (Q5), 0.5-1.0 = Good (Q4), 1.0-1.5 = OK (Q3), 1.5-2.0 = Slow (Q2), > 2.0 = Late (Q1)
- Missed chords (cursor passes without correct notes) = Q0
- Overall quality = average of per-chord qualities
- Shows "Complete! 3/4 chords" with score on key completion

**Transposition:**
- `ProgressionLibrary::transposeProgression()` shifts all chord roots, regenerates display names (including slash bass notes), shifts MIDI notes by semitone offset. Voice leading preserved exactly.

### Practice Modes (Voicing)

**Untimed mode:**
- Show root in grey above keyboard -> turns green on correct -> advances on note release

**Timed mode (4-beat musical cycle):**
```
Count-in:  [1] [2] [3 root shows] [4]
Cycle N:   [1 PLAY!] [2 play] [3 Up next: root] [4 prep]
```

**Custom practice:** 12 key toggles + All/None + Random/Chromatic order.

### Data Model

**MelodyNote struct:**
- `intervalFromKeyRoot` (int), `startBeat`, `durationBeats` (double), `velocity` (int)

**MelodyChordContext struct:**
- `intervalFromKeyRoot` (int, e.g. ii=2, V=7, I=0), `quality` (ChordQuality), `alterations`, `startBeat`, `durationBeats`, `name`

**Melody struct:**
- `id` (UUID), `name`, `keyPitchClass` (0-11), `chordContexts` (vector), `notes` (vector), `totalBeats`, `bpm`, `timeSignatureNum/Den`, `rawMidi` (MidiMessageSequence)

**ProgressionChord struct:**
- `intervals` (vector<int>), `rootPitchClass` (0-11), `quality` (ChordQuality), `alterations`, `name`
- `linkedVoicingId` (optional ref to VoicingLibrary), `startBeat`, `durationBeats`, `midiNotes`
- `getDisplayName()`: generates from root+quality+alterations, includes slash bass if lowest note != root

**Progression struct:**
- `id` (UUID), `name`, `keyPitchClass`, `mode` (Major/Minor/Dorian/etc.), `chords`, `totalBeats`, `bpm`, `timeSignatureNum/Den`, `rawMidi` (MidiMessageSequence)

**Voicing struct:** `id`, `name`, `quality`, `alterations`, `rootPitchClass`, `intervals`, `octaveReference`

**Serialization:** VoicingLibrary, ProgressionLibrary, MelodyLibrary, and SpacedRepetition state all serialized as ValueTree children of APVTS state. Raw MIDI stored as compact "time:byte1:byte2:byte3;..." string.

### Tab System

- Tab 0: **Voicings** -> `VoicingLibraryPanel` (fully functional)
- Tab 1: **Progressions** -> `ProgressionLibraryPanel` (fully functional)
- Tab 2: **Melodies** -> `MelodyLibraryPanel` (fully functional)
- Tab switching clears keyboard highlights, stops previews, updates practice panel selection

### GUI Layout (1100x740)

```
+-----------------------------------------------------+
| CHORDY                                              | 40px header
+-----------------------------------------------------+
|              Db                                     | 52px big root display
|         Up next...  E                               | 28px next root preview
+-----------------------------------------------------+
| === ChordyKeyboardComponent (keyWidth=28) ========= | 140px keyboard (C2-C7)
+-----------------------------------------------------+
| BPM:[===120===] [xClick][xSync][xSynth][Vol] ****  | 36px tempo bar
+----------------------+------------------------------+
| [Voicings|Prog|Mel]  | Practice: Voicing/Prog/Melody |
| List (voicings or    | [Chart if progression/melody] |
|   progs or melodies) | [Start|Next|Play|Custom]      |
| ==== Stats chart ====| [x Timed] [x Backing]         |
| [Chart preview]      | [Key selector if custom]      |
| [Record][Play]       | Feedback + timing + accuracy   |
| [Edit][Delete]       |                                |
+----------------------+------------------------------+
```

## Adding Source Files

All new `.cpp`/`.h` files **must** be added to the `SOURCE_FILES` variable in `CMakeLists.txt`.

## Plugin Configuration (CMakeLists.txt)

- MIDI Input: enabled | MIDI Output: enabled | IS_SYNTH: false
- Formats: VST3, AU, Standalone
- Manufacturer code: `Tap1` | Plugin code: `Chrd`
- `EDITOR_WANTS_KEYBOARD_FOCUS: TRUE`

## APVTS Parameters

| ID | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `practiceMode` | Bool | -- | false | Practice mode flag |
| `midiChannel` | Int | 1-16 | 1 | MIDI channel (1-indexed) |
| `bpm` | Float | 30-300 | 120 | Internal metronome tempo |
| `metronomeOn` | Bool | -- | false | Enable/disable click |
| `useHostSync` | Bool | -- | false | Sync to DAW transport |
| `timedPractice` | Bool | -- | false | Enable timed scoring |
| `responseWindowBeats` | Float | 1-8 | 4 | Beats before timeout |
| `synthEnabled` | Bool | -- | true | Built-in piano synth on/off |
| `synthVolume` | Float | 0-1 | 0.7 | Synth output level |

## Theming System

All visual styling centralized -- **never hardcode hex colors or font sizes**.

- **`ChordyTheme.h`**: All colors, font sizes, spacing, corner radii, chart-specific colors, melody-specific colors.
- **`ChordyLookAndFeel`**: Custom LookAndFeel_V4 for all standard JUCE widgets.
- **Font**: Avenir Next via `setDefaultSansSerifTypefaceName()`. Requires `setDefaultLookAndFeel()`.
- **Palette**: Warm charcoal + amber accent. Green=correct, red=danger/missed, teal=target/preview, amber=accent/cursor.
- **Keyboard overlay colors**: `keyCorrect` (green, semi-transparent), `keyWrong` (red, semi-transparent), `keyTarget` (light teal). Browsing/preview/playback highlights use teal (`KeyColour::Target`). Green (`KeyColour::Correct`) is reserved for correct answers during practice only.
- **Melody chart note colors**: Default=neutral grey, Target=amber outline, Correct=green fill, Missed=red fill.
- **Avoid UTF-8 special characters** in displayed strings (em-dashes, arrows). JUCE's font rendering may not handle them. Use plain ASCII alternatives (-, >, etc.).

## Key Development Notes

- Audio thread rules: no allocations, no blocking, no locks in `processBlock()` -- exceptions: `lastPlayedNotes` mutex (only when notes active), `previewMidiLock` SpinLock (brief).
- Preview MIDI path (`addPreviewMidi`) bypasses `keyboardState` entirely -- no keyboard flash, no interference with practice note detection. Used for voicing preview, chord preview, Play button, progression playback note-offs, melody backing pad.
- Progression playback uses its own sample counter + BPM for beat position. Updates `playbackNotesLow/High` atomics for GUI keyboard highlighting (same pattern as `activeNotesLow/High`).
- Melody playback uses its own sample counter + BPM, same pattern as progression playback but for individual notes instead of chords.
- `ProgressionRecorder.recording` is `std::atomic<bool>` for audio thread safety. Reused for melody recording (no separate MelodyRecorder).
- Tab-change detection in editor timer clears stale selections and updates practice panel type.
- Practice panel's `setSelectedVoicingId` / `setSelectedProgressionId` / `setSelectedMelodyId` each clear the other IDs and set `practiceType` to prevent stale state.
- Melody quantize always re-analyzes from raw MIDI (non-destructive). Progression quantize re-snaps existing chords (preserves name/root/quality edits).
- Melody note intervals stored as `intervalFromKeyRoot` -- transposition shifts `keyPitchClass`, intervals stay the same. Chord context intervals also relative to key root.
- Melody practice keyboard coloring: must refresh every frame (like voicing practice's `updateKeyboardColours`). Track `lastCorrectPC` so held correct notes stay green after target advances. Without continuous refresh, JUCE's default key-down color (orange/yellow) shows through.
- Slash chord detection: if lowest MIDI note's pitch class != root pitch class, append "/BassNote" to display name. Works in both `getDisplayName()` and `transposeProgression()`.
- Start button is blocked when nothing is selected for the current practice type.
- No test infrastructure exists yet
- Planned future features: MIDI output routing to external VST instruments (replaces internal synth), MIDI file import, comping rhythm templates
