# Chordy: Pricing, Distribution & Launch Strategy

## Context

Chordy is a JUCE-based VST3/AU/Standalone plugin for practicing chord voicings, progressions, and melodies with spaced repetition. No competing plugin combines spaced repetition + per-note scoring + DAW-integrated practice. The plan is freemium: a generous free Lite version for students/teachers, with Pro subscription for ongoing practice features.

**STATUS**: Lite/Pro feature split still being finalized. Do not implement feature gating until the split is confirmed. Pricing model shifted to subscription (see below).

---

## Lite (Free) vs Pro (Subscription) Feature Split (DRAFT -- subject to change)

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

### Chordy Pro (Subscription) -- Smart Practice + Full Power

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
- **Subscription fits the product**: Chordy is a practice platform, not a static tool. Spaced repetition delivers ongoing value that compounds over time -- closer to Melodics or a language learning app than a reverb plugin. Subscription funds ongoing development (comping rhythms, scale patterns, chord interleaving).
- **Teachers recommend Lite freely**: No barrier to classroom adoption. Students upgrade when ready.
- **Single binary**: Feature-gated at runtime, not two separate builds.

---

## Pricing

| Tier | Price |
|---|---|
| Chordy Lite | Free forever |
| Pro monthly | $4/mo |
| Pro annual | $36/yr (save 25%) |
| Pro lifetime (optional) | $69-79 one-time (captures "I refuse to subscribe" segment) |

Lower entry point than one-time $49 drives more initial conversions. Annual plan captures committed users. Lifetime option prevents losing anti-subscription buyers entirely.

---

## Distribution

1. **Own website + Lemon Squeezy** -- handles subscriptions, VAT/tax, license keys
2. **KVR product listing** (free) -- discoverability
3. **Plugin Boutique** (later) -- 30-40% cut, huge audience

**Copy protection**: Simple serial key, validated against Lemon Squeezy API on first entry, cached locally. Subscription status checked periodically (with grace period for offline use). No iLok/PACE.

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

## Launch Timeline

Target: ~6-8 weeks from start to public launch (starting April 2026).

### Phase 1: Beta-Ready Build (Week 1-2, April 1-14)

- [ ] Daily dogfooding -- use Chordy yourself, note every rough edge, crash, or confusing flow. Fix blockers.
- [ ] Create installer (.pkg or .dmg) that installs VST3 + AU + Standalone. Beta testers won't build from source.
- [ ] Ship 3-5 preset voicings (ii-V-I staples: Dm9, G13, Cmaj9, etc.) so first launch isn't a blank screen.

### Phase 2: Beta Community (Week 2-3, April 14-28)

- [ ] Recruit 10-20 beta testers: r/jazzpiano, r/musictheory, KVR forums, jazz Discord servers (Piano World, Jazz Guitar Online). Be honest: "I built a voicing practice plugin with spaced repetition, looking for beta testers."
- [ ] Set up feedback channel -- Google Form or small Discord server. Collect: instrument, DAW, what broke, what confused them, what they loved.
- [ ] Iterate for 2-3 weeks on tester feedback. Pay attention to what they don't understand without explanation -- that's what the demo video needs to cover.

### Phase 3: Demo Video (Week 4-5, April 28 - May 12)

- [ ] Record main demo (2-3 min) following the workflow in Demo Video Plan below.
- [ ] Cut 30-60 sec vertical clips for social media (TikTok/Instagram/YouTube Shorts). "Watch me practice a ii-V-I through all 12 keys" is inherently visual.

### Phase 4: Landing Page + Mailing List (Week 5-6, May 12-26)

- [ ] Landing page: hero with demo video, 3-4 feature bullets, "Download Chordy Lite (Free)" button, email signup for Pro launch. Use Carrd ($19/yr) or simple Next.js on Vercel.
- [ ] Mailing list: Mailchimp free tier or Buttondown. This is the #1 conversion channel.
- [ ] KVR product listing (free) -- gets you in front of plugin buyers searching for theory/practice tools.
- [ ] Reach out to jazz educators (Open Studio Jazz, PianoGroove, Learn Jazz Standards) with beta access.

### Phase 5: Launch (Week 7-8, May 26 - June 9)

- [ ] Implement license system (Lemon Squeezy integration + feature gating) -- only after pricing is finalized.
- [ ] Release Lite + Pro simultaneously.
- [ ] Announce: KVR, Reddit, Bedroom Producers Blog, SYNTH ANATOMY.
- [ ] YouTube reviewers who cover theory tools -- personalized emails + free Pro access.
- [ ] Jazz educator outreach with free classroom Pro access.
- [ ] Educational content: "Why spaced repetition works for learning voicings."

### Post-Launch

- Feature updates (comping rhythms, chord interleaving, scale patterns) drive re-engagement and reduce churn.
- Plugin Boutique submission once you have reviews.

### Critical Path

Build -> Testers -> Video -> Landing Page -> Launch. Each phase depends on the previous -- beta feedback shapes the demo, demo goes on the landing page, landing page collects the audience for launch.

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
