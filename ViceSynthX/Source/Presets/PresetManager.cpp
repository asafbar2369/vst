#include "PresetManager.h"

namespace vsx
{
PresetManager::PresetManager()
{
    loadFavorites();
    rescan();
}

juce::File PresetManager::rootFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VICE SYNTH X");
    dir.createDirectory();
    return dir;
}

juce::File PresetManager::userFolder()
{
    auto d = rootFolder().getChildFile ("Presets").getChildFile ("User");
    d.createDirectory();
    return d;
}

juce::File PresetManager::aiFolder()
{
    auto d = rootFolder().getChildFile ("Presets").getChildFile ("AI Generated");
    d.createDirectory();
    return d;
}

juce::File PresetManager::recoveryFile()
{
    auto d = rootFolder().getChildFile ("Recovery");
    d.createDirectory();
    return d.getChildFile ("last_session" + PresetFormat::kExtension);
}

void PresetManager::loadFavorites()
{
    favorites.clear();
    const auto f = rootFolder().getChildFile ("favorites.json");
    if (auto* arr = juce::JSON::parse (f.loadFileAsString()).getArray())
        for (const auto& v : *arr) favorites.addIfNotAlreadyThere (v.toString());
}

void PresetManager::saveFavorites()
{
    juce::Array<juce::var> arr;
    for (const auto& f : favorites) arr.add (f);
    juce::String err;
    writeAtomically (rootFolder().getChildFile ("favorites.json"), juce::JSON::toString (arr), err);
}

void PresetManager::rescan()
{
    entries.clear();
    const auto& factory = FactoryPresets::all();
    for (size_t i = 0; i < factory.size(); ++i)
    {
        const auto& p = factory[i];
        Entry e;
        e.name = p.meta.name; e.category = p.meta.category; e.source = "factory"; e.author = p.meta.author;
        e.description = p.meta.description; e.tags = p.meta.tags; e.factoryIndex = (int) i;
        e.favorite = favorites.contains (e.key());
        entries.push_back (e);
    }

    auto scanFolder = [this] (const juce::File& dir, const juce::String& source)
    {
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*" + PresetFormat::kExtension))
        {
            juce::var v;
            if (juce::JSON::parse (f.loadFileAsString(), v).failed() || ! v.isObject()) continue;
            Entry e;
            const auto& m = v["meta"];
            e.name = m.getProperty ("name", f.getFileNameWithoutExtension()).toString();
            e.category = m.getProperty ("category", "").toString();
            e.source = m.getProperty ("source", source).toString();
            if (e.source == "factory") e.source = source;
            e.author = m.getProperty ("author", "").toString();
            e.description = m.getProperty ("description", "").toString();
            if (auto* tags = m["tags"].getArray())
                for (const auto& t : *tags) e.tags.add (t.toString());
            e.file = f;
            e.favorite = favorites.contains (e.key());
            entries.push_back (e);
        }
    };
    scanFolder (userFolder(), "user");
    scanFolder (aiFolder(), "ai");
}

std::vector<int> PresetManager::search (const juce::String& text, const juce::String& category, SourceFilter filter) const
{
    std::vector<int> result;
    const auto words = juce::StringArray::fromTokens (text.toLowerCase(), " ,", "");
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (category.isNotEmpty() && category != "All" && ! e.category.equalsIgnoreCase (category)) continue;
        switch (filter)
        {
            case SourceFilter::Factory: if (e.source != "factory") continue; break;
            case SourceFilter::User: if (e.source != "user") continue; break;
            case SourceFilter::AI: if (e.source != "ai") continue; break;
            case SourceFilter::Favorites: if (! e.favorite) continue; break;
            case SourceFilter::All: break;
        }
        const auto hay = (e.name + " " + e.category + " " + e.tags.joinIntoString (" ") + " " + e.description + " " + e.author).toLowerCase();
        bool ok = true;
        for (const auto& w : words)
            if (w.isNotEmpty() && ! hay.contains (w)) { ok = false; break; }
        if (ok) result.push_back ((int) i);
    }
    return result;
}

int PresetManager::indexOfName (const juce::String& name) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name.equalsIgnoreCase (name)) return (int) i;
    return -1;
}

