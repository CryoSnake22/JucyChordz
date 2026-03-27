#include "PracticePanel.h"
#include "PluginProcessor.h"
#include "ChordyKeyboardComponent.h"
#include "ChordyTheme.h"
#include <algorithm>
#include <set>

PracticePanel::PracticePanel (AudioPluginAudioProcessor& processor,
                               ChordyKeyboardComponent& keyboard)
    : processorRef (processor), keyboardRef (keyboard)
{
    headerLabel.setText ("PRACTICE", juce::dontSendNotification);
    headerLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    headerLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    addAndMakeVisible (headerLabel);

    targetLabel.setText ("Select a voicing and press Start", juce::dontSendNotification);
    targetLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    targetLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (targetLabel);

    feedbackLabel.setFont (juce::FontOptions (14.0f));
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textSecondary));
    feedbackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (feedbackLabel);

    statsLabel.setFont (juce::FontOptions (13.0f));
    statsLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textTertiary));
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

    // Toggle colors inherited from LookAndFeel
    addAndMakeVisible (timedToggle);

    // Key selector toggles (hidden by default)
    static const char* noteNames[] = { "C", "Db", "D", "Eb", "E", "F",
                                        "Gb", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
    {
        keyToggles[i].setButtonText (noteNames[i]);
        keyToggles[i].setToggleState (true, juce::dontSendNotification);
        // Toggle colors inherited from LookAndFeel
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
    timingFeedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textTertiary));
    timingFeedbackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timingFeedbackLabel);

    practiceChartViewport.setViewedComponent (&practiceChart, false);
    practiceChartViewport.setScrollBarsShown (true, false);
    practiceChartViewport.setVisible (false);
    addAndMakeVisible (practiceChartViewport);

    progDetailedToggle.setToggleState (true, juce::dontSendNotification);
    progDetailedToggle.onClick = [this] {
        practiceChart.setDetailedView (progDetailedToggle.getToggleState());
        resized();
    };
    addAndMakeVisible (progDetailedToggle);

    practiceMLChart.setVisible (false);
    addAndMakeVisible (practiceMLChart);

    backingToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (backingToggle);

    updateStats();
}

void PracticePanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (ChordyTheme::bgSurface));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), ChordyTheme::cornerRadius);
}

void PracticePanel::resized()
{
    auto area = getLocalBounds().reduced (ChordyTheme::panelPadding);

    // Layout bottom-up: fixed controls at bottom, chart fills remaining top
    statsLabel.setBounds (area.removeFromBottom (24));
    area.removeFromBottom (2);
    timingFeedbackLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (2);
    feedbackLabel.setBounds (area.removeFromBottom (24));
    area.removeFromBottom (4);

    // Key selector (conditionally visible)
    if (showingKeySelector)
    {
        auto controlRow = area.removeFromBottom (24);
        selectAllButton.setBounds (controlRow.removeFromLeft (40));
        controlRow.removeFromLeft (4);
        deselectAllButton.setBounds (controlRow.removeFromLeft (45));
        controlRow.removeFromLeft (8);
        orderCombo.setBounds (controlRow);
        area.removeFromBottom (2);

        auto keyRow2 = area.removeFromBottom (22);
        int kw = keyRow2.getWidth() / 6;
        for (int i = 6; i < 12; ++i)
            keyToggles[i].setBounds (keyRow2.removeFromLeft (kw));
        area.removeFromBottom (2);

        auto keyRow1 = area.removeFromBottom (22);
        for (int i = 0; i < 6; ++i)
            keyToggles[i].setBounds (keyRow1.removeFromLeft (kw));
        area.removeFromBottom (4);
    }

    // Toggle row
    auto toggleRow = area.removeFromBottom (24);
    timedToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 3));
    if (practiceType == PracticeType::Progression)
    {
        progDetailedToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2));
        progDetailedToggle.setVisible (true);
        backingToggle.setVisible (false);
    }
    else
    {
        progDetailedToggle.setVisible (false);
        backingToggle.setVisible (true);
    }
    backingToggle.setBounds (toggleRow);
    area.removeFromBottom (4);

    // Button row
    auto buttonRow = area.removeFromBottom (30);
    int buttonWidth = (buttonRow.getWidth() - 12) / 4;
    startButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    nextButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    playButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    customButton.setBounds (buttonRow);
    area.removeFromBottom (4);

    // Target label
    targetLabel.setBounds (area.removeFromBottom (24));
    area.removeFromBottom (4);

    // Header
    headerLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);

    // Chart fills ALL remaining space — always visible
    bool showProgChart = (practicing && practiceType == PracticeType::Progression) || showingProgPreview;
    bool showMelChart = (practicing && practiceType == PracticeType::Melody) || showingMelPreview;

    if (showProgChart)
    {
        practiceChartViewport.setBounds (area);
        // Tell chart how tall one row should be (fills viewport height)
        practiceChart.setViewportHeight (area.getHeight());
        int idealH = juce::jmax (1, practiceChart.getIdealHeight());
        int scrollbarW = (idealH > area.getHeight()) ? 10 : 0;
        practiceChart.setBounds (0, 0, area.getWidth() - scrollbarW, idealH);
        practiceChartViewport.setVisible (true);
        practiceMLChart.setBounds (0, 0, 0, 0);
    }
    else if (showMelChart)
    {
        practiceMLChart.setBounds (area);
        practiceMLChart.setVisible (true);
        practiceChartViewport.setBounds (0, 0, 0, 0);
        practiceChartViewport.setVisible (false);
    }
    else
    {
        practiceChartViewport.setBounds (0, 0, 0, 0);
        practiceChartViewport.setVisible (false);
        practiceMLChart.setBounds (0, 0, 0, 0);
    }
}

// --- Selection ---

void PracticePanel::setSelectedVoicingId (const juce::String& id)
{
    selectedVoicingId = id;
    selectedProgressionId = {};
    practiceType = PracticeType::Voicing;

    if (! practicing)
    {
        const auto* v = processorRef.voicingLibrary.getVoicing (id);
        if (v != nullptr)
            headerLabel.setText ("Practice: " + v->name, juce::dontSendNotification);
        else
            headerLabel.setText ("PRACTICE -Voicings", juce::dontSendNotification);

        targetLabel.setText ("Select a voicing and press Start", juce::dontSendNotification);
    }
}

void PracticePanel::setSelectedProgressionId (const juce::String& id)
{
    selectedProgressionId = id;
    selectedVoicingId = {};
    practiceType = PracticeType::Progression;

    if (! practicing)
    {
        const auto* p = processorRef.progressionLibrary.getProgression (id);
        if (p != nullptr)
            headerLabel.setText ("Practice: " + p->name, juce::dontSendNotification);
        else
            headerLabel.setText ("PRACTICE -Progressions", juce::dontSendNotification);

        targetLabel.setText ("Select a progression and press Start", juce::dontSendNotification);
    }
}

