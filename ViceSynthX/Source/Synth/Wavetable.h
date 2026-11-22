#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace vsx
{
/**
    Immutable, band-limited wavetable.

    Each frame is a 2048-sample single cycle. For every frame, 11 mip levels are built by
    zeroing FFT bins above 1024 >> level harmonics, so playback can always select a level
    whose highest harmonic stays below the (oversampled) Nyquist frequency.
    The first 64 harmonic amplitudes/phases of each frame are kept for the Spectral engine.
*/
class Wavetable
{
public:
    static constexpr int kSize = 2048;
    static constexpr int kLevels = 11;
    static constexpr int kStride = kSize + 4;
    static constexpr int kPartials = 64;
    static constexpr int kMaxFrames = 256;

    juce::String name;
    juce::String sourceId;   // "factory:<name>", "user:<path>", "custom:<name>"
    int numFrames = 0;

    const float* table (int frame, int level) const noexcept
    {
        return data.data() + ((size_t) frame * kLevels + (size_t) level) * kStride;
    }

    const float* partialAmps (int frame) const noexcept { return partialMag.data() + (size_t) frame * kPartials; }
    const float* partialPhases (int frame) const noexcept { return partialPhase.data() + (size_t) frame * kPartials; }
    const std::vector<float>& rawFrame (int frame) const { return frames[(size_t) frame]; }
    const std::vector<std::vector<float>>& rawFrames() const { return frames; }

    /** Mip level for a phase increment (cycles/sample) and a harmonic multiplier (e.g. sync ratio). */
    static int levelFor (float phaseInc, float harmonicMultiplier = 1.0f) noexcept
    {
        const float maxHarm = 0.5f / std::max (1.0e-6f, phaseInc * harmonicMultiplier);
        if (maxHarm >= 1024.0f) return 0;
        if (maxHarm < 1.0f) return kLevels - 1;
        int level = 0;
        while (level < kLevels - 1 && (float) (1024 >> level) > maxHarm)
            ++level;
        return level;
    }

    /** Linear-interpolated read of one frame at one level, phase in [0,1). */
    static float read (const float* t, float phase) noexcept
    {
        const float idx = phase * (float) kSize;
        const int i = (int) idx;
        const float fr = idx - (float) i;
        return t[i] + fr * (t[i + 1] - t[i]);
    }

    /** Builds a table from time-domain frames (each resampled to 2048 if needed). */
    static std::shared_ptr<Wavetable> fromFrames (const std::vector<std::vector<float>>& frames, const juce::String& name,
                                                  const juce::String& sourceId, bool normaliseEachFrame = true);

    /** Builds a table from harmonic amplitude sets (amplitude per harmonic, sine phase). */
    static std::shared_ptr<Wavetable> fromHarmonics (const std::vector<std::vector<float>>& harmonicAmps,
                                                     const juce::String& name, const juce::String& sourceId);

    /** Imports a WAV/AIFF. Lengths that are multiples of 2048 are treated as consecutive frames;
        otherwise the pitch is detected and single cycles are extracted across the file. */
    static std::shared_ptr<Wavetable> fromAudioFile (const juce::File& file, juce::String& error);

    static std::shared_ptr<Wavetable> fromAudio (const juce::AudioBuffer<float>& audio, double sampleRate,
                                                 const juce::String& name, const juce::String& sourceId,
                                                 juce::String& error);

    /** Writes frames as 32-bit float WAV with 2048-sample frames (common wavetable interchange layout). */
    bool exportToWav (const juce::File& file) const;

private:
    std::vector<float> data;
    std::vector<float> partialMag, partialPhase;
    std::vector<std::vector<float>> frames;
};

/** Original procedurally designed factory wavetables. */
namespace WavetableFactory
{
    juce::StringArray names();                          // "Classic" tables first, then the generated families
    juce::String category (const juce::String& name);   // "Classic", "Spectra", "FM", "Formant", ... (empty if unknown)
    juce::StringArray categories();
    std::vector<std::vector<float>> generateFrames (const juce::String& name);
}

/** Message-thread cache of factory and user wavetables. */
class WavetableLibrary
{
public:
    static WavetableLibrary& instance();

    juce::StringArray factoryNames() const { return WavetableFactory::names(); }
    juce::StringArray userNames();
    juce::File userFolder() const;

    /** sourceId: "factory:<name>" or "user:<file path>". Returns nullptr on failure. */
    std::shared_ptr<const Wavetable> load (const juce::String& sourceId, juce::String& error);
    std::shared_ptr<const Wavetable> factory (const juce::String& name);

    void storeCustom (std::shared_ptr<const Wavetable> wt);

private:
    WavetableLibrary() = default;
    std::mutex mutex;
    std::map<juce::String, std::shared_ptr<const Wavetable>> cache;
};

} // namespace vsx
