#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "VoicingModel.h"
#include "SpacedRepetition.h"
#include "ChordDetector.h"
#include "ProgressionModel.h"
#include "ProgressionChartComponent.h"
#include <set>
#include <random>

class AudioPluginAudioProcessor;
class ChordyKeyboardComponent;

enum class RootOrder { Chromatic, Random };
enum class PracticeType { Voicing, Progression };

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
    bool isTimedActive() const { return timedPhase != TimedPhase::Inactive; }
    bool isTimedMode() const { return timedToggle.getToggleState(); }

    // Get current practice display for the main chord area above the keyboard
    juce::String getCurrentRootText() const { return currentRootText; }
    juce::Colour getCurrentRootColour() const { return currentRootColour; }
    juce::String getNextRootText() const { return nextRootText; }

    // Called by editor when user selects a voicing/progression in the library
    void setSelectedVoicingId (const juce::String& id) { selectedVoicingId = id; practiceType = PracticeType::Voicing; }
    void setSelectedProgressionId (const juce::String& id) { selectedProgressionId = id; practiceType = PracticeType::Progression; }

private:
    juce::String selectedVoicingId;
    juce::String selectedProgressionId;
    PracticeType practiceType = PracticeType::Voicing;

    // Main chord display state
    juce::String currentRootText;
    juce::Colour currentRootColour { juce::Colours::white };
    juce::String nextRootText;
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

    // --- Progression practice state ---
    juce::String practicingProgressionId;
    Progression transposedProgression;  // current key's transposition
    int progressionChordIndex = 0;      // which chord we're on
    int progressionKeyOffset = 0;       // semitones from original key
    ProgressionChartComponent practiceChart;

    static int computeQuality (double beatsElapsed, bool hadWrongAttempt);

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
};