void PracticePanel::setSelectedMelodyId (const juce::String& id)
{
    selectedMelodyId = id;
    selectedVoicingId = {};
    selectedProgressionId = {};
    practiceType = PracticeType::Melody;

    if (! practicing)
    {
        const auto* m = processorRef.melodyLibrary.getMelody (id);
        if (m != nullptr)
            headerLabel.setText ("Practice: " + m->name, juce::dontSendNotification);
        else
            headerLabel.setText ("PRACTICE -Melodies", juce::dontSendNotification);

        targetLabel.setText ("Select a melody and press Start", juce::dontSendNotification);
    }
}

// --- Start / Stop ---

void PracticePanel::onStartStop()
{
    if (practicing)
    {
        stopPractice();
        return;
    }

    // Block start if nothing is selected for the current practice type
    if (practiceType == PracticeType::Melody && selectedMelodyId.isEmpty())
    {
        targetLabel.setText ("Select a melody first!", juce::dontSendNotification);
        return;
    }
    if (practiceType == PracticeType::Progression && selectedProgressionId.isEmpty())
    {
        targetLabel.setText ("Select a progression first!", juce::dontSendNotification);
        return;
    }
    if (practiceType == PracticeType::Voicing && selectedVoicingId.isEmpty()
        && processorRef.voicingLibrary.size() == 0)
    {
        targetLabel.setText ("Record a voicing first!", juce::dontSendNotification);
        return;
    }

    // Check if we should practice a melody, progression, or voicing
    if (practiceType == PracticeType::Melody && selectedMelodyId.isNotEmpty())
    {
        const auto* mel = processorRef.melodyLibrary.getMelody (selectedMelodyId);
        if (mel == nullptr || mel->notes.empty())
        {
            targetLabel.setText ("Select a valid melody!", juce::dontSendNotification);
            return;
        }

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

        startMelodyPractice (selectedMelodyId);
        return;
    }

    if (practiceType == PracticeType::Progression && selectedProgressionId.isNotEmpty())
    {
        const auto* prog = processorRef.progressionLibrary.getProgression (selectedProgressionId);
        if (prog == nullptr || prog->chords.empty())
        {
            targetLabel.setText ("Select a valid progression!", juce::dontSendNotification);
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

        startProgressionPractice (selectedProgressionId);
        return;
    }

    // Voicing practice
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

    // Turn metronome on only for timed mode
    if (timedToggle.getToggleState())
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
    startButton.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
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
        currentRootColour = juce::Colour (ChordyTheme::textPrimary);

        keyboardRef.clearAllColours();
        keyboardRef.repaint();
    }
    else
    {
        // Untimed mode: show target immediately
        timedPhase = TimedPhase::Inactive;
        juce::String keyName = ChordDetector::noteNameFromPitchClass (currentChallenge.keyIndex);
        targetLabel.setText ("Play: " + keyName, juce::dontSendNotification);
        targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
        feedbackLabel.setText ("", juce::dontSendNotification);
        timingFeedbackLabel.setText ("", juce::dontSendNotification);

        // Show root in grey on main display
        currentRootText = keyName;
        nextRootText = "";
        currentRootColour = juce::Colour (ChordyTheme::textTertiary);

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
    practicingProgressionId = {};
    practicingMelodyId = {};
    progressionChordIndex = 0;
    practiceChartViewport.setVisible (false);
    practiceChart.setProgressionReadOnly (nullptr);
    practiceMLChart.setVisible (false);
    practiceMLChart.setMelodyReadOnly (nullptr);
    stopMelodyBacking();
    processorRef.stopMelodyPlayback();
    previousFramePitchClasses.clear();

    headerLabel.setText ("PRACTICE", juce::dontSendNotification);
    targetLabel.setText ("Select a voicing, progression, or melody and press Start", juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    feedbackLabel.setText ("", juce::dontSendNotification);
    currentRootText = {};
    nextRootText = {};
    currentRootColour = juce::Colour (ChordyTheme::textPrimary);
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

    // Persist SR data changes from this practice session
    processorRef.saveLibrariesToDisk();
}

// --- Chart preview (non-practice) ---

void PracticePanel::showProgressionPreview (const Progression* prog)
{
    showingMelPreview = false;
    if (prog == nullptr)
    {
        showingProgPreview = false;
        practiceChart.setProgressionReadOnly (nullptr);
    }
    else
    {
        previewProgression = *prog;
        showingProgPreview = true;
        practiceChart.setProgressionReadOnly (&previewProgression);
        practiceChart.clearNoteStates();
    }
    resized();
    repaint();
}

void PracticePanel::showMelodyPreview (const Melody* mel)
{
    showingProgPreview = false;
    if (mel == nullptr)
    {
        showingMelPreview = false;
        practiceMLChart.setMelodyReadOnly (nullptr);
    }
    else
    {
        previewMelody = *mel;
        showingMelPreview = true;
        practiceMLChart.setMelodyReadOnly (&previewMelody);
    }
    resized();
    repaint();
}

void PracticePanel::clearChartPreview()
{
    showingProgPreview = false;
    showingMelPreview = false;
    practiceChart.setProgressionReadOnly (nullptr);
    practiceMLChart.setMelodyReadOnly (nullptr);
    resized();
    repaint();
}

void PracticePanel::showProgressionCursor (double beat)
{
    if (showingProgPreview)
        practiceChart.setCursorBeat (beat);
}

void PracticePanel::showMelodyCursor (double beat)
{
    if (showingMelPreview)
        practiceMLChart.setCursorBeat (beat);
}

// --- Practice update (called from 60Hz timer) ---

void PracticePanel::updatePractice (const std::vector<int>& activeNotes)
{
    if (! practicing)
        return;

    if (practiceType == PracticeType::Melody)
    {
        updateMelodyPractice (activeNotes);
        return;
    }

    if (targetNotes.empty())
        return;

    if (practiceType == PracticeType::Progression)
    {
        updateProgressionPractice (activeNotes);
        return;
    }

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
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));

        // Flash green on main display
        currentRootColour = juce::Colour (ChordyTheme::successBright);

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
                targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));

                // Show on main display as "up next"
                currentRootText = keyName;
                currentRootColour = juce::Colour (ChordyTheme::textPrimary);
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
                    case 5:  qualityText = "Perfect!";  qualityColour = juce::Colour (ChordyTheme::qualityPerfect); break;
                    case 4:  qualityText = "Good";      qualityColour = juce::Colour (ChordyTheme::qualityGood); break;
                    case 3:  qualityText = "OK";        qualityColour = juce::Colour (ChordyTheme::qualityOk); break;
                    case 2:  qualityText = "Slow";      qualityColour = juce::Colour (ChordyTheme::qualitySlow); break;
                    case 1:  qualityText = "Corrected"; qualityColour = juce::Colour (ChordyTheme::qualityCorrected); break;
                    default: qualityText = "Timeout";   qualityColour = juce::Colour (ChordyTheme::qualityTimeout); break;
                }

                feedbackLabel.setText ("Correct!", juce::dontSendNotification);
                feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));
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
                feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));
                timingFeedbackLabel.setText ("Early - Perfect!", juce::dontSendNotification);
                timingFeedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));
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
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::successBright));

    currentRootText = keyName;
    currentRootColour = juce::Colour (ChordyTheme::successBright);

    // Pre-fetch next challenge so we can show "Up next..." immediately
    if (customMode)
        nextChallenge = getNextCustomChallenge();
    else
        nextChallenge = processorRef.spacedRepetition.getNextChallenge (practicingVoicingId, currentChallenge.keyIndex);

    nextRootText = ChordDetector::noteNameFromPitchClass (nextChallenge.keyIndex);

    feedbackLabel.setText ("GO!", juce::dontSendNotification);
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::successBright));
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
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
        timingFeedbackLabel.setText ("Q0 - Too slow", juce::dontSendNotification);
        timingFeedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
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
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textTertiary));

    // Main display: upcoming root dimmed, no "up next" subtitle (it IS the main one now)
    currentRootText = keyName;
    currentRootColour = juce::Colour (ChordyTheme::textTertiary);
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
        currentChallenge = processorRef.spacedRepetition.getNextChallenge (vid, currentChallenge.keyIndex);

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
        targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
        feedbackLabel.setText ("", juce::dontSendNotification);

        // Show root in grey on main display
        currentRootText = keyName;
        nextRootText = "";
        currentRootColour = juce::Colour (ChordyTheme::textTertiary);

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

    // --- Melody practice ---
    if (practiceType == PracticeType::Melody)
    {
        if (melodyNoteIndex < static_cast<int> (transposedMelody.notes.size()))
        {
            practiceMLChart.setNoteState (melodyNoteIndex, MelodyChartComponent::NoteState::Missed);
            melodyNoteIndex++;
            feedbackLabel.setText ("Skipped", juce::dontSendNotification);
            feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));

            if (melodyNoteIndex >= static_cast<int> (transposedMelody.notes.size()))
                advanceMelodyNote(); // triggers key completion
            else
            {
                practiceMLChart.setNoteState (melodyNoteIndex, MelodyChartComponent::NoteState::Target);
                practiceMLChart.setHighlightedNoteIndex (melodyNoteIndex);

                int pc = (melodyKeyRootMidi + transposedMelody.notes[static_cast<size_t> (melodyNoteIndex)].intervalFromKeyRoot) % 12;
                if (pc < 0) pc += 12;
                juce::String noteName = ChordDetector::noteNameFromPitchClass (pc);
                targetLabel.setText ("Play: " + noteName, juce::dontSendNotification);

                updateMelodyBacking();
            }
        }
        return;
    }

    // --- Progression practice ---
    if (practiceType == PracticeType::Progression)
    {
        feedbackLabel.setText ("Skipped", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));

        if (timedPhase != TimedPhase::Inactive)
        {
            // Timed: mark current chord as missed (Q0)
            if (progressionChordIndex >= 0
                && progressionTimedScored.count (progressionChordIndex) == 0)
            {
                progressionTimedScored.insert (progressionChordIndex);
                progressionQualitySum += 0;
                practiceChart.setAllChordNoteStates (progressionChordIndex, ProgressionChartComponent::NoteState::Missed);
            }
            // Don't advance manually — cursor will move with the beat
        }
        else
        {
            // Untimed: skip to next chord, record as miss (don't count as correct)
            practiceChart.setAllChordNoteStates (progressionChordIndex, ProgressionChartComponent::NoteState::Missed);
            challengeCompleted = false;
            advanceProgressionChord();
        }
        return;
    }

    // --- Voicing practice ---
    if (timedPhase != TimedPhase::Inactive)
    {
        if (! playPhaseScored && timedPhase == TimedPhase::Play)
        {
            processorRef.spacedRepetition.recordAttempt (
                currentChallenge.voicingId, currentChallenge.keyIndex, 0);
            updateStats();
        }
        enterPrepPhase();
    }
    else
    {
        if (! challengeCompleted)
        {
            processorRef.spacedRepetition.recordFailure (
                currentChallenge.voicingId, currentChallenge.keyIndex);
            feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
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
        showingKeySelector ? juce::Colour (ChordyTheme::accentMuted)
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
        // Remember the last key to avoid repeats at the boundary
        int lastKey = customKeySequence.back();

        // Cycle: rebuild for next round
        if (rootOrder == RootOrder::Random)
        {
            std::shuffle (customKeySequence.begin(), customKeySequence.end(), rng);
            // Avoid repeating the last key at the boundary
            if (customKeySequence.size() > 1 && customKeySequence.front() == lastKey)
                std::swap (customKeySequence[0], customKeySequence[1]);
        }
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
    // --- Melody practice: play the whole melody ---
    if (practiceType == PracticeType::Melody && transposedMelody.isValid())
    {
        if (processorRef.isPlayingMelody())
            processorRef.stopMelodyPlayback();
        else
            processorRef.startMelodyPlayback (transposedMelody, melodyKeyRootMidi);
        return;
    }

    if (targetNotes.empty())
        return;

    // Just play the sound and show on keyboard — no scoring, no advancing
    int channel = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    for (int note : targetNotes)
        processorRef.addPreviewMidi (juce::MidiMessage::noteOn (channel, note, 0.7f));

    auto notesToRelease = targetNotes;
    juce::Timer::callAfterDelay (600, [this, channel, notesToRelease]
    {
        for (int note : notesToRelease)
            processorRef.addPreviewMidi (juce::MidiMessage::noteOff (channel, note, 0.0f));
    });

    keyboardRef.clearAllColours();
    for (int note : targetNotes)
        keyboardRef.setKeyColour (note, KeyColour::Correct);
    keyboardRef.repaint();
}

//==============================================================================
// Progression practice
//==============================================================================

void PracticePanel::startProgressionPractice (const juce::String& progressionId)
{
    const auto* prog = processorRef.progressionLibrary.getProgression (progressionId);
    if (prog == nullptr || prog->chords.empty())
    {
        targetLabel.setText ("Invalid progression!", juce::dontSendNotification);
        return;
    }

    practicing = true;
    practiceType = PracticeType::Progression;
    practicingProgressionId = progressionId;
    challengeCompleted = false;
    progressionTimedBeat = 0.0;
    progressionTimedScored.clear();

    // Turn on metronome for timed mode
    if (timedToggle.getToggleState())
    {
        if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
            param->setValueNotifyingHost (1.0f);
        processorRef.tempoEngine.resetBeatPosition();
        processorRef.tempoEngine.markChallengeStart();
        timedPhase = TimedPhase::CountIn;
        lastBeatInSequence = -1;
    }
    else
    {
        timedPhase = TimedPhase::Inactive;
    }

    startButton.setButtonText ("Stop");
    startButton.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    nextButton.setEnabled (true);
    playButton.setEnabled (true);

    headerLabel.setText ("Practicing: " + prog->name, juce::dontSendNotification);

    // Get the first key to practice
    if (customMode)
    {
        buildCustomKeySequence();
        auto challenge = getNextCustomChallenge();
        loadProgressionChallenge (challenge.keyIndex);
    }
    else
    {
        // Use SR engine with progression ID to pick most overdue key
        auto challenge = processorRef.spacedRepetition.getNextChallenge (progressionId);
        loadProgressionChallenge (challenge.keyIndex);
    }
}

void PracticePanel::loadProgressionChallenge (int keyIndex)
{
    const auto* prog = processorRef.progressionLibrary.getProgression (practicingProgressionId);
    if (prog == nullptr)
    {
        stopPractice();
        return;
    }

    // Transpose progression to the target key
    int offset = (keyIndex - prog->keyPitchClass + 12) % 12;
    progressionKeyOffset = offset;
    transposedProgression = ProgressionLibrary::transposeProgression (*prog, offset);
    progressionChordIndex = 0;
    progressionChordsCorrect = 0;
    progressionChordsTotal = 0;
    progressionQualitySum = 0;

    // Set up chart
    practiceChartViewport.setVisible (true);
    practiceChart.setProgressionReadOnly (&transposedProgression);
    practiceChart.clearNoteStates();
    practiceChart.setSelectedChord (0);
    practiceChart.setAllChordNoteStates (0, ProgressionChartComponent::NoteState::Target);
    progressionHasWrongAttempt = false;

    // Load first chord's target notes
    const auto& chord = transposedProgression.chords[0];
    targetNotes = chord.midiNotes;

    juce::String keyName = ChordDetector::noteNameFromPitchClass (keyIndex);
    targetLabel.setText ("Key: " + keyName + " - " + chord.getDisplayName(),
                         juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    feedbackLabel.setText ("", juce::dontSendNotification);
    timingFeedbackLabel.setText ("", juce::dontSendNotification);

    currentRootText = keyName;
    currentRootColour = juce::Colour (ChordyTheme::textTertiary);
    nextRootText = "";

    // Show target on keyboard
    keyboardRef.clearAllColours();
    for (int note : targetNotes)
        keyboardRef.setKeyColour (note, KeyColour::Target);
    keyboardRef.repaint();

    resized();
    repaint();
}

void PracticePanel::advanceProgressionChord()
{
    progressionChordIndex++;

    if (progressionChordIndex >= static_cast<int> (transposedProgression.chords.size()))
    {
        // Completed the whole progression in this key
        progressionChordsTotal = static_cast<int> (transposedProgression.chords.size());

        // Compute quality from ratio
        int quality = 0;
        if (progressionChordsTotal > 0)
        {
            double ratio = static_cast<double> (progressionChordsCorrect) / progressionChordsTotal;
            if (ratio >= 0.95) quality = 5;
            else if (ratio >= 0.8) quality = 4;
            else if (ratio >= 0.6) quality = 3;
            else if (ratio >= 0.4) quality = 2;
            else if (ratio > 0.0) quality = 1;
        }

        int keyIndex = (processorRef.progressionLibrary.getProgression (practicingProgressionId)->keyPitchClass
                        + progressionKeyOffset) % 12;
        processorRef.spacedRepetition.recordAttempt (practicingProgressionId, keyIndex, quality);
        updateStats();

        juce::String scoreText = juce::String (progressionChordsCorrect) + "/"
                                 + juce::String (progressionChordsTotal);
        feedbackLabel.setText ("Complete! " + scoreText + " correct", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId,
            quality >= 3 ? juce::Colour (ChordyTheme::success) : juce::Colour (ChordyTheme::danger));

        // Get next key
        int nextKey;
        if (customMode)
        {
            auto challenge = getNextCustomChallenge();
            nextKey = challenge.keyIndex;
        }
        else
        {
            auto challenge = processorRef.spacedRepetition.getNextChallenge (practicingProgressionId, transposedProgression.keyPitchClass);
            nextKey = challenge.keyIndex;
        }

        loadProgressionChallenge (nextKey);
        return;
    }

    // Advance to next chord in the progression
    const auto& chord = transposedProgression.chords[static_cast<size_t> (progressionChordIndex)];
    targetNotes = chord.midiNotes;

    practiceChart.setSelectedChord (progressionChordIndex);
    practiceChart.setAllChordNoteStates (progressionChordIndex, ProgressionChartComponent::NoteState::Target);

    int keyIndex = (processorRef.progressionLibrary.getProgression (practicingProgressionId)->keyPitchClass
                    + progressionKeyOffset) % 12;
    juce::String keyName = ChordDetector::noteNameFromPitchClass (keyIndex);
    targetLabel.setText ("Key: " + keyName + " - " + chord.getDisplayName(),
                         juce::dontSendNotification);
    feedbackLabel.setText ("", juce::dontSendNotification);

    keyboardRef.clearAllColours();
    for (int note : targetNotes)
        keyboardRef.setKeyColour (note, KeyColour::Target);
    keyboardRef.repaint();
}

void PracticePanel::updateProgressionPractice (const std::vector<int>& activeNotes)
{
    bool timedMode = (timedPhase != TimedPhase::Inactive);

    if (timedMode)
    {
        double beatsElapsed = processorRef.tempoEngine.getBeatsSinceChallengeStart();
        int beatInt = static_cast<int> (std::floor (beatsElapsed));

        // --- Count-in (4 beats) ---
        if (beatInt < 4)
        {
            if (beatInt != lastBeatInSequence)
            {
                lastBeatInSequence = beatInt;
                int countdown = 4 - beatInt;
                targetLabel.setText (juce::String (countdown) + "...", juce::dontSendNotification);
                feedbackLabel.setText ("", juce::dontSendNotification);

                // Show first chord on beats 2-3
                if (beatInt >= 2 && ! transposedProgression.chords.empty())
                {
                    const auto& firstChord = transposedProgression.chords[0];
                    int keyIdx = (processorRef.progressionLibrary.getProgression (practicingProgressionId)->keyPitchClass
                                  + progressionKeyOffset) % 12;
                    currentRootText = ChordDetector::noteNameFromPitchClass (keyIdx);
                    currentRootColour = juce::Colour (ChordyTheme::textPrimary);

                    keyboardRef.clearAllColours();
                    for (int note : firstChord.midiNotes)
                        keyboardRef.setKeyColour (note, KeyColour::Target);
                    keyboardRef.repaint();
                }
            }
            return;
        }

        // --- After count-in: cursor follows beats through the progression ---
        double progressBeat = beatsElapsed - 4.0;
        progressionTimedBeat = progressBeat;
        practiceChart.setCursorBeat (progressBeat);

        // Find which chord should be active at this beat
        int targetChordIdx = -1;
        for (int ci = 0; ci < static_cast<int> (transposedProgression.chords.size()); ++ci)
        {
            const auto& c = transposedProgression.chords[static_cast<size_t> (ci)];
            if (progressBeat >= c.startBeat && progressBeat < c.startBeat + c.durationBeats)
            {
                targetChordIdx = ci;
                break;
            }
        }

        // Progression ended — score any remaining unscored chords as misses
        if (progressBeat >= transposedProgression.totalBeats)
        {
            progressionChordsTotal = static_cast<int> (transposedProgression.chords.size());

            // Score any remaining unscored chords as Q0
            for (int ci = 0; ci < progressionChordsTotal; ++ci)
            {
                if (progressionTimedScored.count (ci) == 0)
                {
                    progressionQualitySum += 0; // missed
                    practiceChart.setAllChordNoteStates (ci, ProgressionChartComponent::NoteState::Missed);
                }
            }

            // Overall quality = average of per-chord qualities, rounded
            int quality = 0;
            if (progressionChordsTotal > 0)
                quality = juce::jlimit (0, 5,
                    juce::roundToInt (static_cast<double> (progressionQualitySum) / progressionChordsTotal));

            int keyIdx = (processorRef.progressionLibrary.getProgression (practicingProgressionId)->keyPitchClass
                          + progressionKeyOffset) % 12;
            processorRef.spacedRepetition.recordAttempt (practicingProgressionId, keyIdx, quality);
            updateStats();

            juce::String scoreText = juce::String (progressionChordsCorrect) + "/"
                                     + juce::String (progressionChordsTotal) + " chords";
            feedbackLabel.setText ("Complete! " + scoreText, juce::dontSendNotification);
            feedbackLabel.setColour (juce::Label::textColourId,
                quality >= 3 ? juce::Colour (ChordyTheme::success) : juce::Colour (ChordyTheme::danger));

            // Reset for next key — brief silence to avoid click
            processorRef.tempoEngine.resetBeatPosition();
            processorRef.tempoEngine.markChallengeStart();
            lastBeatInSequence = -1;
            progressionTimedScored.clear();

            int nextKey;
            if (customMode)
                nextKey = getNextCustomChallenge().keyIndex;
            else
                nextKey = processorRef.spacedRepetition.getNextChallenge (practicingProgressionId, transposedProgression.keyPitchClass).keyIndex;

            loadProgressionChallenge (nextKey);
            return;
        }

        // Update current chord if it changed — score missed chords
        if (targetChordIdx >= 0 && targetChordIdx != progressionChordIndex)
        {
            // Any chords between the old and new index that weren't scored = missed (Q0)
            for (int ci = progressionChordIndex; ci < targetChordIdx; ++ci)
            {
                if (progressionTimedScored.count (ci) == 0)
                {
                    progressionTimedScored.insert (ci);
                    progressionQualitySum += 0; // Q0 timeout
                    practiceChart.setAllChordNoteStates (ci, ProgressionChartComponent::NoteState::Missed);
                }
            }

            progressionChordIndex = targetChordIdx;
            progressionHasWrongAttempt = false;
            const auto& chord = transposedProgression.chords[static_cast<size_t> (targetChordIdx)];
            targetNotes = chord.midiNotes;
            practiceChart.setSelectedChord (targetChordIdx);
            practiceChart.setAllChordNoteStates (targetChordIdx, ProgressionChartComponent::NoteState::Target);

            int keyIdx = (processorRef.progressionLibrary.getProgression (practicingProgressionId)->keyPitchClass
                          + progressionKeyOffset) % 12;
            juce::String keyName = ChordDetector::noteNameFromPitchClass (keyIdx);
            targetLabel.setText ("Key: " + keyName + " - " + chord.getDisplayName(),
                                 juce::dontSendNotification);

            if (progressionTimedScored.count (targetChordIdx) == 0)
            {
                feedbackLabel.setText ("GO!", juce::dontSendNotification);
                feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::successBright));
                timingFeedbackLabel.setText ("", juce::dontSendNotification);
            }

            keyboardRef.clearAllColours();
            for (int note : targetNotes)
                keyboardRef.setKeyColour (note, KeyColour::Target);
            keyboardRef.repaint();
        }

        // Check if user is playing the correct chord
        if (targetChordIdx >= 0 && progressionTimedScored.count (targetChordIdx) == 0
            && ! activeNotes.empty())
        {
            updateKeyboardColours (activeNotes);

            std::set<int> targetPC, playedPC;
            for (int n : targetNotes) targetPC.insert (n % 12);
            for (int n : activeNotes) playedPC.insert (n % 12);

            // Update per-note visual states in real-time
            {
                const auto& chord = transposedProgression.chords[static_cast<size_t> (targetChordIdx)];
                for (int ni = 0; ni < static_cast<int> (chord.midiNotes.size()); ++ni)
                {
                    int notePC = chord.midiNotes[static_cast<size_t> (ni)] % 12;
                    if (playedPC.count (notePC) > 0)
                        practiceChart.setNoteState (targetChordIdx, ni,
                            ProgressionChartComponent::NoteState::Correct);
                }
            }

            // Track wrong notes (extras not in target)
            for (int pc : playedPC)
            {
                if (targetPC.count (pc) == 0)
                    progressionHasWrongAttempt = true;
            }

            if (playedPC == targetPC)
            {
                progressionTimedScored.insert (targetChordIdx);
                // Mark all notes as correct
                practiceChart.setAllChordNoteStates (targetChordIdx, ProgressionChartComponent::NoteState::Correct);

                const auto& chord = transposedProgression.chords[static_cast<size_t> (targetChordIdx)];
                double beatIntoChord = progressBeat - chord.startBeat;
                int quality = computeQuality (beatIntoChord, false);

                // Penalize if wrong notes were played at any point during this chord
                if (progressionHasWrongAttempt)
                    quality = juce::jmin (quality, 1);

                progressionQualitySum += quality;
                if (quality >= 3)
                    progressionChordsCorrect++;

                feedbackLabel.setText (progressionHasWrongAttempt ? "Wrong notes!" : "Correct!",
                                      juce::dontSendNotification);
                feedbackLabel.setColour (juce::Label::textColourId,
                    juce::Colour (progressionHasWrongAttempt ? ChordyTheme::danger : ChordyTheme::success));

                juce::String qualityText;
                juce::Colour qualityColour;
                switch (quality)
                {
                    case 5:  qualityText = "Perfect!";  qualityColour = juce::Colour (ChordyTheme::qualityPerfect); break;
                    case 4:  qualityText = "Good";      qualityColour = juce::Colour (ChordyTheme::qualityGood); break;
                    case 3:  qualityText = "OK";        qualityColour = juce::Colour (ChordyTheme::qualityOk); break;
                    case 2:  qualityText = "Slow";      qualityColour = juce::Colour (ChordyTheme::qualitySlow); break;
                    default: qualityText = "Late";      qualityColour = juce::Colour (ChordyTheme::qualityTimeout); break;
                }
                timingFeedbackLabel.setText (juce::String (beatIntoChord, 1) + " beats - " + qualityText,
                                             juce::dontSendNotification);
                timingFeedbackLabel.setColour (juce::Label::textColourId, qualityColour);
            }
        }
        else if (activeNotes.empty() && targetChordIdx >= 0
                 && progressionTimedScored.count (targetChordIdx) == 0)
        {
            keyboardRef.clearAllColours();
            for (int note : targetNotes)
                keyboardRef.setKeyColour (note, KeyColour::Target);
            keyboardRef.repaint();
        }

        return;
    }

    // --- Untimed progression practice ---

    // Auto-advance after success — wait for note release
    if (challengeCompleted)
    {
        if (activeNotes.empty())
        {
            challengeCompleted = false;
            advanceProgressionChord();
        }
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

    // Per-note visual feedback in real-time (untimed)
    {
        const auto& chord = transposedProgression.chords[static_cast<size_t> (progressionChordIndex)];
        for (int ni = 0; ni < static_cast<int> (chord.midiNotes.size()); ++ni)
        {
            int notePC = chord.midiNotes[static_cast<size_t> (ni)] % 12;
            if (playedPitchClasses.count (notePC) > 0)
                practiceChart.setNoteState (progressionChordIndex, ni,
                    ProgressionChartComponent::NoteState::Correct);
        }
    }

    if (playedPitchClasses == targetPitchClasses)
    {
        challengeCompleted = true;
        practiceChart.setAllChordNoteStates (progressionChordIndex, ProgressionChartComponent::NoteState::Correct);
        progressionChordsCorrect++;
        feedbackLabel.setText ("Correct!", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));
        currentRootColour = juce::Colour (ChordyTheme::successBright);
    }
}

