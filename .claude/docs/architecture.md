# Architecture

## Source Files (all in `Source/`)

| File | Purpose |
|---|---|
| `AccuracyTimeChart.h/.cpp` | Per-voicing accuracy line chart with BPM stepper. Shows rolling accuracy at selected BPM. |
| `PluginProcessor.h/.cpp` | Audio/MIDI processing hub. Owns MidiKeyboardState, APVTS, all libraries, engines, synth. |
| `PluginEditor.h/.cpp` | Top-level GUI. Hosts keyboard, chord display, tabbed library panel, practice panel. 60Hz timer. |
| `ExternalInstrument.h/.cpp` | VST3/AU plugin hosting. See `.claude/docs/external-instrument.md`. |
| `ChordyTheme.h` | Header-only constants -- all colors, font sizes, spacing, corner radii. |
| `ChordyLookAndFeel.h/.cpp` | Custom LookAndFeel_V4 subclass. Avenir Next font. |
| `ChordySynth.h/.cpp` | Built-in 2-operator FM piano synth, 12-voice polyphony. |
| `TempoEngine.h/.cpp` | Audio-thread tempo engine. Internal BPM clock, optional DAW sync, metronome click. |
| `BeatIndicatorComponent.h/.cpp` | Visual 4-dot beat indicator with BPM display. |
| `ChordDetector.h/.cpp` | Pitch-class template matching (triads through 13ths). Bass note +50 root bonus. |
| `ChordyKeyboardComponent.h/.cpp` | MidiKeyboardComponent subclass with colored key overlays (green/red/teal). |
| `FolderModel.h/.cpp` | Folder struct + FolderLibrary with ValueTree serialization. |
| `VoicingModel.h/.cpp` | Voicing struct + VoicingLibrary. findByNotes(), createFromNotes(), applyInversion(), applyDrop(). |
| `ProgressionModel.h/.cpp` | ProgressionChord (per-note timing) + Progression struct + ProgressionLibrary. |
| `ProgressionRecorder.h/.cpp` | MIDI recording with beat-relative timestamps. Two-phase analysis pipeline. |
| `MelodyModel.h/.cpp` | MelodyNote + MelodyChordContext + Melody struct + MelodyLibrary. |
| `SpacedRepetition.h/.cpp` | SM-2 engine with recency-weighted accuracy. Per-attempt `AttemptEntry` with BPM + timestamp for drill stats. |
| `ScaleModel.h/.cpp` | Scale definitions (incl. Bebop Major/Dominant), voicingFitsInScale(), diatonicTranspose(). |
| `VoicingStatsChart.h/.cpp` | 12-bar chart (C-B) showing per-key accuracy. Click-to-play callback. |
| `VoicingLibraryPanel.h/.cpp` | Voicing management: 4-state flow, multi-select, folders, MIDI/.chordy import/export. |
| `ProgressionLibraryPanel.h/.cpp` | Progression management: 6-state machine, count-in, quantize, transpose. |
| `MelodyLibraryPanel.h/.cpp` | Melody management: 5-state machine, chord context editor. |
| `ProgressionChartComponent.h/.cpp` | Dual-mode chart: detailed (piano-roll) and simple (chord rects). Draggable end marker. |
| `MelodyChartComponent.h/.cpp` | Note-on-beat-grid renderer. Row wrapping, note states, draggable end marker. |
| `PracticePanel.h/.cpp` | Practice GUI for voicing/progression/melody. 5 order modes (incl. Free), inversion/drop transforms, drill mode with weighted random key selection, scoring, chart preview. |
| `MidiFileUtils.h/.cpp` | MIDI file import/export (480 TPQ, beat-relative timestamps). |
| `LibraryExporter.h/.cpp` | .chordy XML collection format for sharing libraries. |
| `LilyPondExporter.h/.cpp` | LilyPond .ly generation + PDF rendering. See `.claude/docs/lilypond-export.md`. |
| `ExportSheetMusicDialog.h/.cpp` | Export options dialog: key selection, chord symbols, paper size. |
| `PlaceholderPanel.h` | Legacy/unused header. |

## MIDI Data Flow

```
processBlock() [AUDIO THREAD]
  |-- Capture per-note velocities -> noteVelocities[128] atomics
  |-- keyboardState.processNextMidiBuffer() -> bridges to GUI
  |-- Scan isNoteOn() for 128 notes -> atomic bitfield
  |-- If notes active: update lastPlayedNotes (mutex-protected)
  |-- Forward MIDI to ProgressionRecorder if recording (notes + pedal CC64)
  |-- Merge preview MIDI buffer (SpinLock-protected, from GUI thread)
  |-- Progression playback: replay raw MIDI events at beat positions
  |-- Melody playback: replay raw MIDI events at beat positions
  |-- If synthEnabled: ChordySynth.renderNextBlock()
  |-- Else: ExternalInstrument.processBlock()
  |-- Volume slider applied to output
  |-- tempoEngine.process() -> advance clock, render metronome
```

**Note**: Preview MIDI merge happens BEFORE playback so that note-offs from `stopProgressionPlayback()` land before new note-ons in the buffer. This ensures clean re-triggering of shared pitches when switching between voicings.

## Key Architecture Rules

- **Raw MIDI replay**: Both playback engines iterate through recorded `rawMidi` MidiMessageSequence, injecting every event (note-on, note-off, CC, pedal) at the correct beat position. Only reconstruct from analyzed data when transposition requires it.
- **On stop**: Both engines send pedal-off (CC64=0) before note-offs to prevent stuck sustain.
- **Preview MIDI path** (`addPreviewMidi`): Bypasses `keyboardState` entirely -- no keyboard flash, no interference with practice note detection.
- **`ProgressionRecorder.recording`**: `std::atomic<bool>` for audio thread safety. Reused for melody recording.
- **Two-phase chord analysis**: `extractNotes()` creates individual ProgressionNote objects from raw MIDI (matching note-on/off pairs), then `groupNotesIntoChords()` clusters by note-on proximity (0.5 beat gap threshold).
- **TempoEngine reset is deferred**: `resetBeatPosition()` sets `pendingReset` atomic; actual reset at start of next `process()` to avoid cutting off metronome clicks.