bool PresetManager::load (const Entry& e, PatchSpec& out, juce::String& error) const
{
    if (e.factoryIndex >= 0)
    {
        const auto& f = FactoryPresets::all();
        if (e.factoryIndex >= (int) f.size()) { error = "Factory preset index out of range."; return false; }
        out = f[(size_t) e.factoryIndex];
        return true;
    }
    return loadFile (e.file, out, error);
}

bool PresetManager::loadFile (const juce::File& f, PatchSpec& out, juce::String& error) const
{
    if (! f.existsAsFile())
    {
        error = "Preset file not found: " + f.getFullPathName();
        return false;
    }
    if (PatchSpec::fromJson (f.loadFileAsString(), out, error))
        return true;

    // automatic recovery from the backup written on the previous save
    const auto bak = f.withFileExtension (".bak");
    juce::String err2;
    if (bak.existsAsFile() && PatchSpec::fromJson (bak.loadFileAsString(), out, err2))
    {
        error = "Preset was damaged; restored from backup.";
        return true;
    }
    return false;
}

bool PresetManager::writeAtomically (const juce::File& target, const juce::String& text, juce::String& error)
{
    target.getParentDirectory().createDirectory();
    juce::TemporaryFile temp (target);
    if (! temp.getFile().replaceWithText (text, false, false, "\n"))
    {
        error = "Cannot write " + temp.getFile().getFullPathName();
        return false;
    }
    if (target.existsAsFile())
        target.copyFileTo (target.withFileExtension (".bak"));
    if (! temp.overwriteTargetFileWithTemporary())
    {
        error = "Cannot replace " + target.getFullPathName();
        return false;
    }
    return true;
}

juce::File PresetManager::userFileFor (const juce::String& name, const juce::String& category)
{
    auto dir = userFolder();
    if (category.isNotEmpty()) dir = dir.getChildFile (juce::File::createLegalFileName (category));
    return dir.getChildFile (juce::File::createLegalFileName (name.isNotEmpty() ? name : juce::String ("Untitled")) + PresetFormat::kExtension);
}

juce::String PresetManager::uniqueUserName (const juce::String& name, const juce::String& category)
{
    if (! userPresetExists (name, category)) return name;
    for (int i = 2; i < 1000; ++i)
        if (! userPresetExists (name + " " + juce::String (i), category)) return name + " " + juce::String (i);
    return name + " " + juce::Time::getCurrentTime().formatted ("%H%M%S");
}

juce::File PresetManager::save (const PatchSpec& specIn, bool toAiFolder, juce::String& error)
{
    PatchSpec spec = specIn;
    spec.meta.modified = juce::Time::getCurrentTime();
    if (spec.meta.source == "factory") spec.meta.source = toAiFolder ? "ai" : "user";
    if (toAiFolder) spec.meta.source = "ai";
    const auto safeName = juce::File::createLegalFileName (spec.meta.name.isNotEmpty() ? spec.meta.name : juce::String ("Untitled"));
    auto dir = toAiFolder ? aiFolder() : userFolder();
    if (spec.meta.category.isNotEmpty())
        dir = dir.getChildFile (juce::File::createLegalFileName (spec.meta.category));
    const auto file = dir.getChildFile (safeName + PresetFormat::kExtension);
    if (! writeAtomically (file, spec.toJson(), error))
        return {};
    rescan();
    return file;
}

void PresetManager::setFavorite (const Entry& e, bool fav)
{
    if (fav) favorites.addIfNotAlreadyThere (e.key());
    else favorites.removeString (e.key());
    saveFavorites();
    for (auto& en : entries)
        if (en.key() == e.key()) en.favorite = fav;
}