//==============================================================================
// Melody practice
//==============================================================================

void PracticePanel::startMelodyPractice (const juce::String& melodyId)
{
    const auto* mel = processorRef.melodyLibrary.getMelody (melodyId);
    if (mel == nullptr || mel->notes.empty())
    {
        targetLabel.setText ("Invalid melody!", juce::dontSendNotification);
        return;
    }

    practicing = true;
    practiceType = PracticeType::Melody;
    practicingMelodyId = melodyId;
    challengeCompleted = false;
    previousFramePitchClasses.clear();
    melodyTimedScored.clear();
    melodyTimedQualitySum = 0;

    // Timed mode setup
    if (timedToggle.getToggleState())
    {
        if (auto* param = processorRef.apvts.getParameter ("metronomeOn"))
            param->setValueNotifyingHost (1.0f);
        processorRef.tempoEngine.resetBeatPosition();
        processorRef.tempoEngine.markChallengeStart();
        timedPhase = TimedPhase::CountIn;
        lastBeatInSequence = -1;
    }
    else
    {
        timedPhase = TimedPhase::Inactive;
    }

    startButton.setButtonText ("Stop");
    startButton.setColour (juce::TextButton::buttonColourId, juce::Colour (ChordyTheme::dangerMuted));
    nextButton.setEnabled (true);
    playButton.setEnabled (true);

    headerLabel.setText ("Practicing: " + mel->name, juce::dontSendNotification);

    // Get the first key to practice
    int keyIndex;
    if (customMode)
    {
        buildCustomKeySequence();
        keyIndex = getNextCustomChallenge().keyIndex;
    }
    else
    {
        keyIndex = processorRef.spacedRepetition.getNextChallenge (melodyId).keyIndex;
    }

    loadMelodyChallenge (keyIndex);
}

