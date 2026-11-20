#include "Wavetable.h"
#include "../Analysis/Pitch.h"
#include "../Core/DspUtils.h"

namespace vsx
{
namespace
{
    std::vector<float> resampleCycle (const std::vector<float>& in, int outLen)
    {
        std::vector<float> out ((size_t) outLen, 0.0f);
        const int n = (int) in.size();
        if (n == 0) return out;
        if (n == outLen) return in;
        for (int i = 0; i < outLen; ++i)
        {
            const double pos = (double) i * n / outLen;
            const int i0 = (int) pos;
            const float fr = (float) (pos - i0);
            const float a = in[(size_t) (i0 % n)], b = in[(size_t) ((i0 + 1) % n)];
            out[(size_t) i] = a + fr * (b - a);
        }
        return out;
    }
}

std::shared_ptr<Wavetable> Wavetable::fromFrames (const std::vector<std::vector<float>>& inFrames, const juce::String& name,
                                                  const juce::String& sourceId, bool normaliseEachFrame)
{
    if (inFrames.empty())
        return nullptr;

    auto wt = std::make_shared<Wavetable>();
    wt->name = name;
    wt->sourceId = sourceId;
    wt->numFrames = (int) std::min<size_t> (inFrames.size(), (size_t) kMaxFrames);
    wt->data.assign ((size_t) wt->numFrames * kLevels * kStride, 0.0f);
    wt->partialMag.assign ((size_t) wt->numFrames * kPartials, 0.0f);
    wt->partialPhase.assign ((size_t) wt->numFrames * kPartials, 0.0f);
    wt->frames.resize ((size_t) wt->numFrames);

    juce::dsp::FFT fft (11);
    std::vector<float> spectrum ((size_t) kSize * 2), work ((size_t) kSize * 2);

    // global normalisation if requested per table
    float tableGain = 1.0f;
    if (! normaliseEachFrame)
    {
        float peak = 0.0f;
        for (int f = 0; f < wt->numFrames; ++f)
            for (float v : inFrames[(size_t) f]) peak = std::max (peak, std::abs (v));
        tableGain = peak > 1.0e-6f ? 0.98f / peak : 1.0f;
    }

    for (int f = 0; f < wt->numFrames; ++f)
    {
        auto frame = resampleCycle (inFrames[(size_t) f], kSize);
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        std::copy (frame.begin(), frame.end(), spectrum.begin());
        fft.performRealOnlyForwardTransform (spectrum.data(), true);
        spectrum[0] = spectrum[1] = 0.0f;                       // remove DC
        spectrum[(size_t) kSize] = spectrum[(size_t) kSize + 1] = 0.0f; // remove Nyquist

        float gain = tableGain;
        for (int level = 0; level < kLevels; ++level)
        {
            const int maxHarm = std::min (1023, 1024 >> level);
            std::copy (spectrum.begin(), spectrum.begin() + kSize + 2, work.begin());
            for (int k = maxHarm + 1; k <= kSize / 2; ++k)
                work[(size_t) (2 * k)] = work[(size_t) (2 * k + 1)] = 0.0f;
            fft.performRealOnlyInverseTransform (work.data());

            if (level == 0 && normaliseEachFrame)
            {
                float peak = 0.0f;
                for (int i = 0; i < kSize; ++i) peak = std::max (peak, std::abs (work[(size_t) i]));
                gain = peak > 1.0e-6f ? 0.98f / peak : 0.0f;
            }

            float* dst = wt->data.data() + ((size_t) f * kLevels + (size_t) level) * kStride;
            for (int i = 0; i < kSize; ++i)
                dst[i] = work[(size_t) i] * gain;
            for (int i = 0; i < kStride - kSize; ++i)
                dst[kSize + i] = dst[i];

            if (level == 0)
                wt->frames[(size_t) f].assign (dst, dst + kSize);
        }

        for (int k = 1; k <= kPartials; ++k)
        {
            const float re = spectrum[(size_t) (2 * k)], im = spectrum[(size_t) (2 * k + 1)];
            wt->partialMag[(size_t) f * kPartials + (size_t) (k - 1)] = 2.0f * std::sqrt (re * re + im * im) / (float) kSize * gain;
            wt->partialPhase[(size_t) f * kPartials + (size_t) (k - 1)] = std::atan2 (im, re);
        }
    }
    return wt;
}

std::shared_ptr<Wavetable> Wavetable::fromHarmonics (const std::vector<std::vector<float>>& amps, const juce::String& name,
                                                     const juce::String& sourceId)
{
    juce::dsp::FFT fft (11);
    std::vector<std::vector<float>> frames;
    std::vector<float> buf ((size_t) kSize * 2);
    for (const auto& a : amps)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (size_t k = 0; k < a.size() && k + 1 < (size_t) kSize / 2; ++k)
        {
            const size_t h = k + 1;
            buf[2 * h] = 0.0f;
            buf[2 * h + 1] = -(float) kSize * 0.5f * a[k]; // sine phase
        }
        fft.performRealOnlyInverseTransform (buf.data());
        frames.emplace_back (buf.begin(), buf.begin() + kSize);
    }
    return fromFrames (frames, name, sourceId, true);
}

