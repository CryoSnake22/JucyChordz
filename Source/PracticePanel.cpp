#include "PracticePanel.h"
#include "PluginProcessor.h"
#include "ChordyKeyboardComponent.h"
#include <algorithm>
#include <set>

PracticePanel::PracticePanel (AudioPluginAudioProcessor& processor,
                               ChordyKeyboardComponent& keyboard)
    : processorRef (processor), keyboardRef (keyboard)
{
    headerLabel.setText ("PRACTICE", juce::dontSendNotification);
    headerLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (headerLabel);

    targetLabel.setText ("Select a voicing and press Start", juce::dontSendNotification);
    targetLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    targetLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    targetLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (targetLabel);

    feedbackLabel.setFont (juce::FontOptions (14.0f));
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFAABBCC));
    feedbackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (feedbackLabel);

    statsLabel.setFont (juce::FontOptions (13.0f));
    statsLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF889999));
    statsLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statsLabel);

    startButton.onClick = [this] { onStartStop(); };
    addAndMakeVisible (startButton);

    nextButton.onClick = [this] { onNext(); };
    nextButton.setEnabled (false);
    addAndMakeVisible (nextButton);

    playButton.onClick = [this] { onPlay(); };
    playButton.setEnabled (false);
    addAndMakeVisible (playButton);

    customButton.onClick = [this] { onCustomToggle(); };
    addAndMakeVisible (customButton);

    timedToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xFFAABBCC));
    timedToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xFF44CC88));
    addAndMakeVisible (timedToggle);

    // Key selector toggles (hidden by default)
    static const char* noteNames[] = { "C", "Db", "D", "Eb", "E", "F",
                                        "Gb", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
    {
        keyToggles[i].setButtonText (noteNames[i]);
        keyToggles[i].setToggleState (true, juce::dontSendNotification);
        keyToggles[i].setColour (juce::ToggleButton::textColourId, juce::Colour (0xFFAABBCC));
        keyToggles[i].setColour (juce::ToggleButton::tickColourId, juce::Colour (0xFF44CC88));
        addChildComponent (keyToggles[i]);
    }

    selectAllButton.onClick = [this] {
        for (int i = 0; i < 12; ++i)
            keyToggles[i].setToggleState (true, juce::dontSendNotification);
    };
    addChildComponent (selectAllButton);

    deselectAllButton.onClick = [this] {
        for (int i = 0; i < 12; ++i)
            keyToggles[i].setToggleState (false, juce::dontSendNotification);
    };
    addChildComponent (deselectAllButton);

    orderCombo.addItem ("Random", 1);
    orderCombo.addItem ("Chromatic", 2);
    orderCombo.setSelectedId (1, juce::dontSendNotification);
    addChildComponent (orderCombo);

    timingFeedbackLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    timingFeedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF889999));
    timingFeedbackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timingFeedbackLabel);

    updateStats();
}

void PracticePanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xFF222244));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

void PracticePanel::resized()
{
    auto area = getLocalBounds().reduced (8);

    headerLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    targetLabel.setBounds (area.removeFromTop (36));
    area.removeFromTop (4);

    auto buttonRow = area.removeFromTop (30);
    int buttonWidth = (buttonRow.getWidth() - 12) / 4;
    startButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    nextButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    playButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    customButton.setBounds (buttonRow);

    area.removeFromTop (4);
    timedToggle.setBounds (area.removeFromTop (24));

    // Key selector (conditionally visible)
    if (showingKeySelector)
    {
        area.removeFromTop (4);
        // Row 1: C through F (6 keys)
        auto keyRow1 = area.removeFromTop (22);
        int kw = keyRow1.getWidth() / 6;
        for (int i = 0; i < 6; ++i)
            keyToggles[i].setBounds (keyRow1.removeFromLeft (kw));

        area.removeFromTop (2);
        // Row 2: Gb through B (6 keys)
        auto keyRow2 = area.removeFromTop (22);
        for (int i = 6; i < 12; ++i)
            keyToggles[i].setBounds (keyRow2.removeFromLeft (kw));

        area.removeFromTop (2);
        // [All] [None] [Order combo]
        auto controlRow = area.removeFromTop (24);
        selectAllButton.setBounds (controlRow.removeFromLeft (40));
        controlRow.removeFromLeft (4);
        deselectAllButton.setBounds (controlRow.removeFromLeft (45));
        controlRow.removeFromLeft (8);
        orderCombo.setBounds (controlRow);
    }

    area.removeFromTop (8);
    feedbackLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (2);
    timingFeedbackLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);
    statsLabel.setBounds (area.removeFromTop (24));
}

