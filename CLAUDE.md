# Chordy

Chord voicing, progression, and melody/lick practice plugin with spaced repetition and tempo-based drilling. JUCE C++17, targets VST3/AU/Standalone on macOS. Company: "JezzterInc".

## Build Commands

```bash
make build    # cmake -B cmake-build && cmake --build cmake-build
make run      # build + open Standalone app
```

Build a specific format:
```bash
cmake --build cmake-build --target Chordy_VST3
cmake --build cmake-build --target Chordy_AU
cmake --build cmake-build --target Chordy_Standalone
```

Artifacts: `cmake-build/Chordy_artefacts/Standalone/Chordy.app`

## JUCE Dependency

Git submodule at `./JUCE/` via `add_subdirectory(JUCE)`.

## Source Files

All source in `Source/`. New `.cpp`/`.h` files **must** be added to `SOURCE_FILES` in `CMakeLists.txt`.

## Plugin Configuration

- MIDI Input: enabled | MIDI Output: enabled | IS_SYNTH: false
- Formats: VST3, AU, Standalone
- Manufacturer code: `Tap1` | Plugin code: `Chrd`
- `EDITOR_WANTS_KEYBOARD_FOCUS: TRUE`
- `COPY_PLUGIN_AFTER_BUILD=TRUE` (auto-installs to system plugin directories)

## Critical Rules

- **Audio thread** (`processBlock`): no allocations, no blocking, no locks. Exceptions: `lastPlayedNotes` mutex (only when notes active), `previewMidiLock` SpinLock (brief), `pluginLock` CriticalSection (ScopedTryLock).
- **Playback uses raw MIDI replay** by default. Only reconstruct from analyzed data when transposition requires it.
- **Quantize always re-analyzes from raw MIDI** (non-destructive). Switching between Raw/Beat/1/2/1/4 never destroys the original recording.
- **Per-note independent scoring**, never chord-level. See `.claude/docs/recording-and-practice.md`.
- **Avoid UTF-8 special characters** in displayed strings -- JUCE font rendering may not handle them.
- No test infrastructure yet.
- Planned: comping rhythm templates.

## Detailed Context

@.claude/docs/architecture.md
@.claude/docs/data-model.md
@.claude/docs/external-instrument.md
@.claude/docs/theme-and-ui.md
@.claude/docs/recording-and-practice.md
@.claude/docs/lilypond-export.md
