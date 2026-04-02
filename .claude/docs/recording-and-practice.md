# Recording and Practice

## Recording

All recording (voicings, progressions, melodies) captures:
- **Velocity**: Per-note velocity stored and preserved during playback/preview.
- **Sustain pedal**: CC64 events recorded and replayed.
- **Raw MIDI**: Original performance preserved. All quantize levels re-analyze from raw MIDI (non-destructive -- switching quantize never destroys detail).
- **Quantize modes**: Raw (exact timing), Beat (1.0), 1/2 (0.5), 1/4 (0.25). Raw is the default on recording stop.
- **totalBeats** rounds up to nearest 4-beat bar to preserve trailing silence.

### Two-Pass Velocity Capture

Per-note velocities tracked via `noteVelocities[128]` atomic array in processor. Two passes in processBlock: first before `processNextMidiBuffer` catches external MIDI, second after catches on-screen/QWERTY keyboard events. Without the second pass, on-screen keyboard voicing recordings get zero velocities.

## Practice Modes

### Order Modes
- **Chromatic** (default): Up-and-back through all 12 keys (C, C#, ..., B, Bb, ..., C#).
- **Random**: Random key selection.
- **Follow** (voicing only): Root walks along a chosen scale or melody's pitch classes. Pure transposition.
- **Scale/Diatonic** (voicing only): Each note moves independently by scale degrees via `diatonicTranspose()`. Only available when `voicingFitsInScale()` returns true.
- **Free** (voicing only): No target key -- user plays the voicing in any key. Matches played pitch classes against the voicing's interval pattern in all 12 transpositions. Green feedback on match with detected key name, red on wrong notes. No scoring, no auto-advance.

Follow, Scale, and Free are disabled for progression/melody practice.

### Inversion & Drop (voicing practice)

Always visible for voicing practice, between buttons and toggle row. Two combo boxes:

- **Inversion**: Root Position, 1st Inversion, 2nd Inversion, ..., (N-1)th. Moves the bottom note to the first occurrence of its pitch class above the current highest note (not just +12). Iterative -- each inversion step operates on the result of the previous.
- **Drop**: No Drop, Drop 2, Drop 3, ..., Drop 2+4. Moves the Nth voice from the top to the first occurrence of its pitch class below the current lowest note (standard jazz drop voicing counting from top). Drop 2+4 drops both 2nd and 4th voices simultaneously (requires 5+ notes).

Transform pipeline: transpose to key -> apply inversion -> apply drop. Both transforms and the resulting notes are used consistently for practice targets, previews, chart display, and Play/spacebar playback. Velocities are preserved through transforms by pitch-class matching.

Inversion/drop settings persist when the same voicing is re-selected, reset when switching to a different voicing. `createFromNotes()` defaults root to lowest note played (users override manually for rootless voicings).

### Scoring Rules

- **Per-note independent scoring** -- each note is scored individually. Never chord-level. Chord grouping causes cascading false positives.
- **Voicing practice (untimed)**: Timing-based quality. `markChallengeStart()` on challenge load, `computeQuality(beatsElapsed)` on correct: <0.5 beats=Perfect(5), <1.0=Good(4), <1.5=OK(3), else=Slow(2).
- **Progression practice**: Each note scored by whether user plays its pitch class during `[noteStartBeat, noteStartBeat+noteDuration)` window. Notes: Target (amber) when active, Correct (green) when played, Missed (red) when expired.
- **Melody practice**: Per-note pitch-class matching with 0.5-beat coyote time for early hits. Supports overlapping notes (multiple simultaneous targets scored in the same frame). `scored` flag prevents double-scoring.
- **Proportional scoring on skip/timeout**: Missing half the notes = ~50% quality. `computeProportionalMatch()` computes correct/total minus 0.1 per extra note. Maps to SM-2 quality 0-5 via `proportionToQuality()`.
- **`getNextChallenge`** accepts `avoidKey` parameter to prevent repeating the same root consecutively.
- **Library locked during practice**: Record/Play/Edit/Delete buttons, ListBox, search, folder combo, "..." button disabled. Library panel shows AccuracyTimeChart + per-key stats during practice.

### Drill Mode (weighted random key selection)

Toggle "Drill" enables adaptive key selection. When active, Custom button is grayed out and key picker is hidden (drill is random-only).

- **Weighted random selection**: Each key gets a weight based on EMA quality: `weight = max(0.05, (5 - emaQuality)^3)`. Cubic formula gives weak keys ~64x the frequency of mastered keys. Floor of 0.05 means mastered keys still appear rarely.
- **EMA tracking**: `DrillKeyState` with alpha=0.5. First attempt initializes directly, subsequent uses exponential moving average. Stronger recency bias than lifetime stats.
- **Mastery thresholds**: Per-practice-type. Melody: 80% (emaQuality >= 4.0). Voicing/Progression: 90% (emaQuality >= 4.5). Minimum 3 attempts before mastery.
- **Auto BPM toggle**: Visible in toggle row when drill is on (default: enabled). When all keys reach mastery, BPM auto-increments by 5 and all key states reset. BPM parameter uses step size of 5.
- **Drill status**: Displayed on library panel during practice ("3/12 mastered | BPM 125 (+5)").
- **Key selection**: `pickDrillKey()` does weighted random draw excluding the last-played key to prevent consecutive repeats.

### Spaced Repetition

SM-2 algorithm with recency-weighted accuracy: `attemptHistory` vector (capped at 100). Most recent 10 attempts get 2x weight. Backward compatible with legacy successes/failures counts.

### BPM-Differentiated Stats

- **AttemptEntry**: Each practice attempt stores `quality`, `bpm`, `timestamp` in `detailedHistory` (capped at 500 per PracticeRecord).
- **AccuracyTimeChart**: Line chart on library panels showing rolling accuracy at a selected BPM. BPM stepper navigates between practiced BPM levels. Changing BPM in stepper sets the actual tempo.
- **Per-key stats filtered by BPM**: During practice, VoicingStatsChart shows accuracy at current BPM only. When browsing, defaults to max practiced BPM. Uses `getStatsForVoicingAtBpm()`.
- **BPM badge on items**: Library list items show max practiced BPM. Selecting an item auto-sets global BPM to its max practiced BPM.
- **Stats toggle button**: On each library panel, toggles AccuracyTimeChart view even when not practicing.

## Melody-Specific

- **Backing pad**: Root note only. Placed in C3-F#3 range. Backing toggle visible only for melody practice.
- **Intervals relative to key root**: `intervalFromKeyRoot` stored per note. Transposition shifts `keyPitchClass`, intervals unchanged.
- **Multi-note support**: Overlapping notes (legato lines) can be scored simultaneously. Timed mode checks all notes active at current beat (with 0.5 beat coyote). Untimed mode checks from `melodyNoteIndex` forward through overlapping notes.

## Progression-Specific

- **No auto chord grouping**: Recording puts all notes in a single group with no chord label. Users manually add chord boundaries in edit mode. Per-note timing preserved for practice scoring.
- **Chord detection**: `ChordDetector::detect()` with +50 bass=root bonus. Used for real-time display above keyboard and voicing identification, NOT for progression grouping.
