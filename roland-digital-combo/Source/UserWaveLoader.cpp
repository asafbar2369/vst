#include "UserWaveLoader.h"

namespace userwaves
{
int leadingNumber (const juce::String& fileName)
{
    int i = 0, v = 0;
    while (i < fileName.length() && juce::CharacterFunctions::isDigit (fileName[i]) && i < 4)
        v = v * 10 + (fileName[i++] - '0');
    if (i == 0 || i >= 4) return -1; // no number, or an implausibly long one
    return v;
}

static int intMeta (const juce::StringPairArray& m, const juce::String& key, int def)
{
    return m.containsKey (key) ? m.getValue (key, {}).getIntValue() : def;
}

bool readWaveFile (juce::AudioFormatManager& fm, const juce::File& file, dc::SampleWave& out, juce::String& whyNot)
{
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { whyNot = "not a readable WAV/AIFF file"; return false; }
    if (reader->sampleRate < 4000.0 || reader->sampleRate > 384000.0) { whyNot = "unsupported sample rate"; return false; }
    if (reader->lengthInSamples < 2) { whyNot = "file is empty"; return false; }
    if (reader->lengthInSamples > (juce::int64) (reader->sampleRate * kMaxSecondsPerFile))
    {
        whyNot = "longer than " + juce::String (kMaxSecondsPerFile) + " s";
        return false;
    }

    const int len = (int) reader->lengthInSamples;
    const int chans = (int) juce::jlimit (1u, 8u, reader->numChannels);
    juce::AudioBuffer<float> buf (chans, len);
    if (! reader->read (&buf, 0, len, 0, true, chans > 1)) { whyNot = "read error"; return false; }

    out.data.assign ((size_t) len, 0.0f);
    for (int c = 0; c < chans; ++c)
        for (int i = 0; i < len; ++i)
            out.data[(size_t) i] += buf.getSample (c, i) / float (chans);
    for (auto& x : out.data)
        if (! std::isfinite (x)) x = 0.0f;

    out.rate = float (reader->sampleRate);
    const auto& meta = reader->metadataValues;
    const int unity = juce::jlimit (0, 127, intMeta (meta, "MidiUnityNote", 60));
    out.rootHz = dc::midiToHz (float (unity));

    // Sustain loop: WAV smpl gives sample positions (end inclusive); AIFF gives marker ids.
    out.loop = false;
    out.loopStart = out.loopEnd = 0;
    int ls = -1, le = -1;
    if (intMeta (meta, "NumSampleLoops", 0) > 0 && meta.containsKey ("Loop0Start"))
    {
        ls = intMeta (meta, "Loop0Start", -1);
        le = intMeta (meta, "Loop0End", -1) + 1;
    }
    else if (intMeta (meta, "Loop0Type", 0) != 0 && meta.containsKey ("Loop0StartIdentifier"))
    {
        const int startId = intMeta (meta, "Loop0StartIdentifier", -1), endId = intMeta (meta, "Loop0EndIdentifier", -1);
        for (int i = 0; i < intMeta (meta, "NumCuePoints", 0); ++i)
        {
            const int id = intMeta (meta, "Cue" + juce::String (i) + "Identifier", -2);
            const int off = intMeta (meta, "Cue" + juce::String (i) + "Offset", -1);
            if (id == startId) ls = off;
            if (id == endId) le = off;
        }
    }
    if (ls >= 0 && le > ls + 1 && le <= len)
    {
        out.loop = true;
        out.loopStart = ls;
        out.loopEnd = le;
    }
    return true;
}

Report loadFolder (const juce::File& folder, bool jd, dc::UserWaveSet& set)
{
    Report r;
    if (! folder.isDirectory())
    {
        if (folder != juce::File())
            r.messages.add ("Folder not found: " + folder.getFullPathName());
        return r;
    }
    juce::AudioFormatManager fm;
    fm.registerFormat (new juce::WavAudioFormat(), true);
    fm.registerFormat (new juce::AiffAudioFormat(), false);

    const int maxNumber = jd ? dc::UserWaveSet::kNumJd : dc::UserWaveSet::kNumD50;
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.WAV;*.aif;*.AIF;*.aiff;*.AIFF");
    files.sort();
    std::vector<bool> used ((size_t) maxNumber, false);
    for (const auto& f : files)
    {
        const int n = leadingNumber (f.getFileName());
        if (n < 1 || n > maxNumber)
        {
            r.messages.add (f.getFileName() + ": skipped (name must start with a number 1-" + juce::String (maxNumber) + ")");
            continue;
        }
        if (used[(size_t) (n - 1)])
        {
            r.messages.add (f.getFileName() + ": skipped (number " + juce::String (n) + " already loaded)");
            continue;
        }
        dc::SampleWave w;
        juce::String why;
        if (! readWaveFile (fm, f, w, why))
        {
            r.messages.add (f.getFileName() + ": skipped (" + why + ")");
            continue;
        }
        (jd ? set.jd : set.d50)[n - 1] = std::move (w);
        used[(size_t) (n - 1)] = true;
        ++r.loaded;
    }
    return r;
}
} // namespace userwaves