void PracticePanel::loadMelodyChallenge (int keyIndex)
{
    const auto* mel = processorRef.melodyLibrary.getMelody (practicingMelodyId);
    if (mel == nullptr)
    {
        stopPractice();
        return;
    }

    // Transpose to the target key
    int offset = (keyIndex - mel->keyPitchClass + 12) % 12;
    melodyKeyOffset = offset;
    transposedMelody = MelodyLibrary::transposeMelody (*mel, offset);
    melodyKeyRootMidi = 60 + transposedMelody.keyPitchClass;
    melodyNoteIndex = 0;
    melodyNotesCorrect = 0;
    melodyNotesTotal = static_cast<int> (transposedMelody.notes.size());
    previousFramePitchClasses.clear();
    lastCorrectPC = -1;
    currentBackingChordIndex = -1;
    melodyTimedScored.clear();
    melodyTimedQualitySum = 0;

    // Set up chart
    practiceMLChart.setVisible (true);
    practiceMLChart.setMelodyReadOnly (&transposedMelody);
    practiceMLChart.clearNoteStates();
    practiceMLChart.setNoteState (0, MelodyChartComponent::NoteState::Target);
    practiceMLChart.setHighlightedNoteIndex (0);

    // Show target
    juce::String keyName = ChordDetector::noteNameFromPitchClass (keyIndex);
    const auto& firstNote = transposedMelody.notes[0];
    int firstNotePC = (melodyKeyRootMidi + firstNote.intervalFromKeyRoot) % 12;
    if (firstNotePC < 0) firstNotePC += 12;
    juce::String noteName = ChordDetector::noteNameFromPitchClass (firstNotePC);

    targetLabel.setText ("Key: " + keyName + "  —  Play: " + noteName, juce::dontSendNotification);
    targetLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::textPrimary));
    feedbackLabel.setText ("", juce::dontSendNotification);
    timingFeedbackLabel.setText ("", juce::dontSendNotification);

    currentRootText = keyName;
    currentRootColour = juce::Colour (ChordyTheme::textTertiary);
    nextRootText = "";

    // Show first target note on keyboard
    int targetMidi = melodyKeyRootMidi + firstNote.intervalFromKeyRoot;
    keyboardRef.clearAllColours();
    if (targetMidi >= 0 && targetMidi < 128)
        keyboardRef.setKeyColour (targetMidi, KeyColour::Target);
    keyboardRef.repaint();

    // Start backing chord if enabled
    updateMelodyBacking();

    resized();
    repaint();
}

