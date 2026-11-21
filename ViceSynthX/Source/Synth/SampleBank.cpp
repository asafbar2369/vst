#include "SampleBank.h"
#include <limits>
#include "../Analysis/Pitch.h"
#include "../Core/DspUtils.h"
#include <juce_dsp/juce_dsp.h>

namespace vsx
{
namespace
{
    /** 2x decimation with a 63-tap Blackman-windowed sinc low-pass (cutoff 0.22 fs_in). */
    juce::AudioBuffer<float> decimate2 (const juce::AudioBuffer<float>& in, int guard)
    {
        static const std::vector<float> taps = []
        {
            const int n = 63;
            std::vector<float> t ((size_t) n);
            const double fc = 0.22;
            double sum = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double m = i - (n - 1) / 2.0;
                const double sinc = m == 0.0 ? 2.0 * fc : std::sin (2.0 * kPiD * fc * m) / (kPiD * m);
                const double w = 0.42 - 0.5 * std::cos (2.0 * kPiD * i / (n - 1)) + 0.08 * std::cos (4.0 * kPiD * i / (n - 1));
                t[(size_t) i] = (float) (sinc * w);
                sum += sinc * w;
            }
            for (auto& v : t) v = (float) (v / sum);
            return t;
        }();

        const int inLen = in.getNumSamples() - 2 * guard;
        const int outLen = std::max (1, inLen / 2);
        juce::AudioBuffer<float> out (in.getNumChannels(), outLen + 2 * guard);
        out.clear();
        const int half = (int) taps.size() / 2;
        for (int c = 0; c < in.getNumChannels(); ++c)
        {
            const float* src = in.getReadPointer (c) + guard;
            float* dst = out.getWritePointer (c) + guard;
            for (int i = 0; i < outLen; ++i)
            {
                const int centre = i * 2;
                float acc = 0.0f;
                for (int t = 0; t < (int) taps.size(); ++t)
                {
                    const int idx = centre + t - half;
                    if (idx >= 0 && idx < inLen)
                        acc += src[idx] * taps[(size_t) t];
                }
                dst[i] = acc;
            }
        }
        return out;
    }

    void analyseSpectrum (SampleData& s)
    {
        const int len = s.length;
        if (len < 512) return;

        std::vector<float> mono ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < s.numChannels; ++c) v += s.channel (0, c)[i];
            mono[(size_t) i] = v / (float) s.numChannels;
        }

        const int yinLen = std::min (len, 8192);
        const int yinStart = std::max (0, std::min (len - yinLen, (int) (0.1 * s.sampleRate)));
        float conf = 0.0f;
        const float period = yinPeriod (mono.data() + yinStart, yinLen, s.sampleRate, 30.0f, 2000.0f, 0.15f, &conf);
        s.analysedF0 = period > 0.0f ? (float) (s.sampleRate / period) : 0.0f;

        const int order = 12;
        const int fftSize = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> window ((size_t) fftSize);
        float winSum = 0.0f;
        for (int i = 0; i < fftSize; ++i)
        {
            window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) (fftSize - 1));
            winSum += window[(size_t) i];
        }

        int hop = 512;
        while (len / hop > 1024) hop *= 2;
        const int numFrames = std::max (1, (len - fftSize / 2) / hop);
        s.numSpecFrames = numFrames;
        s.specHopSeconds = hop / s.sampleRate;
        s.specAmps.assign ((size_t) numFrames * SampleData::kSpecPartials, 0.0f);

        const float binHz = (float) (s.sampleRate / fftSize);
        const float f0 = s.analysedF0 > 0.0f ? s.analysedF0 : midiToHz ((float) s.rootNote);
        const bool pitched = s.analysedF0 > 0.0f;
        std::vector<float> buf ((size_t) fftSize * 2);
        float maxRms = 1.0e-9f;

        for (int f = 0; f < numFrames; ++f)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            const int start = f * hop - fftSize / 2;
            for (int i = 0; i < fftSize; ++i)
            {
                const int idx = start + i;
                buf[(size_t) i] = (idx >= 0 && idx < len) ? mono[(size_t) idx] * window[(size_t) i] : 0.0f;
            }
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);

            float energy = 0.0f;
            for (int k = 1; k <= SampleData::kSpecPartials; ++k)
            {
                const float freq = f0 * (float) k;
                float amp = 0.0f;
                if (freq < s.sampleRate * 0.5 - binHz)
                {
                    const float bin = freq / binHz;
                    if (pitched)
                    {
                        const int b0 = std::max (1, (int) bin - 1), b1 = std::min (fftSize / 2 - 1, (int) bin + 2);
                        for (int b = b0; b <= b1; ++b) amp = std::max (amp, buf[(size_t) b]);
                    }
                    else
                    {
                        const int b0 = std::max (1, (int) (bin - f0 * 0.5f / binHz)), b1 = std::min (fftSize / 2 - 1, (int) (bin + f0 * 0.5f / binHz));
                        float sum = 0.0f; int cnt = 0;
                        for (int b = b0; b <= b1; ++b) { sum += buf[(size_t) b]; ++cnt; }
                        amp = cnt > 0 ? sum / (float) cnt : 0.0f;
                    }
                    amp *= 2.0f / winSum;
                }
                s.specAmps[(size_t) f * SampleData::kSpecPartials + (size_t) (k - 1)] = amp;
                energy += amp * amp;
            }
            maxRms = std::max (maxRms, std::sqrt (energy * 0.5f));
        }

        const float g = 0.35f / maxRms;
        for (auto& a : s.specAmps) a *= g;
    }
}

