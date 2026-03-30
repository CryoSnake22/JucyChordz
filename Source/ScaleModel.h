#pragma once

#include "VoicingModel.h"
#include "MelodyModel.h"
#include <juce_core/juce_core.h>
#include <vector>

enum class ScaleType
{
    Major,
    NaturalMinor,
    Dorian,
    Mixolydian,
    Lydian,
    Phrygian,
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
    WholeTone,
    DiminishedHW,
    DiminishedWH,
    Chromatic,
    BebopMajor,
    BebopDominant,
    NumScaleTypes
};

namespace ScaleModel
{
    // Returns the interval template for a scale (pitch classes relative to root=0)
    std::vector<int> getScaleIntervals (ScaleType type);

    // Display name for UI
    juce::String getScaleName (ScaleType type);

    // Check if all voicing pitch classes fit within the scale rooted at rootPC
    bool voicingFitsInScale (const Voicing& v, ScaleType type, int rootPC);

    // Build MIDI root note sequence for up-and-back walk through scale degrees (Follow mode).
    // baseMidi = voicing's octaveReference (MIDI note of root when recorded).
    std::vector<int> buildScaleRootSequence (ScaleType type, int rootPC, int baseMidi);

    // Extract MIDI root note sequence from melody notes (Follow/Melody mode).
    // baseMidi = voicing's octaveReference.
    std::vector<int> buildMelodyFollowSequence (const Melody& mel, int baseMidi);

    // Build up-and-back degree index sequence: {0,1,...,n,...,1}
    std::vector<int> buildScaleDegreeUpAndBack (int numDegrees);

    // Diatonic transposition: move each voicing note independently by `degree` scale steps.
    // Returns absolute MIDI notes. baseMidi = voicing's octaveReference.
    std::vector<int> diatonicTranspose (const Voicing& v, ScaleType type,
                                        int scaleRootPC, int degree, int baseMidi);
}