void PracticePanel::advanceMelodyNote()
{
    if (melodyNoteIndex >= static_cast<int> (transposedMelody.notes.size()))
    {
        // Completed the melody in this key
        int quality = 0;
        if (melodyNotesTotal > 0)
        {
            double ratio = static_cast<double> (melodyNotesCorrect) / melodyNotesTotal;
            if (ratio >= 0.95) quality = 5;
            else if (ratio >= 0.8) quality = 4;
            else if (ratio >= 0.6) quality = 3;
            else if (ratio >= 0.4) quality = 2;
            else if (ratio > 0.0) quality = 1;
        }

        int keyIndex = transposedMelody.keyPitchClass;
        processorRef.spacedRepetition.recordAttempt (practicingMelodyId, keyIndex, quality);
        updateStats();

        juce::String scoreText = juce::String (melodyNotesCorrect) + "/"
                                 + juce::String (melodyNotesTotal);
        feedbackLabel.setText ("Complete! " + scoreText + " correct", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId,
            quality >= 3 ? juce::Colour (ChordyTheme::success) : juce::Colour (ChordyTheme::danger));

        stopMelodyBacking();

        // Get next key
        int nextKey;
        if (customMode)
            nextKey = getNextCustomChallenge().keyIndex;
        else
            nextKey = processorRef.spacedRepetition.getNextChallenge (practicingMelodyId, transposedMelody.keyPitchClass).keyIndex;

        loadMelodyChallenge (nextKey);
        return;
    }

    // Show next target note
    practiceMLChart.setNoteState (melodyNoteIndex, MelodyChartComponent::NoteState::Target);
    practiceMLChart.setHighlightedNoteIndex (melodyNoteIndex);

    const auto& note = transposedMelody.notes[static_cast<size_t> (melodyNoteIndex)];
    int notePC = (melodyKeyRootMidi + note.intervalFromKeyRoot) % 12;
    if (notePC < 0) notePC += 12;
    juce::String noteName = ChordDetector::noteNameFromPitchClass (notePC);

    juce::String keyName = ChordDetector::noteNameFromPitchClass (transposedMelody.keyPitchClass);
    targetLabel.setText ("Key: " + keyName + "  —  Play: " + noteName, juce::dontSendNotification);
    feedbackLabel.setText ("", juce::dontSendNotification);

    // Add target on keyboard (don't clear — let Step 3 in updateMelodyPractice handle cleanup)
    int targetMidi = melodyKeyRootMidi + note.intervalFromKeyRoot;
    if (targetMidi >= 0 && targetMidi < 128)
        keyboardRef.setKeyColour (targetMidi, KeyColour::Target);

    // Update backing chord if needed
    updateMelodyBacking();
}