bool PresetManager::exportPack (const PatchSpec& specIn, const juce::File& zipFile, juce::String& error) const
{
    PatchSpec spec = specIn;
    juce::ZipFile::Builder zip;
    std::vector<std::unique_ptr<juce::MemoryBlock>> keep;
    auto addFile = [&] (const juce::String& id, const juce::String& prefix, const juce::String& folder) -> juce::String
    {
        if (! id.startsWith (prefix)) return id;
        const juce::File f (id.fromFirstOccurrenceOf (prefix, false, false));
        if (! f.existsAsFile()) return id;
        zip.addFile (f, 9, folder + "/" + f.getFileName());
        return prefix + "@pack/" + folder + "/" + f.getFileName();
    };
    for (int o = 0; o < kNumOscs; ++o)
    {
        spec.wavetableId[(size_t) o] = addFile (spec.wavetableId[(size_t) o], "user:", "wavetables");
        spec.sampleId[(size_t) o] = addFile (spec.sampleId[(size_t) o], "file:", "samples");
        if (spec.multiId[(size_t) o].startsWith ("sfz:"))
        {
            // include the SFZ and every sample it references
            const juce::File sfz (spec.multiId[(size_t) o].fromFirstOccurrenceOf ("sfz:", false, false));
            if (sfz.existsAsFile())
            {
                const auto dir = sfz.getParentDirectory();
                for (const auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.sfz"))
                    zip.addFile (f, 9, "multisamples/" + sfz.getFileNameWithoutExtension() + "/" + f.getRelativePathFrom (dir).replaceCharacter ('\\', '/'));
                spec.multiId[(size_t) o] = "sfz:@pack/multisamples/" + sfz.getFileNameWithoutExtension() + "/" + sfz.getFileName();
            }
        }
    }
    if (spec.userIr.isNotEmpty() && juce::File (spec.userIr).existsAsFile())
    {
        const juce::File f (spec.userIr);
        zip.addFile (f, 9, "impulses/" + f.getFileName());
        spec.userIr = "@pack/impulses/" + f.getFileName();
    }

    auto json = std::make_unique<juce::MemoryBlock>();
    const auto text = spec.toJson();
    json->append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    zip.addEntry (new juce::MemoryInputStream (*json, false), 9, "preset" + PresetFormat::kExtension, juce::Time::getCurrentTime());
    keep.push_back (std::move (json));

    zipFile.deleteFile();
    juce::FileOutputStream out (zipFile);
    if (! out.openedOk() || ! zip.writeToStream (out, nullptr))
    {
        error = "Cannot write pack " + zipFile.getFullPathName();
        return false;
    }
    return true;
}

bool PresetManager::importPack (const juce::File& zipFile, PatchSpec& out, juce::String& error)
{
    juce::ZipFile zip (zipFile);
    if (zip.getNumEntries() == 0)
    {
        error = "Not a valid preset pack.";
        return false;
    }
    const auto dest = rootFolder().getChildFile ("Packs").getChildFile (zipFile.getFileNameWithoutExtension());
    dest.createDirectory();
    if (zip.uncompressTo (dest, true).failed())
    {
        error = "Could not extract the pack.";
        return false;
    }
    if (! loadFile (dest.getChildFile ("preset" + PresetFormat::kExtension), out, error))
        return false;
    auto fix = [&dest] (juce::String& id, const juce::String& prefix)
    {
        if (id.startsWith (prefix + "@pack/"))
            id = prefix + dest.getChildFile (id.fromFirstOccurrenceOf (prefix + "@pack/", false, false)).getFullPathName();
    };
    for (int o = 0; o < kNumOscs; ++o)
    {
        fix (out.wavetableId[(size_t) o], "user:");
        fix (out.sampleId[(size_t) o], "file:");
        fix (out.multiId[(size_t) o], "sfz:");
    }
    if (out.userIr.startsWith ("@pack/"))
        out.userIr = dest.getChildFile (out.userIr.fromFirstOccurrenceOf ("@pack/", false, false)).getFullPathName();
    out.meta.source = "user";
    juce::String err;
    save (out, false, err);
    return true;
}

void PresetManager::writeRecovery (const PatchSpec& spec)
{
    juce::String err;
    writeAtomically (recoveryFile(), spec.toJson(), err);
}

bool PresetManager::readRecovery (PatchSpec& out) const
{
    juce::String err;
    return recoveryFile().existsAsFile() && PatchSpec::fromJson (recoveryFile().loadFileAsString(), out, err);
}

} // namespace vsx