// --- Start / Stop ---

void PracticePanel::onStartStop()
{
    if (practicing)
    {
        stopPractice();
        return;
    }

    // Start: use currently selected voicing, or first available
    auto& lib = processorRef.voicingLibrary;
    if (lib.size() == 0)
    {
        targetLabel.setText ("Record a voicing first!", juce::dontSendNotification);
        return;
    }

    // Block start if custom mode with no keys selected
    if (customMode)
    {
        bool anySelected = false;
        for (int i = 0; i < 12; ++i)
            if (keyToggles[i].getToggleState())
                anySelected = true;
        if (! anySelected)
        {
            targetLabel.setText ("Select at least one key!", juce::dontSendNotification);
            return;
        }
    }

    // Turn metronome on
    if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
        param->setValueNotifyingHost (1.0f);

    // Use selected voicing, or fall back to first available
    juce::String voicingToUse = selectedVoicingId;
    if (voicingToUse.isEmpty() || processorRef.voicingLibrary.getVoicing (voicingToUse) == nullptr)
        voicingToUse = lib.getAllVoicings()[0].id;

    startPractice (voicingToUse);
}

void PracticePanel::startPractice (const juce::String& voicingId)
{
    practicing = true;
    practicingVoicingId = voicingId;
    challengeCompleted = false;

    // Update button state
    startButton.setButtonText ("Stop");
    startButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF882222));
    nextButton.setEnabled (true);
    playButton.setEnabled (true);

    // Get first challenge
    if (customMode)
    {
        buildCustomKeySequence();
        currentChallenge = getNextCustomChallenge();
    }
    else
    {
        currentChallenge = processorRef.spacedRepetition.getNextChallenge (voicingId);
    }

    const auto* voicing = processorRef.voicingLibrary.getVoicing (voicingId);
    if (voicing == nullptr)
    {
        stopPractice();
        return;
    }

    // Show which voicing we're practicing
    headerLabel.setText ("Practicing: " + voicing->name, juce::dontSendNotification);

    targetNotes = VoicingLibrary::transposeToKey (*voicing, currentChallenge.rootMidiNote);

    bool timedMode = timedToggle.getToggleState();

    if (timedMode)
    {
        // Start timed practice: count-in
        timedPhase = TimedPhase::CountIn;
        lastBeatInSequence = -1;
        playPhaseScored = false;
        hasWrongAttempt = false;
        lastQualityScore = -1;

        // Reset beat counter so count-in is synced with metronome beat 1
        processorRef.tempoEngine.resetBeatPosition();
        processorRef.tempoEngine.markChallengeStart();

        // During count-in, show countdown
        targetLabel.setText ("Get ready...", juce::dontSendNotification);
        feedbackLabel.setText ("", juce::dontSendNotification);
        timingFeedbackLabel.setText ("", juce::dontSendNotification);
        currentRootText = "";
        nextRootText = "";
        currentRootColour = juce::Colours::white;

        keyboardRef.clearAllColours();
        keyboardRef.repaint();
    }
    else
    {
        // Untimed mode: show target immediately
        timedPhase = TimedPhase::Inactive;
        juce::String keyName = ChordDetector::noteNameFromPitchClass (currentChallenge.keyIndex);
        targetLabel.setText ("Play: " + keyName, juce::dontSendNotification);
        targetLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        feedbackLabel.setText ("", juce::dontSendNotification);
        timingFeedbackLabel.setText ("", juce::dontSendNotification);

        // Show root in grey on main display
        currentRootText = keyName;
        nextRootText = "";
        currentRootColour = juce::Colour (0xFF889999);

        // Show target notes on keyboard
        keyboardRef.clearAllColours();
        for (int note : targetNotes)
            keyboardRef.setKeyColour (note, KeyColour::Target);
        keyboardRef.repaint();
    }
}

