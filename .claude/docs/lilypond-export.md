# LilyPond PDF Export (Experimental)

Sheet music export via LilyPond for voicings, progressions, and melodies. Converts raw MIDI data to `.ly` notation files, then renders to PDF.

## Dependencies

- **LilyPond** must be installed on the system. Detected via `findLilyPondBinary()`: checks bundled location (`~/Library/Application Support/Chordy/lilypond/`), `which lilypond`, then common macOS paths (`/opt/homebrew/bin/`, `/usr/local/bin/`).
- **Ghostscript** required by LilyPond for PDF output. GUI apps don't inherit shell PATH, so `renderToPdf()` runs lilypond via `/bin/sh -c` with `export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"`.
- Future: auto-download/bundle LilyPond, Windows/Linux support.

## Source Files

| File | Purpose |
|---|---|
| `LilyPondExporter.h/.cpp` | Core export pipeline: `.ly` generation + PDF rendering |
| `ExportSheetMusicDialog.h/.cpp` | Export options dialog: key selection, chord symbols, paper size |

## Architecture

### Export Options (`ExportOptions` struct)
- `keys`: vector of pitch classes to export (subset of 12)
- `title`, `composer`: header text
- `includeChordSymbols`: show chord names above staff
- `grandStaff`: piano (treble+bass) vs single treble
- `paperSize`: "letter" or "a4"

### Three Export Functions
- `generateVoicingLy()`: 1 voicing per bar (whole note), 4 bars per line. Simple treble/bass split.
- `generateProgressionLy()`: Per-note raw MIDI rendering with polyphonic voice splitting, grace note detection, adaptive treble/bass split. Most complex.
- `generateMelodyLy()`: Single treble staff per key with chord contexts as `\new ChordNames`.

### Progression Export Pipeline
1. **Transpose** each key via `ProgressionLibrary::transposeProgression()` (shifts rawMidi note events)
2. **Extract notes** from raw MIDI via `ProgressionRecorder::extractNotes()`
3. **Grace note detection** (before quantization): notes < 0.3 beats AND < 50% of target duration, within 0.5 beats of a longer note on the same staff
4. **Quantize** remaining notes to sixteenth grid (`snapBeat`)
5. **Treble/bass split** with adaptive split point: `splitPoint = 60 + semitones` preserves original staff assignment across transpositions
6. **Voice splitting** (`generateStaffNotes`): duration-based classification into Voice 1 (held/sustained, stems up) and Voice 2 (moving/melodic, stems down)
7. **LilyPond rendering** (`generateMonoVoice`): beat decomposition with barline-respecting ties, `\acciaccatura` for grace notes

### Key Design Decisions

#### Adaptive Treble/Bass Split
The split threshold adjusts with transposition: `splitPoint = 60 + semitones`. Without this, transposing up causes bass notes to leak into the treble staff (e.g., Key of B's chromatic bass descent appears in treble Voice 2).

#### Grace Notes
- `\acciaccatura` (slashed, with slur to target) renders inside polyphonic voices
- Grace notes at beat 0 of a key section appear before the barline at system breaks -- this is a known LilyPond limitation. Works correctly for mid-bar grace notes.
- Detection threshold: duration < 0.3 beats AND < 50% of target note's duration

#### Time Signature
- Upper staff: `\override Staff.TimeSignature.style = #'numbered \time 4/4`
- Lower staff: `\new Staff \with { \omit TimeSignature }`
- Layout: `\context { \Voice \omit TimeSignature }` prevents implicit voices from `\\` showing extra time sigs

#### Layout Control (Progressions)
- Multi-bar keys (2-3 bars): `line-width` constraint in `\paper` (~52mm per bar) forces 1 key per line. LilyPond auto-breaks at `\bar "||"` positions.
- Single-bar keys: no constraint, multiple keys per line.
- **Never use forced `\break`** -- it creates empty systems when combined with grace notes at system boundaries.
- Enharmonic spelling: `keyUsesSharps()` determines sharp/flat per key. Thread-local `g_useSharps` context avoids parameter threading.
- Chord roots use standard jazz naming (Db, Eb, Ab, Bb) regardless of key.

#### Debug Output
`/tmp/chordy_debug.ly` is written on every export for inspection. Remove before release.

## Known Limitations (Experimental)

- Grace notes at beat 0 of a key section appear before the system barline (LilyPond limitation with `\acciaccatura`/`\slashedGrace` at negative musical time)
- Line-width constraint for 1-key-per-line is tuned for typical progressions (~52mm/bar) and may not be optimal for all content densities
- No test infrastructure
- macOS only (PATH handling in `renderToPdf`)
