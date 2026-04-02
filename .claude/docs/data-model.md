# Data Model

## Structs

**Folder**: `id` (UUID), `name`, `sortOrder`

**Voicing**: `id`, `name`, `quality`, `alterations`, `rootPitchClass`, `intervals`, `velocities` (parallel to intervals), `octaveReference`, `folderId`, `createdAt`. `rootPitchClass` defaults to lowest note played; users override manually for rootless voicings. `VoicingLibrary::applyInversion(notes, n)` / `applyDrop(notes, drops)` transform note arrays for practice.

**ProgressionChord**: `intervals`, `rootPitchClass` (0-11), `quality`, `alterations`, `name`, `linkedVoicingId`, `startBeat`, `durationBeats`, `midiNotes`, `midiVelocities`, `noteStartBeats` (per-note), `noteDurations` (per-note). `getDisplayName()` generates from root+quality+alterations, includes slash bass if lowest != root.

**ProgressionNote** (in ProgressionRecorder.h): `midiNote`, `velocity`, `startBeat`, `durationBeats` -- intermediate struct for `extractNotes()` pipeline.

**AttemptEntry** (in SpacedRepetition.h): `quality` (0-5), `bpm` (float), `timestamp` (seconds since epoch). Stored in `PracticeRecord::detailedHistory` (capped at 500). Used for BPM-differentiated accuracy stats.

**Progression**: `id`, `name`, `keyPitchClass`, `mode`, `chords`, `totalBeats`, `bpm`, `timeSignatureNum/Den`, `rawMidi` (MidiMessageSequence), `quantizeResolution` (0=raw, 1.0=beat, 0.5=half, 0.25=quarter), `folderId`, `createdAt`

**MelodyNote**: `intervalFromKeyRoot`, `startBeat`, `durationBeats`, `velocity`

**MelodyChordContext**: `intervalFromKeyRoot`, `quality`, `alterations`, `startBeat`, `durationBeats`, `name`

**Melody**: `id`, `name`, `keyPitchClass`, `chordContexts`, `notes`, `totalBeats`, `bpm`, `timeSignatureNum/Den`, `rawMidi`, `folderId`, `createdAt`

Melody note intervals are relative to `keyPitchClass` -- transposition shifts keyPitchClass, intervals stay the same.

## APVTS Parameters

| ID | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `practiceMode` | Bool | -- | false | Practice mode flag |
| `midiChannel` | Int | 1-16 | 1 | MIDI channel |
| `bpm` | Float | 30-300 (step 5) | 120 | Internal metronome tempo |
| `metronomeOn` | Bool | -- | false | Metronome (auto-enabled, no UI toggle) |
| `useHostSync` | Bool | -- | false | Sync to DAW transport |
| `timedPractice` | Bool | -- | false | Enable timed scoring |
| `responseWindowBeats` | Float | 1-8 | 4 | Beats before timeout |
| `synthEnabled` | Bool | -- | true | true=internal synth, false=external |
| `synthVolume` | Float | 0-1 | 0.7 | Output volume (both internal/external) |
| `metronomeVolume` | Float | 0-1 | 0.7 | Metronome click volume |

## Serialization

- All libraries + SpacedRepetition + ExternalInstrument state serialize as ValueTree/XML children of APVTS state.
- Raw MIDI stored as compact `"time:byte1:byte2:byte3;..."` string.
- Velocities stored as comma-separated int strings.
- Each library's ValueTree includes a `<Folders>` child. `folderId` and `createdAt` attributes backward compatible (empty/0 defaults).
- **Detailed history**: `AttemptEntry` serialized as `"quality:bpm:timestamp;..."` compact string per PracticeRecord. Backward compatible (empty = no data for old records).
- **Shared library file**: Also saved to `~/Library/Chordy/libraries.xml` for DAW-Standalone sync. Loaded on construction, saved on every library modification, practice stop, and plugin destruction.
- **`.chordy` collection format**: `<ChordyCollection>` root with `<VoicingLibrary>`, `<ProgressionLibrary>`, `<MelodyLibrary>` sections. UUID-based duplicate skipping on import. Folders preserved.