std::shared_ptr<SampleData> SampleData::create (juce::AudioBuffer<float> audio, double sampleRate, const juce::String& name,
                                                const juce::String& sourceId, int rootNote)
{
    auto s = std::make_shared<SampleData>();
    s->name = name;
    s->sourceId = sourceId;
    s->sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    s->numChannels = juce::jlimit (1, 2, audio.getNumChannels());
    s->length = audio.getNumSamples();
    s->rootNote = rootNote;
    s->loopStart = 0;
    s->loopEnd = s->length;

    juce::AudioBuffer<float> level0 (s->numChannels, s->length + 2 * kGuard);
    level0.clear();
    for (int c = 0; c < s->numChannels; ++c)
        level0.copyFrom (c, kGuard, audio, c, 0, s->length);
    s->mips.push_back (std::move (level0));

    for (int l = 1; l < kMaxMips && s->mips.back().getNumSamples() - 2 * kGuard > 256; ++l)
        s->mips.push_back (decimate2 (s->mips.back(), kGuard));

    analyseSpectrum (*s);
    return s;
}

std::shared_ptr<SampleData> SampleData::fromFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Cannot read audio file: " + file.getFullPathName();
        return nullptr;
    }

    if (! std::isfinite (reader->sampleRate) || reader->sampleRate <= 0.0
        || reader->numChannels == 0 || reader->numChannels > 2)
    {
        error = "Sample must be valid mono or stereo audio: " + file.getFileName();
        return nullptr;
    }
    const auto maxLen = (juce::int64) (reader->sampleRate * 120.0);
    if (reader->lengthInSamples > maxLen || reader->lengthInSamples > std::numeric_limits<int>::max())
    {
        error = "Sample exceeds the 120-second import limit; no audio was truncated: " + file.getFileName();
        return nullptr;
    }
    const int len = (int) std::min (reader->lengthInSamples, maxLen);
    if (len <= 0)
    {
        error = "Audio file is empty: " + file.getFileName();
        return nullptr;
    }

    juce::AudioBuffer<float> buf ((int) std::min<unsigned int> (2, reader->numChannels), len);
    if (! reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1))
    {
        error = "Audio decoding failed: " + file.getFileName();
        return nullptr;
    }
    for (int channel = 0; channel < buf.getNumChannels(); ++channel)
        for (int frame = 0; frame < len; ++frame)
            if (! std::isfinite (buf.getSample (channel, frame)))
            {
                error = "Sample contains non-finite audio: " + file.getFileName();
                return nullptr;
            }

    const auto& md = reader->metadataValues;
    int root = -1;
    if (md.containsKey ("MidiUnityNote"))
        root = md["MidiUnityNote"].getIntValue();

    auto s = create (std::move (buf), reader->sampleRate, file.getFileNameWithoutExtension(), "file:" + file.getFullPathName(),
                     root >= 0 ? root : 60);

    if (root < 0)
    {
        // derive the root key from the file name or detected pitch
        const int fromName = Sfz::parseNoteName (file.getFileNameWithoutExtension().fromLastOccurrenceOf ("_", false, false));
        if (fromName >= 0) s->rootNote = fromName;
        else if (s->analysedF0 > 0.0f)
        {
            const float m = hzToMidi (s->analysedF0);
            s->rootNote = juce::jlimit (0, 127, (int) std::round (m));
            s->fineTuneCents = (m - (float) s->rootNote) * 100.0f;
        }
    }

    if (md.getValue ("NumSampleLoops", "0").getIntValue() > 0)
    {
        s->hasLoop = true;
        s->loopStart = juce::jlimit (0, s->length - 1, md["Loop0Start"].getIntValue());
        s->loopEnd = juce::jlimit (s->loopStart + 1, s->length, md["Loop0End"].getIntValue() + 1);
    }
    return s;
}