void PracticePanel::updateMelodyPractice (const std::vector<int>& activeNotes)
{
    // === TIMED MELODY PRACTICE ===
    if (timedPhase != TimedPhase::Inactive)
    {
        double beatsElapsed = processorRef.tempoEngine.getBeatsSinceChallengeStart();
        int beatInt = static_cast<int> (std::floor (beatsElapsed));

        // --- Count-in (4 beats) ---
        if (beatInt < 4)
        {
            if (beatInt != lastBeatInSequence)
            {
                lastBeatInSequence = beatInt;
                int countdown = 4 - beatInt;
                targetLabel.setText (juce::String (countdown) + "...", juce::dontSendNotification);
                feedbackLabel.setText ("", juce::dontSendNotification);

                if (beatInt >= 2 && ! transposedMelody.notes.empty())
                {
                    const auto& firstNote = transposedMelody.notes[0];
                    int pc = ((melodyKeyRootMidi + firstNote.intervalFromKeyRoot) % 12 + 12) % 12;
                    juce::String keyName = ChordDetector::noteNameFromPitchClass (transposedMelody.keyPitchClass);
                    juce::String noteName = ChordDetector::noteNameFromPitchClass (pc);
                    currentRootText = keyName;
                    currentRootColour = juce::Colour (ChordyTheme::textPrimary);
                    targetLabel.setText ("Get ready... " + noteName, juce::dontSendNotification);

                    int targetMidi = melodyKeyRootMidi + firstNote.intervalFromKeyRoot;
                    keyboardRef.clearAllColours();
                    if (targetMidi >= 0 && targetMidi < 128)
                        keyboardRef.setKeyColour (targetMidi, KeyColour::Target);
                    keyboardRef.repaint();
                }
            }
            return;
        }

        // --- After count-in: cursor follows beats through the melody ---
        double progressBeat = beatsElapsed - 4.0;
        practiceMLChart.setCursorBeat (progressBeat);

        // Find which note should be active at this beat
        int targetNoteIdx = -1;
        for (int ni = 0; ni < static_cast<int> (transposedMelody.notes.size()); ++ni)
        {
            const auto& n = transposedMelody.notes[static_cast<size_t> (ni)];
            if (progressBeat >= n.startBeat && progressBeat < n.startBeat + n.durationBeats)
            {
                targetNoteIdx = ni;
                break;
            }
        }

        // Melody ended
        if (progressBeat >= transposedMelody.totalBeats)
        {
            // Score any remaining unscored notes as missed
            for (int ni = 0; ni < static_cast<int> (transposedMelody.notes.size()); ++ni)
            {
                if (melodyTimedScored.count (ni) == 0)
                    practiceMLChart.setNoteState (ni, MelodyChartComponent::NoteState::Missed);
            }

            int totalNotes = static_cast<int> (transposedMelody.notes.size());
            int quality = 0;
            if (totalNotes > 0)
                quality = juce::jlimit (0, 5,
                    juce::roundToInt (static_cast<double> (melodyTimedQualitySum) / totalNotes));

            int keyIdx = transposedMelody.keyPitchClass;
            processorRef.spacedRepetition.recordAttempt (practicingMelodyId, keyIdx, quality);
            updateStats();

            int scored = static_cast<int> (melodyTimedScored.size());
            feedbackLabel.setText ("Complete! " + juce::String (scored) + "/" + juce::String (totalNotes) + " notes",
                                   juce::dontSendNotification);
            feedbackLabel.setColour (juce::Label::textColourId,
                quality >= 3 ? juce::Colour (ChordyTheme::success) : juce::Colour (ChordyTheme::danger));

            stopMelodyBacking();
            practiceMLChart.setCursorBeat (-1.0);

            // Reset for next key
            processorRef.tempoEngine.resetBeatPosition();
            processorRef.tempoEngine.markChallengeStart();
            lastBeatInSequence = -1;
            melodyTimedScored.clear();
            melodyTimedQualitySum = 0;

            int nextKey;
            if (customMode)
                nextKey = getNextCustomChallenge().keyIndex;
            else
                nextKey = processorRef.spacedRepetition.getNextChallenge (practicingMelodyId, transposedMelody.keyPitchClass).keyIndex;

            loadMelodyChallenge (nextKey);
            return;
        }

        // Note changed — mark any skipped notes as missed
        if (targetNoteIdx >= 0 && targetNoteIdx != melodyNoteIndex)
        {
            for (int ni = melodyNoteIndex; ni < targetNoteIdx; ++ni)
            {
                if (melodyTimedScored.count (ni) == 0)
                {
                    melodyTimedScored.insert (ni);
                    practiceMLChart.setNoteState (ni, MelodyChartComponent::NoteState::Missed);
                }
            }
            melodyNoteIndex = targetNoteIdx;
            practiceMLChart.setHighlightedNoteIndex (targetNoteIdx);

            juce::String keyName = ChordDetector::noteNameFromPitchClass (transposedMelody.keyPitchClass);
            const auto& note = transposedMelody.notes[static_cast<size_t> (targetNoteIdx)];
            int pc = ((melodyKeyRootMidi + note.intervalFromKeyRoot) % 12 + 12) % 12;
            juce::String noteName = ChordDetector::noteNameFromPitchClass (pc);
            targetLabel.setText ("Key: " + keyName + " - Play: " + noteName, juce::dontSendNotification);

            if (melodyTimedScored.count (targetNoteIdx) == 0)
            {
                feedbackLabel.setText ("GO!", juce::dontSendNotification);
                feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::successBright));
                timingFeedbackLabel.setText ("", juce::dontSendNotification);
            }

            int targetMidi = melodyKeyRootMidi + note.intervalFromKeyRoot;
            keyboardRef.clearAllColours();
            if (targetMidi >= 0 && targetMidi < 128)
                keyboardRef.setKeyColour (targetMidi, KeyColour::Target);
            keyboardRef.repaint();

            updateMelodyBacking();
        }

        // Check for new note-ons and score
        if (targetNoteIdx >= 0 && melodyTimedScored.count (targetNoteIdx) == 0)
        {
            std::set<int> currentPCs;
            for (int n : activeNotes)
                currentPCs.insert (((n % 12) + 12) % 12);

            std::set<int> newNoteOns;
            for (int pc : currentPCs)
                if (previousFramePitchClasses.count (pc) == 0)
                    newNoteOns.insert (pc);

            previousFramePitchClasses = currentPCs;

            const auto& note = transposedMelody.notes[static_cast<size_t> (targetNoteIdx)];
            int targetPC = ((melodyKeyRootMidi + note.intervalFromKeyRoot) % 12 + 12) % 12;

            for (int pc : newNoteOns)
            {
                if (pc == targetPC)
                {
                    melodyTimedScored.insert (targetNoteIdx);
                    practiceMLChart.setNoteState (targetNoteIdx, MelodyChartComponent::NoteState::Correct);

                    double beatIntoNote = progressBeat - note.startBeat;
                    int quality = computeQuality (beatIntoNote, false);
                    melodyTimedQualitySum += quality;
                    melodyNotesCorrect++;

                    feedbackLabel.setText ("Correct!", juce::dontSendNotification);
                    feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));

                    juce::String qualityText;
                    juce::Colour qualityColour;
                    switch (quality)
                    {
                        case 5:  qualityText = "Perfect!";  qualityColour = juce::Colour (ChordyTheme::qualityPerfect); break;
                        case 4:  qualityText = "Good";      qualityColour = juce::Colour (ChordyTheme::qualityGood); break;
                        case 3:  qualityText = "OK";        qualityColour = juce::Colour (ChordyTheme::qualityOk); break;
                        case 2:  qualityText = "Slow";      qualityColour = juce::Colour (ChordyTheme::qualitySlow); break;
                        default: qualityText = "Late";      qualityColour = juce::Colour (ChordyTheme::qualityTimeout); break;
                    }
                    timingFeedbackLabel.setText (juce::String (beatIntoNote, 1) + " beats - " + qualityText,
                                                 juce::dontSendNotification);
                    timingFeedbackLabel.setColour (juce::Label::textColourId, qualityColour);
                    updateStats();
                    break;
                }
            }

            // Continuous keyboard refresh
            keyboardRef.clearAllColours();
            int targetMidi = melodyKeyRootMidi + note.intervalFromKeyRoot;
            if (targetMidi >= 0 && targetMidi < 128)
                keyboardRef.setKeyColour (targetMidi, KeyColour::Target);
            for (int n : activeNotes)
            {
                int pc = ((n % 12) + 12) % 12;
                if (pc == targetPC)
                    keyboardRef.setKeyColour (n, KeyColour::Correct);
            }
            keyboardRef.repaint();
        }
        else
        {
            previousFramePitchClasses.clear();
            for (int n : activeNotes)
                previousFramePitchClasses.insert (((n % 12) + 12) % 12);
        }

        return;
    }

    // === UNTIMED MELODY PRACTICE ===
    if (melodyNoteIndex >= static_cast<int> (transposedMelody.notes.size()))
        return;

    // Step 1: Compute current pitch classes
    std::set<int> currentPCs;
    for (int n : activeNotes)
    {
        int pc = n % 12;
        if (pc < 0) pc += 12;
        currentPCs.insert (pc);
    }

    // Step 2: Find NEW note-ons (pitch classes not in previous frame)
    std::set<int> newNoteOns;
    for (int pc : currentPCs)
        if (previousFramePitchClasses.count (pc) == 0)
            newNoteOns.insert (pc);

    previousFramePitchClasses = currentPCs;

    // Clear lastCorrectPC when all notes released
    if (activeNotes.empty())
        lastCorrectPC = -1;

    // Step 3: No new notes — refresh keyboard every frame (same as voicing practice)
    if (newNoteOns.empty())
    {
        keyboardRef.clearAllColours();
        if (melodyNoteIndex < static_cast<int> (transposedMelody.notes.size()))
        {
            const auto& tgt = transposedMelody.notes[static_cast<size_t> (melodyNoteIndex)];
            int tgtMidi = melodyKeyRootMidi + tgt.intervalFromKeyRoot;
            int tgtPC = ((melodyKeyRootMidi + tgt.intervalFromKeyRoot) % 12 + 12) % 12;
            if (tgtMidi >= 0 && tgtMidi < 128)
                keyboardRef.setKeyColour (tgtMidi, KeyColour::Target);
            for (int n : activeNotes)
            {
                int pc = ((n % 12) + 12) % 12;
                if (pc == tgtPC || pc == lastCorrectPC)
                    keyboardRef.setKeyColour (n, KeyColour::Correct);
            }
        }
        keyboardRef.repaint();
        return;
    }

    // Step 4: Check new notes against target
    const auto& target = transposedMelody.notes[static_cast<size_t> (melodyNoteIndex)];
    int targetPC = (melodyKeyRootMidi + target.intervalFromKeyRoot) % 12;
    if (targetPC < 0) targetPC += 12;

    bool gotCorrect = false;
    bool gotWrong = false;

    for (int pc : newNoteOns)
    {
        if (pc == targetPC)
            gotCorrect = true;
        else
            gotWrong = true;
    }

    // Step 5: Score and advance
    if (gotCorrect)
    {
        if (! gotWrong)
            melodyNotesCorrect++;

        practiceMLChart.setNoteState (melodyNoteIndex, MelodyChartComponent::NoteState::Correct);
        lastCorrectPC = targetPC;
        melodyNoteIndex++;

        feedbackLabel.setText ("Correct!", juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::success));

        advanceMelodyNote(); // shows next note target or triggers key completion
        return;
    }

    // Wrong note — don't advance (Step 3 handles keyboard coloring next frame)
    feedbackLabel.setText ("Wrong note!", juce::dontSendNotification);
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colour (ChordyTheme::danger));
}

