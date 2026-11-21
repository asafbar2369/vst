#pragma once

#include "PresetFormat.h"

namespace vsx
{
namespace FactoryPresets
{
    /** All factory presets (built from code; original sound design). */
    const std::vector<PatchSpec>& all();
    juce::StringArray categories();
}

/**
    Preset library: factory (built in), user and AI-generated presets on disk, favourites,
    search/filter, atomic saving with backups, crash-recovery snapshots and portable packs.
*/
class PresetManager
{
public:
    struct Entry
    {
        juce::String name, category, source, author, description;
        juce::StringArray tags;
        juce::File file;          // empty for factory presets
        int factoryIndex = -1;
        bool favorite = false;
        juce::String key() const { return factoryIndex >= 0 ? "factory:" + name : "file:" + file.getFullPathName(); }
    };

    enum class SourceFilter { All, Factory, User, AI, Favorites };

    PresetManager();

    static juce::File rootFolder();
    static juce::File userFolder();
    static juce::File aiFolder();
    static juce::File recoveryFile();

    void rescan();
    const std::vector<Entry>& getEntries() const { return entries; }
    std::vector<int> search (const juce::String& text, const juce::String& category, SourceFilter filter) const;

    bool load (const Entry& e, PatchSpec& out, juce::String& error) const;
    bool loadFile (const juce::File& f, PatchSpec& out, juce::String& error) const;

    /** Saves to the user (or AI) folder; returns the written file. */
    juce::File save (const PatchSpec& spec, bool aiFolderTarget, juce::String& error);
    /** The file a user preset with this name and category is stored in (factory presets are built in, never files). */
    static juce::File userFileFor (const juce::String& name, const juce::String& category);
    static bool userPresetExists (const juce::String& name, const juce::String& category) { return userFileFor (name, category).existsAsFile(); }
    /** name, "name 2", "name 3", ... : the first name not used by a user preset in that category. */
    static juce::String uniqueUserName (const juce::String& name, const juce::String& category);
    static bool writeAtomically (const juce::File& target, const juce::String& text, juce::String& error);

    void setFavorite (const Entry& e, bool fav);
    bool isFavorite (const juce::String& key) const { return favorites.contains (key); }

    /** Portable pack: preset + referenced sample/wavetable/IR files in one zip. */
    bool exportPack (const PatchSpec& spec, const juce::File& zipFile, juce::String& error) const;
    bool importPack (const juce::File& zipFile, PatchSpec& out, juce::String& error);

    void writeRecovery (const PatchSpec& spec);
    bool readRecovery (PatchSpec& out) const;

    int indexOfName (const juce::String& name) const;

private:
    void loadFavorites();
    void saveFavorites();
    std::vector<Entry> entries;
    juce::StringArray favorites;
};

} // namespace vsx
