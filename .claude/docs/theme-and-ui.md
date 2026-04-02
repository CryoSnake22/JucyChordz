# Theme and UI

## Theme Rules

- **All styling in `ChordyTheme.h`** -- never hardcode hex colors or font sizes.
- **`ChordyLookAndFeel.h/.cpp`** for widget styling: flat rounded buttons, pill toggles, thin-track sliders, underline tabs.
- **Font**: Avenir Next via `setDefaultSansSerifTypefaceName()`. Requires `setDefaultLookAndFeel()` (not just per-component).
- **Avoid UTF-8 special characters** in displayed strings. JUCE's font rendering may not handle em-dashes, arrows, etc. Use plain ASCII.

## Color Palette

- **Palette**: Warm charcoal + amber accent.
- **Keyboard overlays**: `keyCorrect` (green, semi-transparent), `keyWrong` (red, semi-transparent), `keyTarget` (light teal). Green reserved for correct answers during practice only. Teal for browsing/preview/playback.
- **Chart notes**: Default=neutral grey, Target=amber outline, Correct=green fill, Missed=red fill. 2px rounding. No note names on notes.
- **Chart backgrounds**: Note area uses `bgSurface`. Chord label bar uses `melodyChordBg`. Bar lines at 0.4f alpha.

## UI Consistency

- List item names: 15pt. Secondary text (dates, badges): 12pt. Row height: 36px. Stats chart: 60px.
- Delete buttons use `dangerMuted`. Practice panel has square corners.
- Theme font sizes: `fontSectionHead=15`, `fontBody=14`, `fontSmall=12`, `fontMeta=11`.
- Chord labels hidden when chart column width < 40px.

## GUI Layout (1100x740)

```
+-----------------------------------------------------+
| CHORDY                                              | 40px header
+-----------------------------------------------------+
| Key: Eb    [chord detection always centered]        | 52px chord display
| [countdown]  [Up next: G]                           | 28px sub-labels
+-----------------------------------------------------+
| === ChordyKeyboardComponent (keyWidth=28) ========= | 140px keyboard (C2-C7)
+-----------------------------------------------------+
| [Settings/Hide] BPM:[slider] [Click][Sync] ...     | 36px tempo bar (collapsible)
+----------------------+------------------------------+
| [Voicings|Prog|Mel]  | [Chart preview / practice]   |
| Library panel        | Practice panel               |
| (list, search,       | (chart, controls, scoring,   |
|  folders, stats,     |  key selector, feedback)     |
|  record/play/edit)   |                               |
+----------------------+------------------------------+
```

- **Chord display center**: Always shows detected chord (keyboard, playback, or clicked preview). Never blocked by practice.
- **Left overlay**: "Key: X" (36pt amber) during practice. Countdown below. "Up next: G" below chord display.
- **Tempo bar**: Collapsible via Settings toggle. BPM shown as `[-][value][+]` buttons (5 BPM increments, editable text field snaps to nearest 5). Metronome auto-enabled (no toggle, volume slider only). Contains instrument mode (Internal/External), plugin selector, Scan/Open buttons, volume sliders.
