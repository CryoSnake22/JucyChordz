# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Chordy is a JUCE C++17 audio plugin for chord voicing, progression, and melody/lick practice with spaced repetition and tempo-based drilling. It receives MIDI input, displays a keyboard visualization, identifies chords, lets users record voicings, chord progressions, and melodies/licks into personal libraries, and practice them across all 12 keys with timed or untimed modes. Melodies support multi-chord context tagging (e.g., "ii-V-I lick: Dm7 > G7 > Cmaj7") with a backing pad that plays the chord changes underneath during practice. Includes a built-in piano synth and external VST3/AU instrument hosting for audible playback. Targets VST3, AU, and Standalone on macOS. Company: "JezzterInc".

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
| `PluginProcessor.h/.cpp` | Audio/MIDI processing hub. Owns `MidiKeyboardState`, APVTS, `VoicingLibrary`, `ProgressionLibrary`, `MelodyLibrary`, `ExternalInstrument`, `ProgressionRecorder`, `SpacedRepetitionEngine`, `TempoEngine`, `ChordySynth`. Lock-free note sharing via atomic bitfield + per-note velocity tracking (`noteVelocities[128]`). Preview MIDI injection via SpinLock+MidiBuffer. Progression and melody playback engines replay raw MIDI directly (sample-accurate, preserves velocity + pedal + all CC events). Instrument routing: internal synth OR external hosted plugin, controlled by `synthEnabled` parameter. |
| `PluginEditor.h/.cpp` | Top-level GUI. Hosts keyboard, chord display (center: always shows detected chord; left overlay: practice key + countdown), tabbed library panel (Voicings/Progressions/Melodies), practice panel. 60Hz timer drives chord detection, recording, practice, playback cursor, beat indicator, voicing preview, live stats refresh, clicked-chord display, and library button enable/disable. Collapsible tempo bar via "Settings"/"Hide Settings" toggle. Tab-change detection clears selections AND stops practice. External instrument controls (plugin selector, Scan, Open) visible when settings expanded + External mode selected. |
| `ExternalInstrument.h/.cpp` | VST3/AU plugin hosting for Standalone mode. Uses `AudioPluginFormatManager` (VST3 + AU formats), `KnownPluginList`, `PluginDirectoryScanner`. Background scanning with dead-mans-pedal crash protection. Async plugin loading via `createPluginInstanceAsync` (JUCE-recommended pattern). Bus layout negotiation (stereo preferred, falls back gracefully). Channel-safe `processBlock` with scratch buffer for mismatched layouts. CriticalSection for plugin swap (ScopedTryLock on audio thread). Loading dead-mans-pedal (`loadingPlugin.txt`) protects against SIGSEGV during plugin init. Plugin list cached to `~/Library/Application Support/Chordy/pluginList.xml`. Instruments deduplicated by name (VST3 preferred over AU). Plugin state persisted in session via XML + base64 blob (async restore). Hosted plugin's editor opens in a separate `DocumentWindow`. |
| `ChordyTheme.h` | Header-only constants namespace -- all colors, font sizes, spacing, corner radii, chart colors. Every visual value lives here. |
| `ChordyLookAndFeel.h/.cpp` | Custom `LookAndFeel_V4` subclass. Flat rounded buttons, pill toggles, thin-track sliders, underline tabs. Avenir Next font. |
| `ChordySynth.h/.cpp` | Built-in piano synth. 2-operator FM synthesis (carrier + modulator at 1:1 ratio) with velocity-sensitive dynamics, pitch-dependent decay, and an octave partial for brightness. 12-voice polyphony. Renders additively in `processBlock()`. Enabled by default. |
| `TempoEngine.h/.cpp` | Audio-thread tempo engine. Internal BPM clock with optional DAW sync. Metronome click (sine burst). Beat position via atomics. Challenge timing API. |
| `BeatIndicatorComponent.h/.cpp` | Visual 4-dot beat indicator with BPM display and pulse animation. |
| `ChordDetector.h/.cpp` | Pure-logic chord identification. Pitch-class template matching (triads through 13ths). Bass note prioritized as root (+30 bonus). `getChordTones()` returns interval template for a quality (used for melody backing pad). |
| `ChordyKeyboardComponent.h/.cpp` | `MidiKeyboardComponent` subclass with colored key overlays (green=correct, red=wrong, teal=target). |
| `FolderModel.h/.cpp` | `Folder` struct (id, name, sortOrder) + `FolderLibrary` class with ValueTree serialization. Each library (Voicing/Progression/Melody) owns its own FolderLibrary. Folders serialize as `<Folders>` child inside each library's XML section. |
| `VoicingModel.h/.cpp` | `Voicing` struct (id, name, quality, alterations, rootPitchClass, intervals, velocities, octaveReference, folderId, createdAt) + `VoicingLibrary` class with ValueTree serialization, `findByNotes()`, and `FolderLibrary`. `createFromNotes()` accepts optional velocity vector and auto-sets `createdAt`. |
| `SpacedRepetition.h/.cpp` | SM-2 spaced repetition engine with **recency-weighted accuracy**: `attemptHistory` vector (capped at 100) tracks per-attempt quality scores. Most recent 10 attempts get 2x weight. `KeyStats::accuracy()` uses recency weighting with legacy fallback. Proportional quality mapping via `proportionToQuality()`. |
| `VoicingStatsChart.h/.cpp` | Interactive 12-bar chart (C-B) showing per-key accuracy. Click a bar to play the selected item in that key (`onKeyClicked` callback). Hover highlighting, playing-key amber outline (`setPlayingKey`). Used by voicing, progression, and melody panels. |
| `VoicingLibraryPanel.h/.cpp` | Voicing recording panel with 4-state flow (Idle->Waiting->Capturing->Confirming). Play/Edit/Record/Delete buttons. `enterConfirmingWithVoicing()` for external voicing creation (used by Save Voicing from progression). `selectVoicingById()` for programmatic selection. Captures per-note velocities during recording. **Multi-select** via shift/cmd-click. **Folder dropdown** above search bar with "All Items"/"Unfiled"/named folders. **"..." menu** with Import/Export MIDI, Import/Export Library (.chordy), New/Rename/Delete Folder. **Right-click context menu** with Move to Folder (inline "New Folder..." option), Export as MIDI, Delete. `onKeyPreview`, `onRecordStarted`, `onEditStarted` callbacks. Auto-selects newly saved voicings. Edit removes old voicing before adding updated one. |
| `ProgressionModel.h/.cpp` | `ProgressionChord` struct with **per-note timing**: `midiNotes`, `midiVelocities`, `noteStartBeats`, `noteDurations` (all parallel vectors). Chord-level `startBeat`/`durationBeats` derived from per-note data (min start, max end). `quantizeResolution` stored per progression. `Progression` struct (id, name, key, mode, chords, totalBeats, bpm, timeSig, rawMidi, quantizeResolution, folderId, createdAt). `transposeProgression()` shifts notes AND rawMidi. Auto-upgrades old saved data without per-note timing by re-analyzing from rawMidi on load. `ProgressionLibrary` owns a `FolderLibrary`. |
| `ProgressionRecorder.h/.cpp` | MIDI recording with beat-relative timestamps. Two-phase chord analysis: **`extractNotes()`** walks raw MIDI matching note-on/off pairs into individual `ProgressionNote` structs (like melody), then **`groupNotesIntoChords()`** clusters notes by note-on proximity (0.5 beat gap threshold). `analyzeChordChanges()` = extractNotes + groupNotesIntoChords. `quantize()` snaps per-note timing to grid, recomputes chord-level timing. `quantizeMidi()` snaps raw MIDI event timestamps for playback. `injectEvent()` for pre-held notes (uses actual velocity from processor). Also reused by MelodyLibraryPanel for melody recording. |
| `ProgressionChartComponent.h/.cpp` | Dual-mode chart: **Detailed (default)** = piano-roll view with individual MIDI notes rendered at per-note timing positions (no note names drawn on notes). **Simple** = chord names as rects. Toggle via `setDetailedView()`. Detailed view works in both edit and preview mode. Edit mode has draggable **end marker** (yellow cursor, snaps to whole beats, priority over chord edges). `setLastClickedBeat()` drives per-note amber highlighting during practice playback. Chord labels hidden when width < 40px. **Dynamic `beatsPerRow`**: progressions <= 16 beats use `ceil(totalBeats)` (min 4) so short progressions fill the width; longer ones wrap at 16. Bar lines, cursor, `getNoteState()`/`setNoteState()` for per-note feedback. Edit chart wrapped in `juce::Viewport` for scrolling. |
| `ProgressionLibraryPanel.h/.cpp` | Progression management panel with 6-state machine (Idle->CountIn->Recording->Editing->Confirming). 4-beat count-in synced to metronome. Quantize picker (Raw/Beat/1/2/1/4). Raw re-analyzes from original MIDI with no grid snapping. Transpose +1/-1 buttons. Chord editing (name, root, quality). Delete chord button. Play/Stop playback. Stats bar chart with click-to-play-in-key. **Multi-select**, **folder dropdown**, **"..." menu** (Import/Export MIDI + .chordy, folder management), **right-click context menu** (Move to Folder, Export MIDI, Delete). Selection change stops playback. `onTransposedPreview` callback. Chart preview removed from this panel (lives in practice panel now). |
| `MelodyModel.h/.cpp` | `MelodyNote` struct (intervalFromKeyRoot, startBeat, durationBeats, velocity) + `MelodyChordContext` struct (intervalFromKeyRoot, quality, alterations, startBeat, durationBeats) + `Melody` struct (id, name, keyPitchClass, chordContexts, notes, totalBeats, bpm, timeSig, rawMidi, folderId, createdAt) + `MelodyLibrary` class with `FolderLibrary`. `transposeMelody()` shifts keyPitchClass AND rawMidi note events, regenerates names. `analyzeMelodyNotes()` converts raw MIDI to interval-based notes. `quantizeMelodyNotes()` always re-analyzes from original recording (non-destructive). ValueTree serialization. |
| `MelodyChartComponent.h/.cpp` | Note-on-beat-grid renderer with dynamic viewport height. Notes as rounded rects with pitch on Y, beat on X. Chord context bar along bottom. Row wrapping (8 beats/row). Note states: Default/Target/Correct/Missed. Edit mode: click notes, drag chord context edges, **draggable end marker** (yellow, snaps to whole beats). Edit chart wrapped in `juce::Viewport` for scrolling. Cursor support for playback/practice. |
| `MelodyLibraryPanel.h/.cpp` | Melody management panel with 5-state machine (Idle->CountIn->Recording->Editing->Confirming). Reuses ProgressionRecorder for MIDI capture. Chord context editor in editing state (add/remove/edit chords on timeline with root+quality combos). Quantize picker (Raw/Beat/1/2/1/4) -- Raw keeps exact recorded timing. Quantize always re-analyzes from original (non-destructive). Play/Stop via melody playback engine. **Multi-select**, **folder dropdown**, **"..." menu** (Import/Export MIDI + .chordy, folder management), **right-click context menu** (Move to Folder, Export MIDI, Delete). Selection change stops playback. `onTransposedPreview` callback. Chart preview removed from idle layout (lives in practice panel). |
| `MidiFileUtils.h/.cpp` | MIDI file import/export utilities. `importMidiFile()` reads .mid files, converts ticks to beat-relative timestamps, extracts BPM/timeSig from meta events. `midiToVoicing()` extracts first simultaneous chord. `midiToProgression()` uses ProgressionRecorder analysis pipeline. `midiToMelody()` uses MelodyLibrary analysis. Export functions write standard MIDI files with 480 TPQ, tempo meta-events, beat-to-tick conversion. |
| `LibraryExporter.h/.cpp` | `.chordy` XML collection format for sharing libraries. `exportCollection()` writes selected or all items (per-type via `includeVoicings/Progressions/Melodies` flags) with folder structure. `parseCollection()` reads `.chordy` files. `mergeIntoLibraries()` imports with UUID-based duplicate skipping and folder preservation. |
| `PracticePanel.h/.cpp` | Practice GUI supporting voicing, progression, and melody practice. **Chart always visible at top**. Both charts in `juce::Viewport`. Voicing practice: timed (4-beat cycle) and untimed. **4 order modes** for voicing practice: Chromatic (up-and-back through all 12 keys), Random, Follow (root walks along a chosen scale or melody -- pure transposition), Scale (diatonic mode -- each note moves independently by scale steps, only available when voicing fits the scale). Follow/Scale modes disabled for progression/melody practice. **Progression practice: per-note independent scoring** — each note scored individually by whether user plays its pitch class during its `[startBeat, startBeat+duration)` window. Chords are display-only labels, not scoring units. Notes turn Target (amber) when active, Correct (green) when played, Missed (red) when expired. **Melody practice**: per-note pitch-class matching with 0.5-beat coyote time for early hits (scored flag prevents double-scoring repeated notes). Proportional scoring on skip/timeout. **Save Voicing / See Voicing button**: shares row with target label (doesn't shrink chart). **Chromatic practice default**: all modes use chromatic up-and-back key sequence by default. Practice start button disabled during edit mode. **Compact layout**: feedback/stats/timing labels only take space when they have text content; Custom key selector expands into freed space instead of squeezing upward. |
| `ScaleModel.h/.cpp` | Scale definitions and utilities. `ScaleType` enum (Major, NaturalMinor, Dorian, Mixolydian, Lydian, Phrygian, Locrian, HarmonicMinor, MelodicMinor, MajorPentatonic, MinorPentatonic, Blues, WholeTone, DiminishedHW, DiminishedWH, Chromatic). `getScaleIntervals()` returns pitch class intervals. `voicingFitsInScale()` checks if all voicing notes exist in a scale. `buildScaleRootSequence()` generates up-and-back pitch class sequence for Follow mode. `buildMelodyFollowSequence()` extracts pitch classes from melody notes. `diatonicTranspose()` moves each voicing note independently by scale degrees using a "scale ladder" approach. |
| `PlaceholderPanel.h` | Header-only placeholder (no longer used for Melodies tab). |

### MIDI Data Flow

```
processBlock() [AUDIO THREAD]
  |-- Capture per-note velocities from MIDI input -> noteVelocities[128] atomics
  |-- keyboardState.processNextMidiBuffer() -> bridges to GUI
  |-- Scan isNoteOn() for 128 notes -> write to atomic bitfield
  |-- If notes active: update lastPlayedNotes (mutex-protected)
  |-- Forward MIDI to ProgressionRecorder if recording (notes + pedal CC64)
  |-- Progression playback: replay raw MIDI events directly at beat positions
  |   |-- All note-on/off, velocity, CC events from original recording
  |   |-- Track active notes for keyboard highlighting
  |-- Melody playback: replay raw MIDI events directly at beat positions
  |   |-- Same raw MIDI replay approach as progression
  |-- Merge preview MIDI buffer (SpinLock-protected, from GUI thread)
  |-- If synthEnabled: ChordySynth.renderNextBlock() -> FM audio
  |-- Else: ExternalInstrument.processBlock() -> hosted plugin audio (or silence in DAW mode)
  |-- Volume slider applied to both internal synth and hosted plugin output
  |-- tempoEngine.process() -> advance clock, render metronome click
```

### Playback Architecture

Both progression and melody playback engines use **raw MIDI replay** -- they iterate through the recorded `rawMidi` `MidiMessageSequence` and inject every event (note-on, note-off, CC, pedal) at the correct beat position. This preserves the exact performance including velocity dynamics, sustain pedal timing, and all controller data. No chord/note reconstruction from analyzed data.

On stop, both engines send pedal-off (CC64=0) before note-offs to prevent stuck sustain.

### External Instrument Hosting

- **Compile definitions**: `JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1` in CMakeLists.txt
- **Format registration**: Manual (`new VST3PluginFormat()`, `new AudioUnitPluginFormat()`) because `addDefaultFormats()` is deleted in JUCE headless builds
- **Scanning**: Background thread via `PluginDirectoryScanner`. Results cached to `~/Library/Application Support/Chordy/pluginList.xml`. Dead-mans-pedal file (`deadMansPedal.txt`) blacklists crashy plugins.
- **Loading**: Uses `createPluginInstanceAsync` (JUCE-recommended pattern from HostPluginDemo). Callback fires on message thread. Generation counter prevents stale callbacks from racing. try/catch around `prepareToPlay`.
- **Bus layout negotiation**: `setupPluginBuses()` tries stereo in/out, then stereo-out only (instruments), then accepts plugin default. `setRateAndBufferSizeDetails()` called before `prepareToPlay()`.
- **Channel-safe processBlock**: If hosted plugin's channel count differs from host buffer, a pre-allocated scratch buffer handles the mismatch (copy in, process, copy out). Fast path (zero overhead) when channels match.
- **Thread safety**: `CriticalSection` (not SpinLock) protects hosted plugin pointer. Audio thread uses `ScopedTryLock` (non-blocking). All other paths use `ScopedLock`.
- **Deduplication**: Plugins deduplicated by name in UI, preferring VST3 over AU.
- **Plugin editor**: Opens in a separate `DocumentWindow` (always-on-top, closeable, resizable) via "Open" button in tempo bar. Editor window closed automatically when loading a new plugin (prevents dangling pointer).
- **State persistence**: Plugin description XML + plugin state as base64 blob, saved as ValueTree child of APVTS state. Restore is async via `createPluginInstanceAsync` with try/catch around `setStateInformation` + `prepareToPlay`.
- **Loading dead-mans-pedal**: Before loading any plugin, its name is written to `~/Library/Application Support/Chordy/loadingPlugin.txt`. Cleared on success or caught failure. If a SIGSEGV crashes the host, the file survives -- on next startup, `restoreFromStateXml` detects it and skips the crashy plugin. User can retry via the UI dropdown.
- **Files cached at**: `~/Library/Application Support/Chordy/` -- `pluginList.xml` (scan cache), `deadMansPedal.txt` (scan crashes), `loadingPlugin.txt` (load crashes), `libraries.xml` (shared voicing/progression/melody/SR data for DAW-Standalone sync).

### GUI Layout (1100x740)

```
+-----------------------------------------------------+
| CHORDY                                              | 40px header
+-----------------------------------------------------+
| Key: Eb    [chord detection always centered]        | 52px chord display
| [countdown]  [Up next: G]                           | 28px sub-labels (overlaid left)
+-----------------------------------------------------+
| === ChordyKeyboardComponent (keyWidth=28) ========= | 140px keyboard (C2-C7)
+-----------------------------------------------------+
| [Settings/Hide] BPM:[slider] [Click][Sync] ...     | 36px tempo bar (collapsible)
+----------------------+------------------------------+
| [Voicings|Prog|Mel]  | [Chart preview / practice]   |
| VOICING LIBRARY [...] | (always visible, fills space) |
| [All Items       v]  | [target label] [Save Voicing]|
| [Search...]          | [Start|Next|Play|Custom]      |
| [All            v]   | [x Timed] [x Detailed/Backing]|
| List (voicings or    | [Key selector if custom]      |
|   progs or melodies) | Feedback + timing + accuracy   |
| ==== Stats chart ====|                               |
| [Record][Play]       |                               |
| [Edit][Delete]       |                               |
+----------------------+------------------------------+
```

Chord display area:
- **Center**: Always shows chord detection (from keyboard or playback or clicked preview). Never blocked by practice.
- **Left overlay**: "Key: X" during practice (36pt amber). Countdown below it (18pt tertiary, disappears after count-in).
- **Next root**: "Up next: G" below chord display during practice.

Tempo bar controls (collapsible via Settings toggle):
- **Settings toggle**: "Hide Settings" when expanded (95px), "Settings" when collapsed (70px). Collapsed hides all controls except beat indicator.
- **Instrument mode combo**: "Internal" (default) / "External". Visible in both DAW and Standalone.
- **Plugin selector**: Visible when settings expanded + External mode. Populated from scanned VST3/AU instruments.
- **Scan button**: Triggers background plugin scan. Visible when settings expanded + External mode.
- **Open button**: Opens hosted plugin's editor window. Only visible when a plugin is loaded.
- **Volume slider**: Controls output volume for both internal synth and hosted plugin (3x gain boost applied to external instruments).
- **Metronome volume slider**: Separate volume for metronome click.

### Recording Features

All recording (voicings, progressions, melodies) captures:
- **Velocity**: Per-note velocity stored and preserved during playback/preview
- **Sustain pedal**: CC64 events recorded and replayed during playback
- **Raw mode**: All editors offer Raw/Beat/1/2/1/4 quantize options. Raw keeps exact recorded timing. All quantize levels re-analyze from original raw MIDI (non-destructive -- switching quantize levels never destroys detail).

Voicing-specific:
- Per-note velocities stored in `Voicing.velocities` (parallel to `intervals`)
- Velocities captured from processor's `noteVelocities[128]` atomic array during recording
- Preview playback uses recorded velocities

Progression-specific:
- **Per-note timing**: Each note in a chord has individual `noteStartBeats` and `noteDurations` (parallel to `midiNotes`)
- Per-note velocities stored in `ProgressionChord.midiVelocities` (parallel to `midiNotes`)
- Two-phase analysis: `extractNotes()` (individual notes from MIDI) → `groupNotesIntoChords()` (cluster by 0.5-beat gap)
- Playback replays raw MIDI directly. Quantized playback uses `quantizeMidi()` to snap event timestamps
- Pre-held notes injected at beat 0 with actual velocity from `noteVelocities[]` (not hardcoded 100)

### Data Model

**Folder struct:** `id` (UUID), `name`, `sortOrder`

**Voicing struct:** `id`, `name`, `quality`, `alterations`, `rootPitchClass`, `intervals`, `velocities`, `octaveReference`, `folderId`, `createdAt`

**ProgressionChord struct:**
- `intervals` (vector<int>), `rootPitchClass` (0-11), `quality` (ChordQuality), `alterations`, `name`
- `linkedVoicingId` (optional ref to VoicingLibrary), `startBeat`, `durationBeats`, `midiNotes`, `midiVelocities`
- **Per-note timing**: `noteStartBeats` (vector<double>), `noteDurations` (vector<double>) — parallel to midiNotes, each note has individual start/end
- `getDisplayName()`: generates from root+quality+alterations, includes slash bass if lowest note != root

**ProgressionNote struct** (in ProgressionRecorder.h):
- `midiNote`, `velocity`, `startBeat`, `durationBeats` — intermediate struct used by `extractNotes()` pipeline

**Progression struct:**
- `id` (UUID), `name`, `keyPitchClass`, `mode` (Major/Minor/Dorian/etc.), `chords`, `totalBeats`, `bpm`, `timeSignatureNum/Den`, `rawMidi` (MidiMessageSequence), `quantizeResolution` (0=raw, 1.0=beat, 0.5=half, 0.25=quarter), `folderId`, `createdAt`

**MelodyNote struct:**
- `intervalFromKeyRoot` (int), `startBeat`, `durationBeats` (double), `velocity` (int)

**MelodyChordContext struct:**
- `intervalFromKeyRoot` (int, e.g. ii=2, V=7, I=0), `quality` (ChordQuality), `alterations`, `startBeat`, `durationBeats`, `name`

**Melody struct:**
- `id` (UUID), `name`, `keyPitchClass` (0-11), `chordContexts` (vector), `notes` (vector), `totalBeats`, `bpm`, `timeSignatureNum/Den`, `rawMidi` (MidiMessageSequence), `folderId`, `createdAt`

**Serialization:** VoicingLibrary, ProgressionLibrary, MelodyLibrary, SpacedRepetition state, and ExternalInstrument state all serialized as ValueTree/XML children of APVTS state. Raw MIDI stored as compact "time:byte1:byte2:byte3;..." string. Velocities stored as comma-separated int strings. Each library's ValueTree includes a `<Folders>` child for folder data. `folderId` and `createdAt` attributes on each item (backward compatible -- empty/0 defaults for old data). **Shared library file**: Libraries also saved to `~/Library/Chordy/libraries.xml` for syncing between DAW and Standalone instances. Loaded on construction, saved on every library modification, practice stop, and plugin destruction. **`.chordy` collection format**: `<ChordyCollection>` root with version, name attributes; contains `<VoicingLibrary>`, `<ProgressionLibrary>`, `<MelodyLibrary>` sections using identical ValueTree serialization. Used for import/export sharing between users.

## Adding Source Files

All new `.cpp`/`.h` files **must** be added to the `SOURCE_FILES` variable in `CMakeLists.txt`.

## Plugin Configuration (CMakeLists.txt)

- MIDI Input: enabled | MIDI Output: enabled | IS_SYNTH: false
- Formats: VST3, AU, Standalone
- Manufacturer code: `Tap1` | Plugin code: `Chrd`
- `EDITOR_WANTS_KEYBOARD_FOCUS: TRUE`
- `JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1` (enables hosting external plugins)

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
| `synthEnabled` | Bool | -- | true | true=internal synth, false=external instrument |
| `synthVolume` | Float | 0-1 | 0.7 | Output volume (applies to both internal and external) |
| `metronomeVolume` | Float | 0-1 | 0.7 | Metronome click volume |

## Theming System

All visual styling centralized -- **never hardcode hex colors or font sizes**.

- **`ChordyTheme.h`**: All colors, font sizes, spacing, corner radii, chart-specific colors, melody-specific colors.
- **`ChordyLookAndFeel`**: Custom LookAndFeel_V4 for all standard JUCE widgets.
- **Font**: Avenir Next via `setDefaultSansSerifTypefaceName()`. Requires `setDefaultLookAndFeel()`.
- **Palette**: Warm charcoal + amber accent. Green=correct, red=danger/missed, teal=target/preview, amber=accent/cursor.
- **Keyboard overlay colors**: `keyCorrect` (green, semi-transparent), `keyWrong` (red, semi-transparent), `keyTarget` (light teal). Browsing/preview/playback highlights use teal (`KeyColour::Target`). Green (`KeyColour::Correct`) is reserved for correct answers during practice only.
- **Chart note colors** (both progression and melody): Default=neutral grey (`melodyNoteBg`), Target=amber outline (`accent`), Correct=green fill (`melodyNoteCorrect`), Missed=red fill (`melodyNoteMissed`). Notes have 2px rounding. Dynamic height = noteAreaHeight / pitchRange, capped at 16px. No note names drawn on notes (removed for cleaner preview). Chord labels hidden when width < 40px.
- **Chart backgrounds**: Note area uses `bgSurface` (blends with card). Chord label bar uses `melodyChordBg` (flat, no rounding). Bar lines at 0.4f alpha. No rounded rect on note area background.
- **UI consistency standards**: List item names at 15pt, secondary text (dates, badges) at 12pt. Row height 36px. Stats chart 60px. Delete buttons use `dangerMuted`. Practice panel has square corners. Theme font sizes: `fontSectionHead=15`, `fontBody=14`, `fontSmall=12`, `fontMeta=11`.
- **Avoid UTF-8 special characters** in displayed strings (em-dashes, arrows). JUCE's font rendering may not handle them. Use plain ASCII alternatives (-, >, etc.).

## Key Development Notes

- Audio thread rules: no allocations, no blocking, no locks in `processBlock()` -- exceptions: `lastPlayedNotes` mutex (only when notes active), `previewMidiLock` SpinLock (brief), `pluginLock` CriticalSection (ScopedTryLock, non-blocking for hosted plugin).
- Preview MIDI path (`addPreviewMidi`) bypasses `keyboardState` entirely -- no keyboard flash, no interference with practice note detection. Used for voicing preview, chord preview, Play button, playback note-offs, melody backing pad.
- Both playback engines (progression + melody) replay raw MIDI directly from `rawMidi` MidiMessageSequence. This preserves velocity, pedal, all CC events exactly as recorded. No chord/note reconstruction during playback.
- `ProgressionRecorder.recording` is `std::atomic<bool>` for audio thread safety. Reused for melody recording (no separate MelodyRecorder). Records note-on/off AND CC64 (sustain pedal).
- Chord analysis uses a **two-phase pipeline**: `extractNotes()` creates individual `ProgressionNote` objects from raw MIDI (matching note-on/off pairs), then `groupNotesIntoChords()` clusters notes by note-on proximity (0.5 beat gap threshold). This replaces the old single-pass chord boundary detection which had accumulation bugs.
- Per-note velocities tracked via `noteVelocities[128]` atomic array in processor, updated from MIDI input each processBlock. **Two-pass capture**: first pass before `processNextMidiBuffer` catches external MIDI; second pass after catches on-screen/QWERTY keyboard events injected by `processNextMidiBuffer`. Without the second pass, on-screen keyboard voicing recordings get zero velocities. Used by VoicingLibraryPanel to capture velocities during voicing recording.
- Tab-change detection in editor timer clears stale selections and updates practice panel type.
- Practice panel's `setSelectedVoicingId` / `setSelectedProgressionId` / `setSelectedMelodyId` each clear the other IDs and set `practiceType` to prevent stale state.
- Both melody and progression quantize always re-analyze from raw MIDI (non-destructive). Switching between Raw/Beat/1/2/1/4 never destroys the original recording.
- Melody note intervals stored as `intervalFromKeyRoot` -- transposition shifts `keyPitchClass`, intervals stay the same. Chord context intervals also relative to key root.
- Melody practice keyboard coloring: must refresh every frame (like voicing practice's `updateKeyboardColours`). Track `lastCorrectPC` so held correct notes stay green after target advances. Without continuous refresh, JUCE's default key-down color (orange/yellow) shows through.
- Start button is blocked when nothing is selected for current practice type. Shows "Press Start" when item is selected (not the long "Select a voicing..." message).
- **Tab switching stops practice**: `practicePanel.stopPractice()` called on tab change. Also stops playback.
- **Library fully locked during practice**: `setButtonsEnabled(false)` disables Record/Play/Edit/Delete buttons, ListBox, search, folder combo, "..." button, and stats chart mouse interaction. Driven by 60Hz timer.
- **Record clears state**: Pressing Record fires `onRecordStarted` callback which clears keyboard highlighting and stops all playback.
- **Keyboard highlighting is persistent**: Clicking a chord/voicing/note highlights keys and they stay highlighted even when user plays other notes. Cleared only on: tab switch, selection change, practice start, or record start.
- **Clicked chord display**: `PracticePanel.clickedChordName` with frame-based auto-clear shows chord/note names in the center display when clicking items in preview. Timer-ticked from editor.
- **Transposed preview**: Stats chart key-click fires `onTransposedPreview` callback to update the practice panel chart to the transposed version, so clicking chords in the transposed preview plays correct notes.
- **TempoEngine reset is deferred**: `resetBeatPosition()` sets a `pendingReset` atomic flag; actual reset happens at start of next `process()` to avoid cutting off mid-render metronome clicks. Also resets `challengeStartBeatPosition` to prevent negative beat counts.
- **Melody backing pad uses root-position stacked thirds**: Chord tones from `getChordTones()` placed with root in C3-F#3 range, intervals stacking upward. No octave scrambling.
- External instrument hosting: `addDefaultFormats()` is deleted in JUCE headless builds -- must register formats manually via `new VST3PluginFormat()` / `new AudioUnitPluginFormat()`.
- **Plugin hosting must use `createPluginInstanceAsync`** (not `Thread::launch` + synchronous `createPluginInstance`). Complex plugins like Keyscape need message thread access during initialization. The old background-thread approach caused crashes. Follow the JUCE `HostPluginDemo` pattern: async load, callback on message thread, CriticalSection (not SpinLock), bus layout negotiation, `setRateAndBufferSizeDetails` before `prepareToPlay`.
- **Bus layout negotiation is essential** before calling a hosted plugin's `processBlock`. The AudioBuffer channel count must exactly match what the plugin expects. Use `setupPluginBuses()` after loading and a scratch buffer in `processBlock` for mismatches.
- Loading dead-mans-pedal (`loadingPlugin.txt`) protects against signal-level crashes (SIGSEGV) that try/catch cannot catch. Written before load, cleared on success. Checked on restore to skip crashy plugins.
- **Playback should always use raw MIDI replay by default.** Only reconstruct from analyzed data when transposition requires it (e.g., practice mode transposing to different keys). Raw replay preserves velocity, pedal, timing nuance exactly.
- **When a UI behavior works in one mode but not another**, diff the two working code paths side-by-side before writing a fix. The answer is always in what the working version does differently.
- **External instrument hosting works in both DAW and Standalone** (no longer standalone-only). 3x gain boost applied to external instrument output to match internal synth levels.
- **`SpacedRepetitionEngine::getNextChallenge` accepts `avoidKey` parameter** to prevent repeating the same root note consecutively. Custom mode shuffle also avoids boundary repeats.
- **Progression practice is per-note, not per-chord**: Each individual note is independently scored based on whether the user plays its pitch class during its `[noteStartBeat, noteStartBeat+noteDuration)` window. Chords are display-only labels. No chord-level "scored" flag — chart noteStates track everything.
- **Melody practice coyote time**: 0.5-beat anticipation window checks the next note if the current one is already scored. `scored` flag prevents double-scoring repeated pitch classes. Timing quality scales with note duration (`forgiveness = max(0.5, noteDuration)`).
- **Proportional scoring on skip/timeout**: Missing half the notes = ~50% quality (not flat failure). `computeProportionalMatch()` computes correct/total minus 0.1 per extra note. `proportionToQuality()` maps to SM-2 quality 0-5.
- **Recency-weighted accuracy**: `attemptHistory` vector in `PracticeRecord` (capped at 100 entries). Most recent 10 get 2x weight. Backward compatible with legacy successes/failures.
- **Spacebar shortcuts**: `keyPressed` on editor handles space — stops practice if active, toggles play/stop on progression/melody tabs, plays voicing on voicing tab.
- **Save/See Voicing from progression**: Clicking a chord in progression preview shows "Save Voicing" or "See Voicing" button (checks `findByNotes()`). Save creates voicing and enters VoicingLibraryPanel confirm mode. See switches to voicing tab and selects the match.
- **Chromatic practice default**: All practice modes use chromatic up-and-back key sequence (C, C#, ..., B, Bb, ..., C#) by default. `orderCombo` defaults to Chromatic (ID 2). Additional voicing-only order modes: Follow (ID 3, root walks scale degrees or melody notes -- pure transposition) and Scale (ID 4, diatonic mode -- each note moves independently by scale steps via `ScaleModel::diatonicTranspose`). Follow/Scale disabled for progression/melody practice.
- **Follow mode**: Two sub-sources: Scale (root follows scale degrees up-and-back) and Melody (root follows pitch class sequence from a melody in the library). Uses standard `transposeToKey` -- only the key sequence changes.
- **Scale (diatonic) mode**: `voicingFitsInScale()` checks compatibility -- incompatible scales greyed out in picker. `diatonicTranspose()` builds a "scale ladder" (all MIDI notes in the scale across octaves), finds each voicing note's position, shifts by N degrees. E.g., Cmaj triad in C Major at degree 1 = D, F, A (Dm).
- **Exercise preview**: Play button next to scale/melody picker in Follow/Scale modes. `playExercisePreview()` schedules timed MIDI playback through one scale cycle at current BPM. `buildExercisePreviewProgression()` builds a `Progression` from the exercise and displays it via `showProgressionPreview()`. Spacebar plays/stops exercise when custom panel is open in Follow/Scale mode. Stats chart click plays exercise in the clicked key.
- **Voicing hover timeout**: Keyboard highlights auto-clear after ~4 seconds (240 frames at 60Hz) when browsing voicings in the library. Uses frame countdown in editor timer (`keyboardHighlightFramesRemaining`). Does not affect practice mode highlights.
- **Recording defaults to Raw mode**: `currentQuantizeResolution = 0.0` on recording stop. `totalBeats` rounds up to nearest 4-beat bar to preserve trailing silence.
- **End marker (yellow cursor)**: Draggable in both progression and melody edit charts. Snaps to whole beats. Priority over chord edge hit testing. Freely movable (not locked to last note/chord). Chart grows via `setSize` to accommodate new rows during drag. Epsilon offset (`-0.01`) prevents disappearing at exact row boundaries.
- **Edit mode shows detailed view**: Removed `!editMode` gate from progression chart rendering. Edit charts wrapped in `juce::Viewport` for scrolling. Chart width reduced by scrollbar thickness to prevent overlap. Practice panel preview cleared during edit mode via `onEditStarted` callback, restored on `enterIdle`.
- **Auto-select on save**: All library panels select and scroll to newly saved items after confirm.
- **Selection change stops playback**: `selectedRowsChanged` in progression and melody panels calls `stopProgressionPlayback()`/`stopMelodyPlayback()` to prevent orphaned playback when clicking a different item.
- **Multi-select behavior**: `getSelectedVoicingId()`/`getSelectedProgressionId()`/`getSelectedMelodyId()` return empty when `getNumSelectedRows() != 1`. Play/Edit buttons disabled when multi-select active. Batch delete and batch move-to-folder work on all selected items. Practice panel gets empty ID on multi-select.
- **Folder system**: Each library owns a `FolderLibrary`. Items have `folderId` (empty = root/unfiled). Folder combo in each panel filters `updateDisplayed*()`. "..." menu provides New/Rename/Delete Folder. Right-click "Move to Folder" submenu includes "New Folder..." for inline creation. `refreshFolderCombo()` called from constructor and `refresh()` to populate on load. Folder deletion moves contained items to root (not deleted).
- **MIDI import/export**: `MidiFileUtils::importMidiFile()` reads .mid, converts ticks to beats via detected BPM. For voicings: extracts first simultaneous chord. For progressions: runs `ProgressionRecorder::analyzeChordChanges()`. For melodies: runs `MelodyLibrary::analyzeMelodyNotes()`. Export writes standard MIDI Type 1 with 480 TPQ. All `FileChooser::launchAsync` calls wrapped in `juce::MessageManager::callAsync` to avoid conflicts with `PopupMenu`'s modal state.
- **Library export/import (.chordy)**: XML format using identical ValueTree serialization. `ExportOptions` has `includeVoicings/Progressions/Melodies` flags for per-type export. Import merges by UUID (duplicates skipped). Folders preserved in export/import.
- **Voicing edit removes-then-adds**: `onConfirmSave()` calls `removeVoicing(savedId)` before `addVoicing(pendingVoicing)` so edits replace the original. `populateConfirmFields()` preserves name/alterations when editing (vs auto-detecting for new recordings).
- **Voicing preview velocity fallback**: `startVoicingPreview` falls back to 0.8f velocity if stored velocity is 0 (prevents note-off interpretation from zero-velocity note-on).
- **Creation timestamps**: `createdAt` (juce::int64 milliseconds) set on `createFromNotes()`, recording stop, and MIDI import. Displayed as "Mar 28, 2026" on second line of list items. Serialized as XML attribute (backward compatible -- 0 default for old items).
- No test infrastructure exists yet
- Planned future features: LilyPond PDF export, comping rhythm templates
