#pragma once

#include "VoicingModel.h"
#include "ProgressionModel.h"
#include "MelodyModel.h"
#include "FolderModel.h"

namespace LibraryExporter
{
    struct ExportOptions
    {
        juce::String collectionName;
        std::vector<juce::String> voicingIds;      // empty = all
        std::vector<juce::String> progressionIds;   // empty = all
        std::vector<juce::String> melodyIds;         // empty = all
        bool includeVoicings = true;
        bool includeProgressions = true;
        bool includeMelodies = true;
    };

    bool exportCollection (const ExportOptions& opts,
                           const VoicingLibrary& vl,
                           const ProgressionLibrary& pl,
                           const MelodyLibrary& ml,
                           const juce::File& destFile);

    struct ImportResult
    {
        std::vector<Voicing> voicings;
        std::vector<Progression> progressions;
        std::vector<Melody> melodies;
        std::vector<Folder> voicingFolders;
        std::vector<Folder> progressionFolders;
        std::vector<Folder> melodyFolders;
        juce::String collectionName;
        bool success = false;
    };

    ImportResult parseCollection (const juce::File& file);

    struct MergeResult { int voicingsAdded = 0; int progressionsAdded = 0; int melodiesAdded = 0; };

    MergeResult mergeIntoLibraries (const ImportResult& imported,
                                     VoicingLibrary& vl,
                                     ProgressionLibrary& pl,
                                     MelodyLibrary& ml);
}