void PracticePanel::stopPractice()
{
    practicing = false;
    challengeCompleted = false;
    timedPhase = TimedPhase::Inactive;
    targetNotes.clear();
    practicingVoicingId = {};

    headerLabel.setText ("PRACTICE", juce::dontSendNotification);
    targetLabel.setText ("Select a voicing and press Start", juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    feedbackLabel.setText ("", juce::dontSendNotification);
    currentRootText = {};
    nextRootText = {};
    currentRootColour = juce::Colours::white;
    timingFeedbackLabel.setText ("", juce::dontSendNotification);

    startButton.setButtonText ("Start");
    startButton.setColour (juce::TextButton::buttonColourId,
                            getLookAndFeel().findColour (juce::TextButton::buttonColourId));
    nextButton.setEnabled (false);
    playButton.setEnabled (false);

    keyboardRef.clearAllColours();
    keyboardRef.repaint();

    // Turn metronome off
    if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
        param->setValueNotifyingHost (0.0f);

    processorRef.tempoEngine.clearChallengeStart();
}

// --- Practice update (called from 60Hz timer) ---

void PracticePanel::updatePractice (const std::vector<int>& activeNotes)
{
    if (! practicing || targetNotes.empty())
        return;

    if (timedPhase != TimedPhase::Inactive)
        updateTimedPractice (activeNotes);
    else
        updateUntimedPractice (activeNotes);
}

// --- Untimed practice (original behavior) ---

void PracticePanel::updateUntimedPractice (const std::vector<int>& activeNotes)
{
    // Auto-advance after success — wait for note release then advance
    if (challengeCompleted)
    {
        if (activeNotes.empty())
            loadNextChallenge();
        return;
    }

    if (activeNotes.empty())
    {
        keyboardRef.clearAllColours();
        for (int note : targetNotes)
            keyboardRef.setKeyColour (note, KeyColour::Target);
        keyboardRef.repaint();
        return;
    }

    updateKeyboardColours (activeNotes);

    std::set<int> targetPitchClasses;
    for (int n : targetNotes)
        targetPitchClasses.insert (n % 12);

    std::set<int> playedPitchClasses;
    for (int n : activeNotes)
        playedPitchClasses.insert (n % 12);

    if (playedPitchClasses == targetPitchClasses)
    {
        challengeCompleted = true;
        processorRef.spacedRepetition.recordSuccess (
            currentChallenge.voicingId, currentChallenge.keyIndex);
        feedbackLabel.setText ("Correct!", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00CC44));

        // Flash green on main display
        currentRootColour = juce::Colour (0xFF00FF66);

        updateStats();
    }
}

// --- Timed practice (4-beat musical cycle) ---