std::shared_ptr<Wavetable> Wavetable::fromAudio (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name,
                                                 const juce::String& sourceId, juce::String& error)
{
    const int len = audio.getNumSamples();
    if (len < 16)
    {
        error = "Audio is too short to build a wavetable.";
        return nullptr;
    }

    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < audio.getNumChannels(); ++c)
    {
        const float* src = audio.getReadPointer (c);
        for (int i = 0; i < len; ++i) mono[(size_t) i] += src[i] / (float) audio.getNumChannels();
    }

    std::vector<std::vector<float>> frames;
    if (len % kSize == 0 && len / kSize <= kMaxFrames)
    {
        for (int f = 0; f < len / kSize; ++f)
            frames.emplace_back (mono.begin() + f * kSize, mono.begin() + (f + 1) * kSize);
    }
    else if (len <= 4096)
    {
        frames.push_back (mono);
    }
    else
    {
        const int analysisLen = std::min (len, 8192);
        const int start = std::max (0, len / 2 - analysisLen / 2);
        const float period = yinPeriod (mono.data() + start, analysisLen, sampleRate, 25.0f, 2000.0f);
        const int numFrames = 64;
        if (period > 4.0f)
        {
            const int p = (int) std::round (period);
            for (int f = 0; f < numFrames; ++f)
            {
                int pos = (int) ((double) f / (numFrames - 1) * (len - p - 2));
                // align to an upward zero crossing for continuity
                for (int s = 0; s < p && pos + s + 1 < len - p; ++s)
                    if (mono[(size_t) (pos + s)] <= 0.0f && mono[(size_t) (pos + s + 1)] > 0.0f) { pos += s + 1; break; }
                pos = std::min (pos, len - p - 1);
                std::vector<float> cyc (mono.begin() + pos, mono.begin() + pos + p);
                // enforce cyclic continuity with a short crossfade towards the start
                const int xf = std::min (32, p / 8);
                for (int i = 0; i < xf; ++i)
                {
                    const float t = (float) (i + 1) / (float) (xf + 1);
                    auto& v = cyc[(size_t) (p - xf + i)];
                    v = v * (1.0f - t) + cyc[(size_t) i] * t;
                }
                frames.push_back (resampleCycle (cyc, kSize));
            }
        }
        else
        {
            const int chunk = std::max (64, len / numFrames);
            for (int f = 0; f < numFrames && (f + 1) * chunk <= len; ++f)
                frames.push_back (resampleCycle (std::vector<float> (mono.begin() + f * chunk, mono.begin() + (f + 1) * chunk), kSize));
        }
    }

    if (frames.empty())
    {
        error = "Could not extract any cycles from the audio.";
        return nullptr;
    }
    return fromFrames (frames, name, sourceId, true);
}

std::shared_ptr<Wavetable> Wavetable::fromAudioFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + file.getFileName();
        return nullptr;
    }
    const auto len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) kSize * kMaxFrames);
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    return fromAudio (buf, reader->sampleRate, file.getFileNameWithoutExtension(), "user:" + file.getFullPathName(), error);
}

bool Wavetable::exportToWav (const juce::File& file) const
{
    juce::AudioBuffer<float> buf (1, numFrames * kSize);
    for (int f = 0; f < numFrames; ++f)
        buf.copyFrom (0, f * kSize, frames[(size_t) f].data(), kSize);

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (44100.0)
                                                   .withNumChannels (1).withBitsPerSample (32)
                                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (writer == nullptr) return false;
    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

// ---------------------------------------------------------------------------------
WavetableLibrary& WavetableLibrary::instance()
{
    static WavetableLibrary lib;
    return lib;
}

juce::File WavetableLibrary::userFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("VICE SYNTH X").getChildFile ("Wavetables");
    dir.createDirectory();
    return dir;
}

juce::StringArray WavetableLibrary::userNames()
{
    juce::StringArray result;
    for (const auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff"))
        result.add (f.getFullPathName());
    result.sort (true);
    return result;
}

std::shared_ptr<const Wavetable> WavetableLibrary::factory (const juce::String& name)
{
    juce::String err;
    return load ("factory:" + name, err);
}

std::shared_ptr<const Wavetable> WavetableLibrary::load (const juce::String& sourceId, juce::String& error)
{
    {
        std::lock_guard<std::mutex> lock (mutex);
        auto it = cache.find (sourceId);
        if (it != cache.end())
            return it->second;
    }

    std::shared_ptr<const Wavetable> wt;
    if (sourceId.startsWith ("factory:"))
    {
        const auto name = sourceId.fromFirstOccurrenceOf ("factory:", false, false);
        if (! WavetableFactory::names().contains (name))
        {
            error = "Unknown factory wavetable: " + name;
            return nullptr;
        }
        wt = Wavetable::fromFrames (WavetableFactory::generateFrames (name), name, sourceId, true);
    }
    else if (sourceId.startsWith ("user:"))
    {
        const juce::File file (sourceId.fromFirstOccurrenceOf ("user:", false, false));
        if (! file.existsAsFile())
        {
            error = "Missing wavetable file: " + file.getFullPathName();
            return nullptr;
        }
        wt = Wavetable::fromAudioFile (file, error);
    }
    else
    {
        error = "Unknown wavetable source: " + sourceId;
        return nullptr;
    }

    if (wt != nullptr)
    {
        std::lock_guard<std::mutex> lock (mutex);
        cache[sourceId] = wt;
    }
    return wt;
}

void WavetableLibrary::storeCustom (std::shared_ptr<const Wavetable> wt)
{
    if (wt == nullptr) return;
    std::lock_guard<std::mutex> lock (mutex);
    cache[wt->sourceId] = wt;
}

} // namespace vsx
