#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "VoicingModel.h"
#include "SpacedRepetition.h"
#include "ChordDetector.h"
#include "ProgressionModel.h"
#include "ProgressionChartComponent.h"
#include "MelodyModel.h"
#include "MelodyChartComponent.h"
#include <set>
#include <random>

class AudioPluginAudioProcessor;
class ChordyKeyboardComponent;

enum class RootOrder { Chromatic, Random };
enum class PracticeType { Voicing, Progression, Melody };

class PracticePanel : public juce::Component
{
public:
    PracticePanel (AudioPluginAudioProcessor& processor,
                   ChordyKeyboardComponent& keyboard);

    void resized() override;
    void paint (juce::Graphics& g) override;

    // Call from timer to update practice state
    void updatePractice (const std::vector<int>& activeNotes);

    // Start practicing a specific voicing
    void startPractice (const juce::String& voicingId);

    // Start practicing a progression
    void startProgressionPractice (const juce::String& progressionId);

    // Stop practice mode
    void stopPractice();

    bool isPracticing() const { return practicing; }
    void setStartEnabled (bool enabled) { startButton.setEnabled (enabled); }
    bool isTimedActive() const { return timedPhase != TimedPhase::Inactive; }
    bool isTimedMode() const { return timedToggle.getToggleState(); }

    // Get current practice display for the main chord area above the keyboard
    juce::String getCurrentRootText() const { return currentRootText; }
    juce::Colour getCurrentRootColour() const { return currentRootColour; }
    juce::String getNextRootText() const { return nextRootText; }
    juce::String getCountdownText() const { return countdownText; }

    // Called by editor when user selects a voicing/progression/melody in the library
    void setSelectedVoicingId (const juce::String& id);
    void setSelectedProgressionId (const juce::String& id);
    void setSelectedMelodyId (const juce::String& id);

    // Show chart preview for selected item (even when not practicing)
    void showProgressionPreview (const Progression* prog);
    void showMelodyPreview (const Melody* mel);
    void clearChartPreview();
    void showProgressionCursor (double beat);
    void showMelodyCursor (double beat);
    juce::String getPlaybackChordName() const;
    juce::String getClickedChordName() const { return clickedChordName; }
    void setClickedChordName (const juce::String& name, int frames = 60) { clickedChordName = name; clickedChordFrames = frames; }
    void clearClickedChordName() { clickedChordName = {}; clickedChordFrames = 0; }
    void tickClickedChord() { if (clickedChordFrames > 0 && --clickedChordFrames == 0) clickedChordName = {}; }

private:
    juce::String selectedVoicingId;
    juce::String selectedProgressionId;
    juce::String selectedMelodyId;
    PracticeType practiceType = PracticeType::Voicing;

    // Main chord display state
    juce::String currentRootText;
    juce::Colour currentRootColour { juce::Colours::white };
    juce::String nextRootText;
    juce::String countdownText;
    juce::String clickedChordName;
    int clickedChordFrames = 0;
    AudioPluginAudioProcessor& processorRef;
    ChordyKeyboardComponent& keyboardRef;

    juce::Label headerLabel;
    juce::Label targetLabel;
    juce::Label feedbackLabel;
    juce::Label statsLabel;
    juce::TextButton startButton { "Start" };
    juce::TextButton nextButton { "Next" };
    juce::TextButton playButton { "Play" };
    juce::TextButton customButton { "Custom" };
    juce::ToggleButton timedToggle { "Timed" };
    juce::Label timingFeedbackLabel;

    // Key selector UI (visible when showingKeySelector)
    bool showingKeySelector = false;
    juce::ToggleButton keyToggles[12];
    juce::TextButton selectAllButton { "All" };
    juce::TextButton deselectAllButton { "None" };
    juce::ComboBox orderCombo;

    // Custom practice state
    bool customMode = false;
    std::set<int> customAllowedKeys;
    RootOrder rootOrder = RootOrder::Random;
    std::vector<int> customKeySequence;
    int customKeyIndex = 0;
    std::mt19937 rng { std::random_device{}() };