// ---------------------------------------------------------------------------------
const SampleZone* MultiSample::find (int note, int velocity) const noexcept
{
    const SampleZone* best = nullptr;
    int bestDist = 1 << 30;
    for (const auto& z : zones)
    {
        if (z.sample == nullptr) continue;
        const bool velOk = velocity >= z.loVel && velocity <= z.hiVel;
        int dist = 0;
        if (note < z.loKey) dist = z.loKey - note;
        else if (note > z.hiKey) dist = note - z.hiKey;
        dist = dist * 2 + (velOk ? 0 : 1000);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &z;
            if (dist == 0) break;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------
namespace Sfz
{
int parseNoteName (const juce::String& textIn)
{
    const auto text = textIn.trim().toLowerCase();
    if (text.isEmpty()) return -1;
    if (text.containsOnly ("0123456789"))
    {
        const int v = text.getIntValue();
        return v >= 0 && v <= 127 ? v : -1;
    }

    // find a note letter followed by optional accidental and octave number
    for (int i = 0; i < text.length(); ++i)
    {
        const auto c = text[i];
        const juce::String letters = "cdefgab";
        const int li = letters.indexOfChar (c);
        if (li < 0) continue;
        static const int semis[] = { 0, 2, 4, 5, 7, 9, 11 };
        int note = semis[li];
        int j = i + 1;
        if (j < text.length() && (text[j] == '#' || text[j] == 's')) { ++note; ++j; }
        else if (j < text.length() && text[j] == 'b' && j + 1 < text.length() && (juce::CharacterFunctions::isDigit (text[j + 1]) || text[j + 1] == '-')) { --note; ++j; }
        int sign = 1;
        if (j < text.length() && text[j] == '-') { sign = -1; ++j; }
        if (j < text.length() && juce::CharacterFunctions::isDigit (text[j]))
        {
            const int octave = sign * (text[j] - '0');
            const int midi = (octave + 1) * 12 + note;
            if (midi >= 0 && midi <= 127) return midi;
        }
    }
    return -1;
}

namespace
{
    using Opcodes = std::map<juce::String, juce::String>;

    juce::String stripComments (const juce::String& in)
    {
        juce::String out;
        auto lines = juce::StringArray::fromLines (in);
        bool inBlock = false;
        for (auto line : lines)
        {
            juce::String result;
            int i = 0;
            while (i < line.length())
            {
                if (inBlock)
                {
                    const int end = line.indexOf (i, "*/");
                    if (end < 0) { i = line.length(); break; }
                    inBlock = false; i = end + 2; continue;
                }
                const int lc = line.indexOf (i, "//");
                const int bc = line.indexOf (i, "/*");
                if (bc >= 0 && (lc < 0 || bc < lc))
                {
                    result += line.substring (i, bc);
                    inBlock = true; i = bc + 2; continue;
                }
                if (lc >= 0) { result += line.substring (i, lc); break; }
                result += line.substring (i);
                break;
            }
            out << result << "\n";
        }
        return out;
    }

    struct Token { bool isHeader; juce::String key, value; };

    std::vector<Token> tokenise (const juce::String& text)
    {
        std::vector<Token> tokens;
        for (const auto& rawLine : juce::StringArray::fromLines (text))
        {
            const juce::String line = rawLine;
            int pos = 0;
            const int len = line.length();
            while (pos < len)
            {
                while (pos < len && juce::CharacterFunctions::isWhitespace (line[pos])) ++pos;
                if (pos >= len) break;
                if (line[pos] == '<')
                {
                    const int end = line.indexOfChar (pos, '>');
                    if (end < 0) break;
                    tokens.push_back ({ true, line.substring (pos + 1, end).trim().toLowerCase(), {} });
                    pos = end + 1;
                    continue;
                }
                const int eq = line.indexOfChar (pos, '=');
                if (eq < 0) break;
                const auto key = line.substring (pos, eq).trim().toLowerCase();

                int valueEnd = len;
                const int nextHeader = line.indexOfChar (eq + 1, '<');
                const int nextEq = line.indexOfChar (eq + 1, '=');
                if (nextEq >= 0 && (nextHeader < 0 || nextEq < nextHeader))
                {
                    int k = nextEq - 1;
                    while (k > eq && ! juce::CharacterFunctions::isWhitespace (line[k])) --k;
                    valueEnd = k;
                }
                else if (nextHeader >= 0)
                    valueEnd = nextHeader;

                tokens.push_back ({ false, key, line.substring (eq + 1, valueEnd).trim() });
                pos = std::max (valueEnd, eq + 1);
            }
        }
        return tokens;
    }

    int noteOpcode (const juce::String& v, int fallback)
    {
        const int n = parseNoteName (v);
        return n >= 0 ? n : fallback;
    }
}

std::shared_ptr<MultiSample> load (const juce::File& sfzFile, juce::String& error)
{
    if (! sfzFile.existsAsFile())
    {
        error = "SFZ file not found: " + sfzFile.getFullPathName();
        return nullptr;
    }

    const auto tokens = tokenise (stripComments (sfzFile.loadFileAsString()));
    auto ms = std::make_shared<MultiSample>();
    ms->name = sfzFile.getFileNameWithoutExtension();
    ms->sourceId = "sfz:" + sfzFile.getFullPathName();

    Opcodes control, global, master, group, region;
    juce::String level;
    std::map<juce::String, std::shared_ptr<const SampleData>> loaded;

    auto flushRegion = [&]
    {
        if (level != "region") return;
        Opcodes merged = global;
        for (auto& kv : master) merged[kv.first] = kv.second;
        for (auto& kv : group) merged[kv.first] = kv.second;
        for (auto& kv : region) merged[kv.first] = kv.second;

        auto samplePath = merged["sample"].replaceCharacter ('\\', '/');
        if (samplePath.isEmpty()) return;
        auto defaultPath = control["default_path"].replaceCharacter ('\\', '/');
        const auto file = sfzFile.getParentDirectory().getChildFile (defaultPath + samplePath);

        std::shared_ptr<const SampleData> sample;
        auto it = loaded.find (file.getFullPathName());
        if (it != loaded.end()) sample = it->second;
        else
        {
            juce::String err;
            if (file.existsAsFile())
                sample = SampleData::fromFile (file, err);
            loaded[file.getFullPathName()] = sample;
            if (sample == nullptr)
                ms->missingFiles.addIfNotAlreadyThere (file.getFullPathName());
        }
        if (sample == nullptr) return;

        SampleZone z;
        z.sample = sample;
        if (merged.count ("key"))
        {
            const int k = noteOpcode (merged["key"], 60);
            z.loKey = z.hiKey = z.rootNote = k;
        }
        z.loKey = noteOpcode (merged.count ("lokey") ? merged["lokey"] : juce::String(), z.loKey);
        z.hiKey = noteOpcode (merged.count ("hikey") ? merged["hikey"] : juce::String(), z.hiKey);
        z.rootNote = noteOpcode (merged.count ("pitch_keycenter") ? merged["pitch_keycenter"] : juce::String(),
                                 merged.count ("key") ? z.rootNote : sample->rootNote);
        if (merged.count ("lovel")) z.loVel = merged["lovel"].getIntValue();
        if (merged.count ("hivel")) z.hiVel = merged["hivel"].getIntValue();
        if (merged.count ("tune")) z.tuneCents = merged["tune"].getFloatValue();
        if (merged.count ("transpose")) z.rootNote -= merged["transpose"].getIntValue();
        if (merged.count ("volume")) z.gainDb = merged["volume"].getFloatValue();
        if (merged.count ("pan")) z.pan = juce::jlimit (-1.0f, 1.0f, merged["pan"].getFloatValue() / 100.0f);
        if (merged.count ("offset")) z.offset = merged["offset"].getIntValue();
        if (merged.count ("loop_mode") || merged.count ("loopmode"))
        {
            const auto m = merged.count ("loop_mode") ? merged["loop_mode"] : merged["loopmode"];
            z.loopMode = (m == "loop_continuous" || m == "loop_sustain") ? 1 : 0;
        }
        const auto ls = merged.count ("loop_start") ? merged["loop_start"] : merged["loopstart"];
        const auto le = merged.count ("loop_end") ? merged["loop_end"] : merged["loopend"];
        if (ls.isNotEmpty()) z.loopStart = ls.getIntValue();
        if (le.isNotEmpty()) z.loopEnd = le.getIntValue() + 1;
        ms->zones.push_back (z);
    };

    for (const auto& t : tokens)
    {
        if (t.isHeader)
        {
            flushRegion();
            level = t.key;
            if (level == "region") region.clear();
            else if (level == "group") { group.clear(); region.clear(); }
            else if (level == "master") { master.clear(); group.clear(); }
            else if (level == "global") { global.clear(); master.clear(); group.clear(); }
            continue;
        }
        if (level == "control") control[t.key] = t.value;
        else if (level == "global") global[t.key] = t.value;
        else if (level == "master") master[t.key] = t.value;
        else if (level == "group") group[t.key] = t.value;
        else if (level == "region") region[t.key] = t.value;
    }
    flushRegion();

    if (ms->zones.empty())
    {
        error = ms->missingFiles.isEmpty() ? "SFZ contains no playable regions."
                                           : "All SFZ samples are missing (" + juce::String (ms->missingFiles.size()) + " files).";
        return nullptr;
    }
    if (! ms->missingFiles.isEmpty())
        error = juce::String (ms->missingFiles.size()) + " sample file(s) referenced by the SFZ are missing.";
    return ms;
}
} // namespace Sfz

std::shared_ptr<MultiSample> multisampleFromFiles (const juce::Array<juce::File>& files, juce::String& error)
{
    struct Entry { juce::File file; int root; };
    std::vector<Entry> entries;
    juce::Array<juce::var> paths;
    if (files.isEmpty() || files.size() > 128)
    {
        error = "Select between 1 and 128 samples with distinct root notes in their names.";
        return nullptr;
    }
    for (const auto& f : files)
    {
        const int n = Sfz::parseNoteName (f.getFileNameWithoutExtension().replaceCharacters ("-_ ", "   ").fromLastOccurrenceOf (" ", false, false));
        const int root = n >= 0 ? n : Sfz::parseNoteName (f.getFileNameWithoutExtension());
        if (root < 0 || root > 127)
        {
            error = "Include a root note in each filename (for example Piano_C4.wav), or use SFZ: " + f.getFileName();
            return nullptr;
        }
        entries.push_back ({ f, root });
        paths.add (f.getFullPathName());
    }
    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.root < b.root; });
    auto ms = std::make_shared<MultiSample>();
    ms->name = "Imported multisample";
    ms->sourceId = "files:" + juce::JSON::toString (juce::var (paths), true);
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0 && entries[i].root == entries[i - 1].root)
        {
            error = "Duplicate root notes need explicit SFZ velocity zones.";
            return nullptr;
        }
        auto sample = SampleData::fromFile (entries[i].file, error);
        if (sample == nullptr) return nullptr; // Keep the previous instrument on any failed region.
        SampleZone zone;
        zone.sample = sample;
        zone.rootNote = entries[i].root;
        zone.loKey = i == 0 ? 0 : (entries[i - 1].root + entries[i].root) / 2 + 1;
        zone.hiKey = i + 1 == entries.size() ? 127 : (entries[i].root + entries[i + 1].root) / 2;
        ms->zones.push_back (zone);
    }
    return ms;
}