void PracticePanel::updateTimedPractice (const std::vector<int>& activeNotes)
{
    double beatsElapsed = processorRef.tempoEngine.getBeatsSinceChallengeStart();
    int beatInSequence = static_cast<int> (std::floor (beatsElapsed));

    // Detect beat transitions
    bool beatChanged = (beatInSequence != lastBeatInSequence);
    lastBeatInSequence = beatInSequence;

    // --- Count-in phase (beats 0-3) ---
    if (beatInSequence < 4)
    {
        if (beatInSequence < 2)
        {
            // Beats 0-1: blank count-in
            int countdown = 4 - beatInSequence;
            targetLabel.setText (juce::String (countdown) + "...",
                                 juce::dontSendNotification);
        }
        else
        {
            // Beats 2-3: show first chord root (prep) — white, getting ready
            {
                juce::String keyName = ChordDetector::noteNameFromPitchClass (currentChallenge.keyIndex);
                targetLabel.setText ("Next: " + keyName, juce::dontSendNotification);
                targetLabel.setColour (juce::Label::textColourId, juce::Colours::white);

                // Show on main display as "up next"
                currentRootText = keyName;
                currentRootColour = juce::Colours::white;
                nextRootText = "";
            }

            // Show target notes on keyboard during prep
            keyboardRef.clearAllColours();
            for (int note : targetNotes)
                keyboardRef.setKeyColour (note, KeyColour::Target);
            keyboardRef.repaint();
        }
        return;
    }

    // --- After count-in: alternating Play/Prep cycles ---
    int beatAfterCountIn = beatInSequence - 4;
    int beatInCycle = beatAfterCountIn % 4;

    // Beats 0-1 of cycle = Play phase, Beats 2-3 = Prep phase
    bool inPlayPhase = (beatInCycle < 2);

    if (inPlayPhase)
    {
        // --- PLAY PHASE ---
        if (beatChanged && beatInCycle == 0)
            enterPlayPhase();

        if (playPhaseScored)
        {
            // Already scored — just keep showing feedback
            return;
        }

        // Check user's notes
        if (! activeNotes.empty())
        {
            updateKeyboardColours (activeNotes);

            std::set<int> targetPitchClasses;
            for (int n : targetNotes)
                targetPitchClasses.insert (n % 12);

            std::set<int> playedPitchClasses;
            for (int n : activeNotes)
                playedPitchClasses.insert (n % 12);

            // Only count as wrong if user played notes OUTSIDE the target set
            // (partial matches = building up the chord = fine)
            bool hasExtraNotes = false;
            for (int pc : playedPitchClasses)
                if (targetPitchClasses.count (pc) == 0)
                    hasExtraNotes = true;
            if (hasExtraNotes)
                hasWrongAttempt = true;

            if (playedPitchClasses == targetPitchClasses)
            {
                // Correct! Score based on how far into the play phase
                double beatFraction = beatsElapsed - static_cast<double> (beatInSequence - beatInCycle);
                int quality = computeQuality (beatFraction, hasWrongAttempt);
                processorRef.spacedRepetition.recordAttempt (
                    currentChallenge.voicingId, currentChallenge.keyIndex, quality);
                lastQualityScore = quality;
                playPhaseScored = true;

                juce::String qualityText;
                juce::Colour qualityColour;
                switch (quality)
                {
                    case 5:  qualityText = "Perfect!";  qualityColour = juce::Colour (0xFF00CC44); break;
                    case 4:  qualityText = "Good";      qualityColour = juce::Colour (0xFF44CC88); break;
                    case 3:  qualityText = "OK";        qualityColour = juce::Colour (0xFFCCCC00); break;
                    case 2:  qualityText = "Slow";      qualityColour = juce::Colour (0xFFCC8800); break;
                    case 1:  qualityText = "Corrected"; qualityColour = juce::Colour (0xFFCC4400); break;
                    default: qualityText = "Timeout";   qualityColour = juce::Colour (0xFFCC2200); break;
                }

                feedbackLabel.setText ("Correct!", juce::dontSendNotification);
                feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00CC44));
                timingFeedbackLabel.setText (
                    juce::String (beatFraction, 1) + " beats - " + qualityText,
                    juce::dontSendNotification);
                timingFeedbackLabel.setColour (juce::Label::textColourId, qualityColour);
                updateStats();
            }
        }
        else
        {
            // No notes pressed — show target
            keyboardRef.clearAllColours();
            for (int note : targetNotes)
                keyboardRef.setKeyColour (note, KeyColour::Target);
            keyboardRef.repaint();
        }
    }
    else
    {
        // --- PREP PHASE ---
        if (beatChanged && beatInCycle == 2)
            enterPrepPhase();

        // Accept early play during prep — if user already has the correct chord,
        // mark it so enterPlayPhase() can score it as quality 5
        if (! activeNotes.empty() && ! playPhaseScored)
        {
            std::set<int> targetPC, playedPC;
            for (int n : targetNotes) targetPC.insert (n % 12);
            for (int n : activeNotes) playedPC.insert (n % 12);
            if (playedPC == targetPC)
            {
                // Credit as perfect — they anticipated the change
                processorRef.spacedRepetition.recordAttempt (
                    currentChallenge.voicingId, currentChallenge.keyIndex, 5);
                lastQualityScore = 5;
                playPhaseScored = true;

                feedbackLabel.setText ("Correct!", juce::dontSendNotification);
                feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00CC44));
                timingFeedbackLabel.setText ("Early - Perfect!", juce::dontSendNotification);
                timingFeedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00CC44));
                updateStats();

                updateKeyboardColours (activeNotes);
            }
        }
    }
}

void PracticePanel::enterPlayPhase()
{
    timedPhase = TimedPhase::Play;
    playPhaseScored = false;
    hasWrongAttempt = false;

    // Show root in urgent color
    juce::String keyName = ChordDetector::noteNameFromPitchClass (currentChallenge.keyIndex);
    targetLabel.setText (keyName, juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00FF66));

    currentRootText = keyName;
    currentRootColour = juce::Colour (0xFF00FF66);

    // Pre-fetch next challenge so we can show "Up next..." immediately
    if (customMode)
        nextChallenge = getNextCustomChallenge();
    else
        nextChallenge = processorRef.spacedRepetition.getNextChallenge (practicingVoicingId);

    nextRootText = ChordDetector::noteNameFromPitchClass (nextChallenge.keyIndex);

    feedbackLabel.setText ("GO!", juce::dontSendNotification);
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00FF66));
    timingFeedbackLabel.setText ("", juce::dontSendNotification);

    // Show target notes
    keyboardRef.clearAllColours();
    for (int note : targetNotes)
        keyboardRef.setKeyColour (note, KeyColour::Target);
    keyboardRef.repaint();
}

