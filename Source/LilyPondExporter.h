#pragma once

#include "VoicingModel.h"
#include "ProgressionModel.h"
#include "ProgressionRecorder.h"
#include "MelodyModel.h"
#include <juce_core/juce_core.h>

namespace LilyPondExporter
{
    struct ExportOptions
    {
        juce::String title;
        juce::String composer;
        std::vector<int> keys;           // pitch classes 0-11 to export
        bool includeChordSymbols = true;
        bool grandStaff = true;          // piano grand staff vs single treble
        juce::String paperSize = "letter";
    };

    struct ExportResult
    {
        bool success = false;
        juce::String errorMessage;
        juce::File pdfFile;
    };

    // Detect lilypond binary on the system
    juce::File findLilyPondBinary();

    // Generate .ly content from Chordy data
    juce::String generateVoicingLy (const Voicing& v, const ExportOptions& opts);
    juce::String generateProgressionLy (const Progression& prog, const ExportOptions& opts);
    juce::String generateMelodyLy (const Melody& mel, const ExportOptions& opts);

    // Write .ly file, invoke lilypond CLI, return result
    ExportResult renderToPdf (const juce::String& lyContent, const juce::File& outputPdf);
}
