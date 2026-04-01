# Chordy: Pricing, Distribution & Launch Strategy

## Context

Chordy is a JUCE-based VST3/AU/Standalone plugin for practicing chord voicings, progressions, and melodies with spaced repetition. No competing plugin combines spaced repetition + per-note scoring + DAW-integrated practice. The plan is freemium: a generous free Lite version for students/teachers, with Pro features (spaced rep, external instruments, melody mode) behind a $49 one-time purchase. Demo content and community seeding come first.

**STATUS**: Lite/Pro feature split still being finalized. Do not implement feature gating until the split is confirmed.

---

## Lite (Free) vs Pro ($49) Feature Split (DRAFT -- subject to change)

### Chordy Lite (Free) -- Genuinely Useful

| Feature | Details |
|---|---|
| Voicing library | Save, browse, organize in folders, unlimited voicings |
| Voicing practice | Chromatic + Random order modes |
| Progression recording | Record, quantize (Raw/Beat/1/2/1/4), edit chords |
| Progression practice | Per-note scoring, chart display, count-in |
| Chord detection | Always-on display |
| Internal synth | Built-in FM piano |
| MIDI keyboard input | Full range |
| Basic stats | Accuracy per key chart |

### Chordy Pro ($49) -- Smart Practice + Full Power

| Feature | Why it's Pro |
|---|---|
| **Spaced repetition** | The killer feature -- smart scheduling replaces sequential drilling |
| **External instrument hosting** | Use Keyscape, Pianoteq, any VST3/AU -- niche but high-value |
| **Melody recording & practice** | Third practice mode (future: scale patterns) |
| **Follow / Scale / Diatonic / Free modes** | Advanced practice order modes |
| **Inversion & drop transforms** | Drop 2, drop 3, inversions during practice |
| **Timed practice** | Response window scoring |
| **LilyPond PDF export** | Sheet music generation |
| **.chordy import/export** | Library sharing |

### Why this split works
- **Lite solves a real problem**: Drill voicings + progressions through keys with a built-in synth. Students and teachers can use it fully without paying.
- **Pro upgrade is natural**: After weeks of sequential drilling, you *want* spaced rep to stop forgetting. External instruments appeal to serious players. Melody mode adds depth.
- **Teachers recommend Lite freely**: No barrier to classroom adoption. Students upgrade when ready.
- **Single binary**: Feature-gated at runtime, not two separate builds.

---

## Pricing

| Tier | Price |
|---|---|
| Chordy Lite | Free forever |
| Pro launch price (first 2-3 weeks) | $29 |
| Pro regular price | $49 |

Stable pricing after launch (Valhalla model -- no sales/discounts).

---

## Distribution

1. **Own website + Lemon Squeezy** -- 5% + $0.50, handles VAT/tax, generates license keys
2. **KVR product listing** (free) -- discoverability
3. **Plugin Boutique** (later) -- 30-40% cut, huge audience

**Copy protection**: Simple serial key, validated against Lemon Squeezy API on first entry, cached locally for offline use. No iLok/PACE.

---

## Demo Video Plan

One main demo showing the natural workflow (2-3 min):

1. **Start in Progression mode** -- record a jazz progression (e.g., ii-V-I-VI)
2. **Practice the progression** in 2 different keys -- show per-note scoring, chart highlighting
3. **Zero in on one chord** -- click a chord, "Save as Voicing"
4. **Switch to Voicing practice** -- drill that chord through keys
5. **Try Scale mode** (Pro) -- show diatonic transposition
6. **Try a different inversion** (Pro) -- show drop 2 transform
7. **Change the sound** (Pro) -- switch from internal synth to an external VST
8. Brief mention of spaced repetition at the end -- "the plugin remembers what you need to practice"

Also create 30-60 sec clips from this for social media (TikTok/Instagram/YouTube Shorts).

---

## Launch Strategy

### Phase 1: Demo Content + Beta Community (START HERE)

- Record the demo video described above
- Post in r/jazzpiano, r/musictheory, KVR forums asking for beta testers
- Reach out to jazz educators (Open Studio Jazz, PianoGroove, Learn Jazz Standards)
- Educational content: "Why spaced repetition works for learning voicings"

### Phase 2: Landing Page + Email List

- Landing page with demo video + email signup + "Get Chordy Lite free" button
- Link from all community posts and video descriptions
- ~10% of email signups convert to paying customers

### Phase 3: Launch

- Release Lite + Pro simultaneously
- $29 intro Pro price for 2-3 weeks
- Announce: KVR, Reddit, Bedroom Producers Blog, SYNTH ANATOMY
- YouTube reviewers who cover theory tools -- personalized emails + free Pro license
- Jazz educator outreach with free classroom Pro licenses

### Phase 4: Post-Launch

- $49 regular Pro price
- Feature updates (comping rhythms, chord interleaving, scale patterns) drive re-engagement
- Plugin Boutique submission once you have reviews

---

## Code Implementation for Freemium (when ready)

### Files to create
- `Source/LicenseManager.h/.cpp` -- license key storage, validation, `isProUnlocked()` method

### Files to modify
- `CMakeLists.txt` -- add LicenseManager source files
- `PluginProcessor.h/.cpp` -- own LicenseManager, expose `isProUnlocked()`
- `PluginEditor.h/.cpp` -- show upgrade prompt, Pro badges on locked features
- `PracticePanel.h/.cpp` -- gate Follow/Scale/Free modes, inversions/drops, timed practice, SR weighting
- `MelodyLibraryPanel.h/.cpp` -- gate entire panel (show lock overlay)
- `SpacedRepetition.h/.cpp` -- `getNextChallenge()` falls back to sequential in Lite
- `ExternalInstrument.h/.cpp` -- gate instrument selector (show upgrade prompt)
- `LilyPondExporter.h/.cpp` -- gate export
- `LibraryExporter.h/.cpp` -- gate .chordy import/export

### Upgrade prompt UI
- When user clicks a locked feature: dialog showing what Pro includes + "Enter License Key" + "Get Chordy Pro" (opens website)
- Small "PRO" badges on locked UI elements (tabs, buttons, combo boxes)
- Non-intrusive -- locked features are visible but clearly marked, not hidden

### License key flow
1. User purchases on website -- receives serial key via email
2. In Chordy: Settings or menu -- "Activate Pro" -- enter key
3. Key validated against Lemon Squeezy API (requires internet once)
4. Cached at `~/Library/Application Support/Chordy/license.key`
5. Works offline after initial activation

---

## Market Context

No existing VST/AU plugin combines spaced repetition + per-note scoring + DAW-integrated practice. Competitors:

| Product | Price | Positioning |
|---|---|---|
| Scaler 3 | $79-99 | Composition tool, not practice |
| Captain Chords | $99 | Composition tool |
| ChordPrism 2 | $50 | Detection + suggestion, no practice |
| iReal Pro | $16 | Backing tracks, no DAW integration or scoring |
| Melodics | $100-150/yr | Subscription, no jazz focus |

## Realistic Expectations

- Lite downloads could be 10-50x Pro sales -- each is a potential future customer + word of mouth
- With good launch execution: 100-500 Pro sales in first months is ambitious but achievable
- Marketing effort = development effort
- The email list is the #1 conversion channel