void PracticePanel::enterPrepPhase()
{
    timedPhase = TimedPhase::Prep;

    // Score the play phase if user didn't get it right
    if (! playPhaseScored)
    {
        processorRef.spacedRepetition.recordAttempt (
            currentChallenge.voicingId, currentChallenge.keyIndex, 0);
        lastQualityScore = 0;
        feedbackLabel.setText ("Timeout!", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCC2200));
        timingFeedbackLabel.setText ("Q0 - Too slow", juce::dontSendNotification);
        timingFeedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCC2200));
        updateStats();
    }

    // Use the next challenge that was pre-fetched in enterPlayPhase()
    const auto* voicing = processorRef.voicingLibrary.getVoicing (nextChallenge.voicingId);
    if (voicing == nullptr)
    {
        stopPractice();
        return;
    }

    // Load next challenge's target notes (for the upcoming play phase)
    currentChallenge = nextChallenge;
    targetNotes = VoicingLibrary::transposeToKey (*voicing, currentChallenge.rootMidiNote);

    // Show upcoming root dimmed as the main display (prep = getting ready)
    juce::String keyName = ChordDetector::noteNameFromPitchClass (currentChallenge.keyIndex);
    targetLabel.setText ("Up next: " + keyName, juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF889999));

    // Main display: upcoming root dimmed, no "up next" subtitle (it IS the main one now)
    currentRootText = keyName;
    currentRootColour = juce::Colour (0xFF889999);
    nextRootText = "";

    // Clear keyboard
    keyboardRef.clearAllColours();
    keyboardRef.repaint();
}

// --- Shared helpers ---

void PracticePanel::loadNextChallenge()
{
    challengeCompleted = false;

    juce::String vid = practicingVoicingId.isNotEmpty() ? practicingVoicingId
                                                        : currentChallenge.voicingId;

    // Get next challenge without rebuilding custom sequence
    if (customMode)
        currentChallenge = getNextCustomChallenge();
    else
        currentChallenge = processorRef.spacedRepetition.getNextChallenge (vid);

    const auto* voicing = processorRef.voicingLibrary.getVoicing (vid);
    if (voicing == nullptr)
    {
        stopPractice();
        return;
    }

    targetNotes = VoicingLibrary::transposeToKey (*voicing, currentChallenge.rootMidiNote);

    bool timedMode = timedToggle.getToggleState();

    if (timedMode)
    {
        // In timed mode, loadNextChallenge is only used from untimed path fallback
        // Timed mode uses enterPrepPhase/enterPlayPhase cycle instead
        timedPhase = TimedPhase::Play;
        playPhaseScored = false;
        hasWrongAttempt = false;
        enterPlayPhase();
    }
    else
    {
        juce::String keyName = ChordDetector::noteNameFromPitchClass (currentChallenge.keyIndex);
        targetLabel.setText ("Play: " + keyName, juce::dontSendNotification);
        targetLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        feedbackLabel.setText ("", juce::dontSendNotification);

        // Show root in grey on main display
        currentRootText = keyName;
        nextRootText = "";
        currentRootColour = juce::Colour (0xFF889999);

        keyboardRef.clearAllColours();
        for (int note : targetNotes)
            keyboardRef.setKeyColour (note, KeyColour::Target);
        keyboardRef.repaint();
    }
}

int PracticePanel::computeQuality (double beatsElapsed, bool hadWrongAttempt)
{
    if (hadWrongAttempt) return 1;
    if (beatsElapsed < 0.5) return 5;
    if (beatsElapsed < 1.0) return 4;
    if (beatsElapsed < 1.5) return 3;
    return 2;
}

