#include "LibraryExporter.h"

bool LibraryExporter::exportCollection (const ExportOptions& opts,
                                         const VoicingLibrary& vl,
                                         const ProgressionLibrary& pl,
                                         const MelodyLibrary& ml,
                                         const juce::File& destFile)
{
    juce::ValueTree root ("ChordyCollection");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("name", opts.collectionName, nullptr);

    // Voicings
    if (opts.includeVoicings)
    {
        auto vlTree = juce::ValueTree ("VoicingLibrary");
        const auto& all = vl.getAllVoicings();
        bool exportAll = opts.voicingIds.empty();

        for (const auto& v : all)
        {
            if (exportAll || std::find (opts.voicingIds.begin(), opts.voicingIds.end(), v.id) != opts.voicingIds.end())
            {
                // Re-use the library's serialization by building a temp library
                VoicingLibrary temp;
                temp.addVoicing (v);
                auto tempTree = temp.toValueTree();
                for (int i = 0; i < tempTree.getNumChildren(); ++i)
                {
                    auto child = tempTree.getChild (i);
                    if (child.hasType ("Voicing"))
                        vlTree.appendChild (child.createCopy(), nullptr);
                }
            }
        }

        // Include folders if exporting all
        if (exportAll)
            vlTree.appendChild (vl.getFolders().toValueTree(), nullptr);

        root.appendChild (vlTree, nullptr);
    }

    // Progressions
    if (opts.includeProgressions)
    {
        auto plTree = juce::ValueTree ("ProgressionLibrary");
        const auto& all = pl.getAllProgressions();
        bool exportAll = opts.progressionIds.empty();

        for (const auto& p : all)
        {
            if (exportAll || std::find (opts.progressionIds.begin(), opts.progressionIds.end(), p.id) != opts.progressionIds.end())
            {
                ProgressionLibrary temp;
                temp.addProgression (p);
                auto tempTree = temp.toValueTree();
                for (int i = 0; i < tempTree.getNumChildren(); ++i)
                {
                    auto child = tempTree.getChild (i);
                    if (child.hasType ("Progression"))
                        plTree.appendChild (child.createCopy(), nullptr);
                }
            }
        }

        if (exportAll)
            plTree.appendChild (pl.getFolders().toValueTree(), nullptr);

        root.appendChild (plTree, nullptr);
    }

    // Melodies
    if (opts.includeMelodies)
    {
        auto mlTree = juce::ValueTree ("MelodyLibrary");
        const auto& all = ml.getAllMelodies();
        bool exportAll = opts.melodyIds.empty();

        for (const auto& m : all)
        {
            if (exportAll || std::find (opts.melodyIds.begin(), opts.melodyIds.end(), m.id) != opts.melodyIds.end())
            {
                MelodyLibrary temp;
                temp.addMelody (m);
                auto tempTree = temp.toValueTree();
                for (int i = 0; i < tempTree.getNumChildren(); ++i)
                {
                    auto child = tempTree.getChild (i);
                    if (child.hasType ("Melody"))
                        mlTree.appendChild (child.createCopy(), nullptr);
                }
            }
        }

        if (exportAll)
            mlTree.appendChild (ml.getFolders().toValueTree(), nullptr);

        root.appendChild (mlTree, nullptr);
    }

    auto xml = root.createXml();
    if (xml == nullptr) return false;

    destFile.deleteFile();
    return xml->writeTo (destFile);
}

LibraryExporter::ImportResult LibraryExporter::parseCollection (const juce::File& file)
{
    ImportResult result;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("ChordyCollection"))
        return result;

    auto root = juce::ValueTree::fromXml (*xml);
    result.collectionName = root.getProperty ("name").toString();

    // Parse voicings
    auto vlTree = root.getChildWithName ("VoicingLibrary");
    if (vlTree.isValid())
    {
        VoicingLibrary temp;
        temp.fromValueTree (vlTree);
        result.voicings = temp.getAllVoicings();
        result.voicingFolders = temp.getFolders().getAllFolders();
    }

    // Parse progressions
    auto plTree = root.getChildWithName ("ProgressionLibrary");
    if (plTree.isValid())
    {
        ProgressionLibrary temp;
        temp.fromValueTree (plTree);
        result.progressions = temp.getAllProgressions();
        result.progressionFolders = temp.getFolders().getAllFolders();
    }

    // Parse melodies
    auto mlTree = root.getChildWithName ("MelodyLibrary");
    if (mlTree.isValid())
    {
        MelodyLibrary temp;
        temp.fromValueTree (mlTree);
        result.melodies = temp.getAllMelodies();
        result.melodyFolders = temp.getFolders().getAllFolders();
    }

    result.success = ! result.voicings.empty()
                  || ! result.progressions.empty()
                  || ! result.melodies.empty();
    return result;
}

LibraryExporter::MergeResult LibraryExporter::mergeIntoLibraries (
    const ImportResult& imported,
    VoicingLibrary& vl,
    ProgressionLibrary& pl,
    MelodyLibrary& ml)
{
    MergeResult result;

    // Merge voicings (skip duplicates by ID)
    for (const auto& v : imported.voicings)
    {
        if (vl.getVoicing (v.id) == nullptr)
        {
            vl.addVoicing (v);
            ++result.voicingsAdded;
        }
    }
    // Merge voicing folders
    for (const auto& f : imported.voicingFolders)
    {
        if (vl.getFolders().getFolder (f.id) == nullptr)
            vl.getFolders().addFolder (f);
    }

    // Merge progressions
    for (const auto& p : imported.progressions)
    {
        if (pl.getProgression (p.id) == nullptr)
        {
            pl.addProgression (p);
            ++result.progressionsAdded;
        }
    }
    for (const auto& f : imported.progressionFolders)
    {
        if (pl.getFolders().getFolder (f.id) == nullptr)
            pl.getFolders().addFolder (f);
    }

    // Merge melodies
    for (const auto& m : imported.melodies)
    {
        if (ml.getMelody (m.id) == nullptr)
        {
            ml.addMelody (m);
            ++result.melodiesAdded;
        }
    }
    for (const auto& f : imported.melodyFolders)
    {
        if (ml.getFolders().getFolder (f.id) == nullptr)
            ml.getFolders().addFolder (f);
    }

    return result;
}
