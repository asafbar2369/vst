#include "../Source/Analysis/SoundMatcher.h"
#include "../Source/Presets/SoundRecipes.h"
#include "../Source/Presets/PresetManager.h"
#include <cmath>
#include <limits>

namespace vsx
{
class CoreDspTests final : public juce::UnitTest
{
public:
    CoreDspTests() : juce::UnitTest ("Core DSP rendering", "VICE SYNTH X") {}

    void runTest() override
    {
        beginTest ("Factory library has distinct names and round-trippable parameter state");
        const auto& factory = FactoryPresets::all();
        expect (factory.size() >= 85);
        juce::StringArray names;
        for (const auto& spec : factory)
        {
            expect (! names.contains (spec.meta.name), spec.meta.name);
            names.add (spec.meta.name);
            PatchSpec loaded;
            juce::String error;
            const bool ok = PatchSpec::fromJson (spec.toJson(), loaded, error);
            expect (ok, spec.meta.name + ": " + error);
            if (ok)
            {
                expectEquals (loaded.meta.name, spec.meta.name);
                for (size_t i = 0; i < spec.values.size(); ++i)
                    expectWithinAbsoluteError (loaded.values[i], spec.values[i], 0.0001f);
            }
        }

        beginTest ("All six oscillator modes produce finite, non-silent audio");
        using namespace Recipes;
        std::array<PatchSpec, 6> patches;
        for (auto& spec : patches)
            spec = init ("DSP smoke", "Test");
        wavetable (patches[0], 0, "Basic Shapes", 0.67f, 0.6f);
        analog (patches[1], 0, 0.67f, 0.6f);
        sample (patches[2], 0, "Breath Pad", 0.6f, 60, 1);
        multisample (patches[3], 0, "Tine Piano", 0.6f);
        granular (patches[4], 0, "Granular Texture", 0.4f, 80.0f, 30.0f, 0.1f, 0.6f);
        spectral (patches[5], 0, "Soft Pad", false, 0.6f);
        for (const auto rate : { 44100.0, 48000.0 })
        {
            auto rendererOwner = std::make_unique<OfflineRenderer> (rate);
            auto& renderer = *rendererOwner;
            for (size_t mode = 0; mode < patches.size(); ++mode)
            {
                auto output = renderer.render (patches[mode], 60, 100, 0.4, 0.2, 1);
                const auto label = "mode " + juce::String ((int) mode) + " at " + juce::String (rate);
                expectEquals (output.getNumChannels(), 2, label);
                expectEquals (output.getNumSamples(), (int) ((0.4 + 0.2) * rate), label);
                bool finite = true;
                float peak = 0.0f;
                for (int ch = 0; ch < output.getNumChannels(); ++ch)
                    for (int i = 0; i < output.getNumSamples(); ++i)
                    {
                        const auto value = output.getSample (ch, i);
                        finite = finite && std::isfinite (value);
                        if (std::isfinite (value))
                            peak = std::max (peak, std::abs (value));
                    }
                expect (finite, label + ": non-finite sample");
                expect (peak > 0.00001f, label + ": silent output");
            }
        }

        beginTest ("Reference analysis recognizes a known sine wave");
        constexpr double sr = 48000.0;
        juce::AudioBuffer<float> tone (2, 48000);
        for (int i = 0; i < tone.getNumSamples(); ++i)
        {
            const auto x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sr);
            tone.setSample (0, i, x);
            tone.setSample (1, i, x);
        }
        const auto features = AudioFeatures::analyse (tone, sr);
        expect (features.valid);
        expectWithinAbsoluteError (features.f0, 440.0f, 10.0f);
        expectWithinAbsoluteError (features.stereoWidth, 0.0f, 0.001f);
        expectWithinAbsoluteError (AudioFeatures::similarity (features, features), 1.0f, 0.001f);

        beginTest ("Malformed reference data is rejected safely");
        expect (! AudioFeatures::analyse (tone, 0.0).valid);
        expect (! AudioFeatures::analyse (tone, std::numeric_limits<double>::quiet_NaN()).valid);
        juce::AudioBuffer<float> empty (1, 0);
        expect (! AudioFeatures::analyse (empty, sr).valid);
        juce::AudioBuffer<float> shortTone (1, 2048);
        shortTone.clear();
        shortTone.setSample (0, 0, 0.5f);
        expect (! AudioFeatures::analyse (shortTone, 768000.0).valid);
        tone.setSample (0, 10, std::numeric_limits<float>::infinity());
        expect (! AudioFeatures::analyse (tone, sr).valid);
    }
};

static CoreDspTests coreDspTests;
} // namespace vsx