    bool practicing = false;
    juce::String practicingVoicingId;  // which voicing we're practicing
    PracticeChallenge currentChallenge;
    std::vector<int> targetNotes;   // absolute MIDI notes for current challenge
    bool challengeCompleted = false;
    double challengeCompletedTime = 0.0;

    // Timed practice state machine
    enum class TimedPhase { Inactive, CountIn, Prep, Play };
    TimedPhase timedPhase = TimedPhase::Inactive;
    int lastBeatInSequence = -1;        // for detecting beat transitions
    bool playPhaseScored = false;       // has current play phase been scored
    bool hasWrongAttempt = false;
    int lastQualityScore = -1;
    double playPhaseStartBeatFraction = 0.0; // fractional beat position when play phase started
    PracticeChallenge nextChallenge;    // pre-fetched for prep display

    // Chart preview data (stored here so pointers stay valid)
    Progression previewProgression;
    Melody previewMelody;
    bool showingProgPreview = false;
    bool showingMelPreview = false;

    // --- Progression practice state ---
    juce::String practicingProgressionId;
    Progression transposedProgression;  // current key's transposition
    int progressionChordIndex = 0;      // which chord we're on
    int progressionKeyOffset = 0;       // semitones from original key
    ProgressionChartComponent practiceChart;
    juce::Viewport practiceChartViewport;
    juce::ToggleButton progDetailedToggle { "Detailed" };
    double progressionTimedBeat = 0.0;           // current beat in timed progression practice
    std::set<int> progressionTimedScored;         // chord indices already scored this run
    int progressionChordsCorrect = 0;             // chords scored >= Q3
    int progressionChordsTotal = 0;               // total chords in progression
    int progressionQualitySum = 0;                // sum of all chord qualities (for avg)

    // --- Melody practice state ---
    juce::String practicingMelodyId;
    Melody transposedMelody;
    int melodyNoteIndex = 0;
    int melodyNotesCorrect = 0;
    int melodyNotesTotal = 0;
    int melodyKeyOffset = 0;
    int melodyKeyRootMidi = 60;
    std::set<int> previousFramePitchClasses;
    int lastCorrectPC = -1;  // pitch class of last correctly played note (for green hold)
    std::set<int> melodyTimedScored;       // note indices already scored in timed mode
    int melodyTimedQualitySum = 0;         // sum of per-note qualities for averaging
    MelodyChartComponent practiceMLChart;
    juce::Viewport practiceMLChartViewport;
    juce::ToggleButton backingToggle { "Backing" };
    std::vector<int> backingChordNotes;
    int currentBackingChordIndex = -1;

    std::set<int> lastAttemptPCs;  // last notes user played (for proportional scoring on skip/timeout)

    static int computeQuality (double beatsElapsed, bool hadWrongAttempt);
    static double computeProportionalMatch (const std::set<int>& playedPCs,
                                            const std::set<int>& targetPCs);
    static int proportionToQuality (double proportion, double beatsElapsed = -1.0);

    void onStartStop();
    void onNext();
    void onPlay();
    void onCustomToggle();
    void buildCustomKeySequence();
    PracticeChallenge getNextCustomChallenge();
    void loadNextChallenge();
    void updateTimedPractice (const std::vector<int>& activeNotes);
    void updateUntimedPractice (const std::vector<int>& activeNotes);
    void updateProgressionPractice (const std::vector<int>& activeNotes);
    void enterPlayPhase();
    void enterPrepPhase();
    void scorePlayPhase (const std::vector<int>& activeNotes);
    void updateKeyboardColours (const std::vector<int>& activeNotes);
    void updateStats();

    void loadProgressionChallenge (int keyIndex);
    void advanceProgressionChord();

    // Melody practice methods
    void startMelodyPractice (const juce::String& melodyId);
    void loadMelodyChallenge (int keyIndex);
    void advanceMelodyNote();
    void updateMelodyPractice (const std::vector<int>& activeNotes);
    void updateMelodyBacking();
    void stopMelodyBacking();
};