//==============================================================================
// Melody backing pad
//==============================================================================

void PracticePanel::updateMelodyBacking()
{
    if (! backingToggle.getToggleState() || transposedMelody.chordContexts.empty())
    {
        stopMelodyBacking();
        return;
    }

    if (melodyNoteIndex >= static_cast<int> (transposedMelody.notes.size()))
        return;

    // Find which chord context is active for the current note
    double currentBeat = transposedMelody.notes[static_cast<size_t> (melodyNoteIndex)].startBeat;
    int activeCC = -1;
    for (int i = 0; i < static_cast<int> (transposedMelody.chordContexts.size()); ++i)
    {
        const auto& cc = transposedMelody.chordContexts[static_cast<size_t> (i)];
        if (currentBeat >= cc.startBeat && currentBeat < cc.startBeat + cc.durationBeats)
        {
            activeCC = i;
            break;
        }
    }

    if (activeCC < 0)
        activeCC = 0; // fallback to first

    // Only change if chord context changed
    if (activeCC == currentBackingChordIndex)
        return;

    // Stop old backing notes
    stopMelodyBacking();

    currentBackingChordIndex = activeCC;
    const auto& cc = transposedMelody.chordContexts[static_cast<size_t> (activeCC)];

    // Get chord tones for this quality
    auto chordTones = ChordDetector::getChordTones (cc.quality);
    int chordRoot = melodyKeyRootMidi + cc.intervalFromKeyRoot;

    // Build MIDI notes for the backing pad (in a low register for pad sound)
    int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    backingChordNotes.clear();

    for (int tone : chordTones)
    {
        int midiNote = chordRoot + tone;
        // Bring into range 48-72 (C3-C5) for a warm pad
        while (midiNote > 72) midiNote -= 12;
        while (midiNote < 48) midiNote += 12;

        if (midiNote >= 0 && midiNote < 128)
        {
            backingChordNotes.push_back (midiNote);
            processorRef.addPreviewMidi (juce::MidiMessage::noteOn (ch, midiNote, 0.4f));
        }
    }
}

void PracticePanel::stopMelodyBacking()
{
    if (backingChordNotes.empty())
        return;

    int ch = static_cast<int> (*processorRef.apvts.getRawParameterValue ("midiChannel"));
    for (int note : backingChordNotes)
        processorRef.addPreviewMidi (juce::MidiMessage::noteOff (ch, note, 0.0f));

    backingChordNotes.clear();
    currentBackingChordIndex = -1;
}