std::shared_ptr<MultiSample> multisampleFromFolder (const juce::File& folder, juce::String& error)
{
    const auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
    auto result = multisampleFromFiles (files, error);
    if (result != nullptr)
    {
        result->name = folder.getFileName();
        result->sourceId = "folder:" + folder.getFullPathName();
    }
    return result;
}

// ---------------------------------------------------------------------------------
SampleLibrary& SampleLibrary::instance()
{
    static SampleLibrary lib;
    return lib;
}

juce::File SampleLibrary::userFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VICE SYNTH X").getChildFile ("Samples");
    dir.createDirectory();
    return dir;
}

std::shared_ptr<const SampleData> SampleLibrary::loadSample (const juce::String& sourceId, juce::String& error)
{
    {
        std::lock_guard<std::mutex> lock (mutex);
        auto it = samples.find (sourceId);
        if (it != samples.end() && sourceId.startsWith ("builtin:")) return it->second;
    }
    std::shared_ptr<const SampleData> s;
    if (sourceId.startsWith ("file:"))
    {
        const juce::File f (sourceId.fromFirstOccurrenceOf ("file:", false, false));
        if (! f.existsAsFile()) { error = "Missing sample: " + f.getFullPathName(); return nullptr; }
        s = SampleData::fromFile (f, error);
    }
    else if (sourceId.startsWith ("builtin:"))
    {
        const auto name = sourceId.fromFirstOccurrenceOf ("builtin:", false, false);
        s = BuiltinSamples::renderSingle (name);
        if (s == nullptr) error = "Unknown built-in sample: " + name;
    }
    else
        error = "Unknown sample source: " + sourceId;

    if (s != nullptr)
    {
        std::lock_guard<std::mutex> lock (mutex);
        samples[sourceId] = s;
    }
    return s;
}

