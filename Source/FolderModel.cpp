#include "FolderModel.h"
#include <algorithm>

static const juce::Identifier ID_Folders ("Folders");
static const juce::Identifier ID_Folder ("Folder");
static const juce::Identifier ID_id ("id");
static const juce::Identifier ID_name ("name");
static const juce::Identifier ID_sortOrder ("sortOrder");

void FolderLibrary::addFolder (const Folder& f)
{
    folders.push_back (f);
}

void FolderLibrary::removeFolder (const juce::String& id)
{
    folders.erase (
        std::remove_if (folders.begin(), folders.end(),
                        [&id] (const Folder& f) { return f.id == id; }),
        folders.end());
}

Folder* FolderLibrary::getFolder (const juce::String& id)
{
    for (auto& f : folders)
        if (f.id == id)
            return &f;
    return nullptr;
}

const Folder* FolderLibrary::getFolder (const juce::String& id) const
{
    for (const auto& f : folders)
        if (f.id == id)
            return &f;
    return nullptr;
}

juce::ValueTree FolderLibrary::toValueTree() const
{
    juce::ValueTree tree (ID_Folders);
    for (const auto& f : folders)
    {
        juce::ValueTree child (ID_Folder);
        child.setProperty (ID_id, f.id, nullptr);
        child.setProperty (ID_name, f.name, nullptr);
        child.setProperty (ID_sortOrder, f.sortOrder, nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

void FolderLibrary::fromValueTree (const juce::ValueTree& tree)
{
    folders.clear();
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild (i);
        if (child.hasType (ID_Folder))
        {
            Folder f;
            f.id = child.getProperty (ID_id).toString();
            f.name = child.getProperty (ID_name).toString();
            f.sortOrder = child.getProperty (ID_sortOrder, 0);
            folders.push_back (f);
        }
    }
}