void PracticePanel::updateKeyboardColours (const std::vector<int>& activeNotes)
{
    keyboardRef.clearAllColours();

    std::set<int> targetPitchClasses;
    for (int n : targetNotes)
        targetPitchClasses.insert (n % 12);

    std::set<int> playedPitchClasses;
    for (int n : activeNotes)
        playedPitchClasses.insert (n % 12);

    for (int note : activeNotes)
    {
        int pc = note % 12;
        if (targetPitchClasses.count (pc) > 0)
            keyboardRef.setKeyColour (note, KeyColour::Correct);
        else
            keyboardRef.setKeyColour (note, KeyColour::Wrong);
    }

    for (int note : targetNotes)
    {
        int pc = note % 12;
        if (playedPitchClasses.count (pc) == 0)
            keyboardRef.setKeyColour (note, KeyColour::Target);
    }

    keyboardRef.repaint();
}

void PracticePanel::updateStats()
{
    int attempts = processorRef.spacedRepetition.getTotalAttempts();
    int successes = processorRef.spacedRepetition.getTotalSuccesses();
    double accuracy = processorRef.spacedRepetition.getAccuracy();

    juce::String statsText = "Accuracy: " + juce::String (juce::roundToInt (accuracy * 100))
                             + "% (" + juce::String (successes) + "/" + juce::String (attempts) + ")";
    statsLabel.setText (statsText, juce::dontSendNotification);
}

void PracticePanel::onNext()
{
    if (! practicing)
        return;

    if (timedPhase != TimedPhase::Inactive)
    {
        // In timed mode, skip to next — score current as failure if not scored
        if (! playPhaseScored && timedPhase == TimedPhase::Play)
        {
            processorRef.spacedRepetition.recordAttempt (
                currentChallenge.voicingId, currentChallenge.keyIndex, 0);
            updateStats();
        }
        // Force into prep for next
        enterPrepPhase();
    }
    else
    {
        // Untimed mode
        if (! challengeCompleted)
        {
            processorRef.spacedRepetition.recordFailure (
                currentChallenge.voicingId, currentChallenge.keyIndex);
            feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCC2200));
            updateStats();
        }
        loadNextChallenge();
    }
}

void PracticePanel::onCustomToggle()
{
    if (practicing)
        return;

    showingKeySelector = ! showingKeySelector;

    for (int i = 0; i < 12; ++i)
        keyToggles[i].setVisible (showingKeySelector);
    selectAllButton.setVisible (showingKeySelector);
    deselectAllButton.setVisible (showingKeySelector);
    orderCombo.setVisible (showingKeySelector);

    customMode = showingKeySelector;
    customButton.setColour (juce::TextButton::buttonColourId,
        showingKeySelector ? juce::Colour (0xFF446688)
                           : getLookAndFeel().findColour (juce::TextButton::buttonColourId));

    resized();
    repaint();
}

void PracticePanel::buildCustomKeySequence()
{
    customAllowedKeys.clear();
    for (int i = 0; i < 12; ++i)
        if (keyToggles[i].getToggleState())
            customAllowedKeys.insert (i);

    if (customAllowedKeys.empty())
    {
        // Nothing selected — use all
        for (int i = 0; i < 12; ++i)
            customAllowedKeys.insert (i);
    }

    rootOrder = (orderCombo.getSelectedId() == 2) ? RootOrder::Chromatic : RootOrder::Random;

    customKeySequence.clear();
    customKeySequence.assign (customAllowedKeys.begin(), customAllowedKeys.end());

    if (rootOrder == RootOrder::Random)
        std::shuffle (customKeySequence.begin(), customKeySequence.end(), rng);

    customKeyIndex = 0;
}

PracticeChallenge PracticePanel::getNextCustomChallenge()
{
    if (customKeySequence.empty())
        buildCustomKeySequence();

    if (customKeyIndex >= static_cast<int> (customKeySequence.size()))
    {
        // Cycle: rebuild for next round
        if (rootOrder == RootOrder::Random)
            std::shuffle (customKeySequence.begin(), customKeySequence.end(), rng);
        customKeyIndex = 0;
    }

    int key = customKeySequence[static_cast<size_t> (customKeyIndex++)];

    PracticeChallenge challenge;
    challenge.voicingId = practicingVoicingId;
    challenge.keyIndex = key;
    challenge.rootMidiNote = 48 + key;
    return challenge;
}

void PracticePanel::onPlay()
{
    if (targetNotes.empty())
        return;

    int channel = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    for (int note : targetNotes)
        processorRef.keyboardState.noteOn (channel, note, 0.8f);

    juce::Timer::callAfterDelay (800, [this, channel]
    {
        for (int note : targetNotes)
            processorRef.keyboardState.noteOff (channel, note, 0.0f);
    });
}
