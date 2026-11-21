#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace vsx
{
/**
    Immutable sample with anti-aliasing mip levels and spectral analysis frames.
    Level 0 is the original; level n is decimated by 2^n with a windowed-sinc low-pass,
    used when the sample is played more than 2^n times faster than recorded.
*/
struct SampleData
{
    static constexpr int kMaxMips = 4;
    static constexpr int kSpecPartials = 64;
    static constexpr int kGuard = 4;

    juce::String name;
    juce::String sourceId;      // "file:<path>" | "builtin:<name>/<zone>" | "sfz:<path>#<n>"
    double sampleRate = 44100.0;
    int numChannels = 1;
    int length = 0;             // in samples at level 0
    int rootNote = 60;
    float fineTuneCents = 0.0f;
    bool hasLoop = false;
    int loopStart = 0, loopEnd = 0;
    bool truncated = false;     // file longer than the import limit: only the first kMaxSeconds were loaded
    bool rootFromFile = false;  // rootNote / fineTuneCents come from file metadata, the file name or pitch detection
    static constexpr double kMaxSeconds = 120.0;

    std::vector<juce::AudioBuffer<float>> mips; // each with kGuard extra samples at both ends (index offset kGuard)

    // Spectral analysis (for Spectral mode on samples)
    float analysedF0 = 0.0f;    // 0 if unpitched
    double specHopSeconds = 0.0;
    int numSpecFrames = 0;
    std::vector<float> specAmps; // [frame][kSpecPartials]

    const float* channel (int level, int ch) const noexcept
    {
        const auto& b = mips[(size_t) level];
        return b.getReadPointer (std::min (ch, b.getNumChannels() - 1)) + kGuard;
    }
    int levelLength (int level) const noexcept { return mips[(size_t) level].getNumSamples() - 2 * kGuard; }

    /** Builds mips + spectral frames from raw audio. */
    static std::shared_ptr<SampleData> create (juce::AudioBuffer<float> audio, double sampleRate, const juce::String& name,
                                               const juce::String& sourceId, int rootNote = 60);

    static std::shared_ptr<SampleData> fromFile (const juce::File& file, juce::String& error);
};

/** Key/velocity-mapped zones (multisample). */
struct SampleZone
{
    std::shared_ptr<const SampleData> sample;
    int loKey = 0, hiKey = 127, loVel = 1, hiVel = 127;
    int rootNote = 60;
    float tuneCents = 0.0f;
    float gainDb = 0.0f;
    float pan = 0.0f;
    int loopMode = -1;        // -1 = use oscillator parameter, 0 off, 1 forward, 2 ping-pong
    int loopStart = -1, loopEnd = -1; // -1 = use sample metadata / parameters
    int offset = 0;
};

struct MultiSample
{
    juce::String name;
    juce::String sourceId;    // "sfz:<path>" | "builtin:<name>" | "folder:<path>"
    std::vector<SampleZone> zones;
    juce::StringArray missingFiles;

    /** Best matching zone for a note/velocity, falling back to the nearest key range. */
    const SampleZone* find (int note, int velocity) const noexcept;
};

/** SFZ subset parser: <control> default_path, <global>/<group>/<region> inheritance and the
    opcodes sample, lokey, hikey, key, pitch_keycenter, lovel, hivel, tune, transpose, volume,
    pan, loop_mode, loop_start/loopstart, loop_end/loopend, offset. */
namespace Sfz
{
    std::shared_ptr<MultiSample> load (const juce::File& sfzFile, juce::String& error);
    int parseNoteName (const juce::String& text); // "c4", "f#3", "60" -> MIDI (C4 = 60)
}

/** Builds a multisample from a folder of audio files whose names contain note names. */
std::shared_ptr<MultiSample> multisampleFromFolder (const juce::File& folder, juce::String& error);

/** Original procedurally synthesised sample content (no third-party recordings). */
namespace BuiltinSamples
{
    juce::StringArray singleNames();       // single samples (for Sample/Granular/Spectral)
    juce::StringArray multiNames();        // multisample instruments
    std::shared_ptr<SampleData> renderSingle (const juce::String& name);
    std::shared_ptr<MultiSample> renderMulti (const juce::String& name);
}

/** Thread-safe cache/loader for samples and multisamples. */
class SampleLibrary
{
public:
    static SampleLibrary& instance();
    juce::File userFolder() const;

    std::shared_ptr<const SampleData> loadSample (const juce::String& sourceId, juce::String& error);
    std::shared_ptr<const MultiSample> loadMulti (const juce::String& sourceId, juce::String& error);

private:
    SampleLibrary() = default;
    std::mutex mutex;
    std::map<juce::String, std::shared_ptr<const SampleData>> samples;
    std::map<juce::String, std::shared_ptr<const MultiSample>> multis;
};

} // namespace vsx
