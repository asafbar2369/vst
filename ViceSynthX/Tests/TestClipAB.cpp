#include "../Source/Analysis/SoundMatcher.h"
#include "../Source/Presets/PresetManager.h"
#include "../Source/Core/DspUtils.h"
#include <juce_audio_formats/juce_audio_formats.h>

/*
    Before / after evidence for the clipping fix (gain staging). Same DSP, same MIDI (a full-velocity six-note chord
    then a single note), rendered through the output stage of the plug-in (Preset Volume, then the safety stage that
    engages above -1 dBFS):
      before = the Preset Volume these presets shipped with in 9f2cf20 / 91e6193 (calibrated on single notes only)
      after  = the current calibration (single note and full-velocity chord)
    MEASURE|clipab|<preset> ... samples in the safety stage / peaks. With VSX_AUDIO_DIR, writes clip_<preset>_{before,after}.wav.
*/
namespace vsx
{
class ClipABTests final : public juce::UnitTest
{
public:
    ClipABTests() : juce::UnitTest ("Clipping before / after evidence", "VICE SYNTH X") {}

    static float safe (float x)
    {
        const float ax = std::abs (x);
        if (ax <= 0.89f) return x;
        return (x < 0.0f ? -1.0f : 1.0f) * (0.89f + 0.1f * fastTanh ((ax - 0.89f) / 0.1f));
    }

    void runTest() override
    {
        beginTest ("Full-velocity chords: time spent in the output safety stage, before vs after");
        const std::pair<const char*, float> shipped[] = { { "Ocean Drive Poly", -7.4f }, { "Five Voice Brass", -5.0f }, { "Sunset Boulevard Pad", -6.9f },
                                                          { "Ladder Mono Bass", -6.9f }, { "Miami Hero Lead", -5.5f }, { "Crystal Sky", 3.4f },
                                                          { "Reese Tide", -7.9f }, { "Club Organ", 2.1f } };
        auto renderer = std::make_unique<OfflineRenderer> (48000.0);
        const auto dir = juce::SystemStats::getEnvironmentVariable ("VSX_AUDIO_DIR", {});
        for (const auto& [name, oldDb] : shipped)
        {
            const PatchSpec* spec = nullptr;
            for (const auto& p : FactoryPresets::all()) if (p.meta.name == name) spec = &p;
            expect (spec != nullptr, name);
            if (spec == nullptr) continue;
            const int root = spec->meta.category == "Bass" ? 36 : 48;
            auto chord = renderer->renderNotes (*spec, { root, root + 4, root + 7, root + 11, root + 14, root + 19 }, 127, 1.6, 0.6, 1);
            const auto single = renderer->render (*spec, root + 12, 100, 1.2, 0.6, 1);
            // renderer output includes the current Preset Volume; rescale it to the shipped one for "before"
            const float newDb = spec->get ("master_volume");
            juce::AudioBuffer<float> phrase (2, chord.getNumSamples() + single.getNumSamples());
            for (int c = 0; c < 2; ++c)
            {
                phrase.copyFrom (c, 0, chord, c, 0, chord.getNumSamples());
                phrase.copyFrom (c, chord.getNumSamples(), single, c, 0, single.getNumSamples());
            }
            auto out = [&] (float db, int& engaged, float& peak)
            {
                juce::AudioBuffer<float> b (phrase);
                b.applyGain (juce::Decibels::decibelsToGain (db - newDb));
                engaged = 0; peak = 0.0f;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < b.getNumSamples(); ++i)
                    {
                        const float x = b.getSample (c, i);
                        peak = std::max (peak, std::abs (x));
                        if (std::abs (x) > 0.89f) ++engaged;
                        b.setSample (c, i, safe (x));
                    }
                return b;
            };
            int eb = 0, ea = 0; float pb = 0, pa = 0;
            const auto before = out (oldDb, eb, pb);
            const auto after = out (newDb, ea, pa);
            logMessage ("MEASURE|clipab|" + juce::String (name) + " samples in safety stage before / after|" + juce::String (eb) + " / " + juce::String (ea) + "|samples");
            logMessage ("MEASURE|clipab|" + juce::String (name) + " pre-safety peak before / after|" + juce::String (juce::Decibels::gainToDecibels (pb), 1)
                        + " / " + juce::String (juce::Decibels::gainToDecibels (pa), 1) + "|dBFS");
            expect (ea == 0, juce::String (name) + ": the calibrated preset must not reach the safety stage");
            if (dir.isNotEmpty())
            {
                const juce::File d (dir);
                d.createDirectory();
                for (auto [b, tag] : { std::pair<const juce::AudioBuffer<float>*, const char*> { &before, "before" }, { &after, "after" } })
                {
                    const auto f = d.getChildFile ("clip_" + juce::String (name).replace (" ", "_").toLowerCase() + "_" + tag + ".wav");
                    f.deleteFile();
                    auto os = std::make_unique<juce::FileOutputStream> (f);
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 2, 24, {}, 0));
                    if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (*b, 0, b->getNumSamples()); }
                }
            }
        }
    }
};

static ClipABTests clipABTests;
} // namespace vsx
