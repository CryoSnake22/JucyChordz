#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "VoicingModel.h"
#include "SpacedRepetition.h"
#include "ChordDetector.h"
#include "ProgressionModel.h"
#include "ProgressionChartComponent.h"
#include "MelodyModel.h"
#include "MelodyChartComponent.h"
#include "ScaleModel.h"
#include <set>
#include <random>

class AudioPluginAudioProcessor;
class ChordyKeyboardComponent;

enum class RootOrder { Chromatic, Random, Follow, Scale, Free };
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
    bool isDrillActive() const { return drillActive; }
    int getDrillMasteredCount() const;
    int getDrillTotalKeys() const;
    int getDrillBpmLevel() const { return drillBpmLevel; }
    float getDrillStartBpm() const { return drillStartBpm; }
    void setStartEnabled (bool enabled) { startButton.setEnabled (enabled); }
    bool isTimedActive() const { return timedPhase != TimedPhase::Inactive; }
    bool isTimedMode() const { return timedToggle.getToggleState(); }
    bool isExercisePreviewActive() const { return exercisePreviewPlaying; }
    bool canPlayExercisePreview() const;
    void playExercisePreview();
    void stopExercisePreview();

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
    void highlightNotesAtBeat (double beat);
    void highlightMelodyNotesAtBeat (double beat);
    void showVoicingChartPreview (const juce::String& voicingId);
    std::vector<int> applyCurrentTransforms (const std::vector<int>& notes) const;
    juce::String getPlaybackChordName() const;
    juce::String getClickedChordName() const { return clickedChordName; }
    void setClickedChordName (const juce::String& name, int frames = 60) { clickedChordName = name; clickedChordFrames = frames; }
    void clearClickedChordName() { clickedChordName = {}; clickedChordFrames = 0; }
    void tickClickedChord() { if (clickedChordFrames > 0 && --clickedChordFrames == 0) clickedChordName = {}; }
    void tickChordPreview();
    void clearChordSelection();
    void tickMelodyPreview();
    void clearMelodySelection();

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

public:
    // Voicing save/see from progression chord click
    juce::TextButton voicingButton { "Save Voicing" };
    std::function<void (const std::vector<int>& notes, const std::vector<int>& velocities)> onSaveVoicing;
    std::function<void (const juce::String& voicingId)> onSeeVoicing;

private:
    juce::String clickedVoicingMatchId;
    std::vector<int> clickedChordMidiNotes;
    std::vector<int> clickedChordVelocities;
    std::vector<int> chordPreviewNotes;
    int chordPreviewFrames = 0;
    static constexpr int chordPreviewHoldFrames = 300; // 5 seconds at 60Hz
    int melodyPreviewNote = -1;
    int melodyPreviewFrames = 0;
    static constexpr int melodyPreviewHoldFrames = 300; // 5 seconds at 60Hz
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
    juce::ToggleButton drillToggle { "Drill" };
    juce::ToggleButton autoBpmToggle { "Auto BPM" };
    juce::Label timingFeedbackLabel;

    // Key selector UI (visible when showingKeySelector)
    bool showingKeySelector = false;
    juce::ToggleButton keyToggles[12];
    juce::TextButton selectAllButton { "All" };
    juce::TextButton deselectAllButton { "None" };
    juce::ComboBox orderCombo;

    // Inversion / Drop (voicing practice only)
    juce::ComboBox inversionCombo;
    juce::ComboBox dropCombo;
    int currentInversion = 0;  // 0 = root position
    std::vector<int> currentDrop;  // empty = none, {2} = drop-2, {2,4} = drop 2+4, etc.
    juce::String lastInvDropVoicingId;
    void updateInversionDropCombos (const Voicing* voicing);
    void onInversionDropChanged();

    // Custom practice state
    bool customMode = false;
    std::set<int> customAllowedKeys;
    RootOrder rootOrder = RootOrder::Random;
    std::vector<int> customKeySequence;
    int customKeyIndex = 0;
    std::mt19937 rng { std::random_device{}() };

    // Drill mode state (weighted random key selection with BPM escalation)
    bool drillActive = false;
    struct DrillKeyState {
        int attempts = 0;
        double emaQuality = 0.0;
        bool initialized = false;
        static constexpr double alpha = 0.5;

        void recordQuality (int quality)
        {
            attempts++;
            if (! initialized) { emaQuality = quality; initialized = true; }
            else { emaQuality = alpha * quality + (1.0 - alpha) * emaQuality; }
        }
        double weight() const
        {
            if (! initialized) return 25.0;
            double w = 5.0 - emaQuality;
            return std::max (0.05, w * w * w);
        }
        bool mastered (PracticeType type) const
        {
            double threshold = (type == PracticeType::Melody) ? 4.0 : 4.5;
            return attempts >= 3 && emaQuality >= threshold;
        }
    };
    std::array<DrillKeyState, 12> drillKeyStates {};
    int drillBpmLevel = 0;
    float drillStartBpm = 120.0f;
    int lastDrillKey = -1;
    int pickDrillKey();
    void onDrillScored (int keyIndex, int quality);
    void checkDrillMastery();
    void resetDrillState();
    void updateDrillDisplay();

    // Follow/Scale mode state
    ScaleType selectedScaleType = ScaleType::Major;
    int scaleRootPitchClass = 0;
    enum class FollowSource { Scale, Melody };
    FollowSource followSource = FollowSource::Scale;
    juce::String followMelodyId;
    int currentScaleDegree = 0;
    std::vector<int> scaleDegreeSequence;

    // Two-level loop for Follow(scale)/Scale: outer = chromatic keys, inner = scale degrees
    std::vector<int> scaleInnerSequence;   // inner loop: semitone offsets (Follow) or degree indices (Scale)
    int innerIndex = 0;
    int currentModulationKey = 0;          // current outer chromatic key (pitch class)
    int currentModulationBaseMidi = 60;    // base MIDI for current modulation key
    bool pendingModulationCountIn = false; // triggers 4-beat count-in at modulation boundary

    // Follow/Scale UI components
    juce::ComboBox scalePickerCombo;
    juce::ComboBox followSourceCombo;
    juce::ComboBox melodyPickerCombo;
    juce::TextButton exercisePlayButton { "Play" };

    // Exercise preview playback state
    bool exercisePreviewPlaying = false;

    bool practicing = false;
    juce::String practicingVoicingId;  // which voicing we're practicing
    int practiceOctaveRef = 60;       // voicing's octaveReference (MIDI note of root when recorded)
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
    std::vector<int> computeTargetNotes (const Voicing& v, const PracticeChallenge& challenge);
    void updateScalePickerAvailability();

    // Exercise preview (private helpers; playExercisePreview/stopExercisePreview are public)
    void playExerciseInKey (int chromaticKey);
    void buildExercisePreviewProgression();
    std::vector<std::vector<int>> buildExerciseNoteSequence (int chromaticKey);
    Progression buildExerciseProgression (int chromaticKey);
    void populateMelodyPicker();
    void updateFollowScaleVisibility();
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