std::shared_ptr<const MultiSample> SampleLibrary::loadMulti (const juce::String& sourceId, juce::String& error)
{
    {
        std::lock_guard<std::mutex> lock (mutex);
        auto it = multis.find (sourceId);
        if (it != multis.end() && sourceId.startsWith ("builtin:")) return it->second;
    }
    std::shared_ptr<const MultiSample> m;
    if (sourceId.startsWith ("files:"))
    {
        const auto list = juce::JSON::parse (sourceId.substring (6));
        juce::Array<juce::File> files;
        if (auto* paths = list.getArray())
            for (const auto& path : *paths) files.add (juce::File (path.toString()));
        m = multisampleFromFiles (files, error);
    }
    else if (sourceId.startsWith ("sfz:"))
        m = Sfz::load (juce::File (sourceId.fromFirstOccurrenceOf ("sfz:", false, false)), error);
    else if (sourceId.startsWith ("folder:"))
        m = multisampleFromFolder (juce::File (sourceId.fromFirstOccurrenceOf ("folder:", false, false)), error);
    else if (sourceId.startsWith ("builtin:"))
    {
        const auto name = sourceId.fromFirstOccurrenceOf ("builtin:", false, false);
        m = BuiltinSamples::renderMulti (name);
        if (m == nullptr) error = "Unknown built-in multisample: " + name;
    }
    else
        error = "Unknown multisample source: " + sourceId;

    if (m != nullptr)
    {
        std::lock_guard<std::mutex> lock (mutex);
        multis[sourceId] = m;
    }
    return m;
}

} // namespace vsx
