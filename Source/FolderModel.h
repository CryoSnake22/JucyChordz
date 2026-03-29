#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <vector>

struct Folder
{
    juce::String id;
    juce::String name;
    int sortOrder = 0;
};

class FolderLibrary
{
public:
    FolderLibrary() = default;

    void addFolder (const Folder& f);
    void removeFolder (const juce::String& id);
    Folder* getFolder (const juce::String& id);
    const Folder* getFolder (const juce::String& id) const;
    const std::vector<Folder>& getAllFolders() const { return folders; }
    int size() const { return static_cast<int> (folders.size()); }

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    std::vector<Folder> folders;
};
