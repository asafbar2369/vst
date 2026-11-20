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
            auto renderer = std::make_unique<OfflineRenderer> (rate);
            for (size_t mode = 0; mode < patches.size(); ++mode)
            {
                auto output = renderer->render (patches[mode], 60, 100, 0.4, 0.2, 1);
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

        // One reusable renderer, heap allocated so test stack size does not
        // obscure a real DSP failure. Keep the tests independent of the GUI.
        const auto checkAudio = [this] (const juce::AudioBuffer<float>& out, const juce::String& label)
        {
            bool finite = true;
            float peak = 0.0f;
            double energy = 0.0;
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                for (int n = 0; n < out.getNumSamples(); ++n)
                {
                    const float value = out.getSample (ch, n);
                    if (! std::isfinite (value)) { finite = false; continue; }
                    peak = std::max (peak, std::abs (value));
                    energy += (double) value * value;
                }
            expect (out.getNumChannels() == 2 && out.getNumSamples() > 0, label + ": invalid buffer");
            expect (finite, label + ": NaN/Inf");
            expect (peak > 1.0e-5f && energy > 1.0e-10, label + ": unexpected silence");
            // This is a runaway guard, not a loudness or clipping certification.
            expect (peak < 16.0f, label + ": runaway output");
        };
        {
            auto engine = std::make_unique<OfflineRenderer> (48000.0);
            beginTest ("Every filter model at multiple resonance settings");
            for (int model = 0; model <= SVF; ++model)
                for (const float resonance : { 0.1f, 0.5f, 0.9f })
                {
                    auto spec = init ("Filter QA", "Test");
                    for (int fxType = 0; fxType < pid::NumFx; ++fxType) fxOff (spec, fxType);
                    filter (spec, 0, model, LP24, 1600.0f, resonance, 0.2f);
                    const auto label = "model " + juce::String (model) + " resonance " + juce::String (resonance);
                    checkAudio (engine->render (spec, 48, 100, 0.45, 0.25), label);
                }

            beginTest ("Every effect renders finite audio in isolation");
            for (int fxType = 0; fxType < pid::NumFx; ++fxType)
            {
                auto spec = init ("FX QA", "Test");
                for (int k = 0; k < pid::NumFx; ++k) fxOff (spec, k);
                fx (spec, fxType, 0.5f);
                checkAudio (engine->render (spec, 60, 100, 0.65, 0.5),
                            choices::fxTypeNames[fxType]);
            }

            beginTest ("Every factory preset produces finite playable audio");
            for (const auto& spec : factory)
            {
                // Include a long enough note to exercise slow pads and risers.
                checkAudio (engine->render (spec, 60, 100, 2.0, 0.5), spec.meta.name);
            }
        }

        beginTest ("Representative patch renders at high sample rates and all quality levels");
        for (const double rate : { 88200.0, 96000.0, 192000.0 })
        {
            auto renderer = std::make_unique<OfflineRenderer> (rate);
            for (int quality = 0; quality <= 2; ++quality)
                checkAudio (renderer->render (patches[0], 60, 100, 0.25, 0.15, quality),
                            juce::String (rate) + " Hz quality " + juce::String (quality));
        }

        beginTest ("Invalid oversampling requests are bounded");
        {
            auto state = std::make_unique<PatchState>();
            auto engine = std::make_unique<SynthEngine> (*state);
            engine->prepare (48000.0, 512);
            expectEquals (engine->latencyFor (-1), 0);
            expectEquals (engine->latencyFor (3), engine->latencyFor (2));
            expectEquals (engine->latencyFor (999), engine->latencyFor (2));
        }

        beginTest ("Arpeggiator mode changes and panic release direct notes");
        {
            ArpSeq arp;
            arp.prepare (48000.0);
            ParamSnapshot p;
            p.value = init().values;
            TransportInfo t;
            juce::MidiBuffer input, output;
            ArpPattern pattern;
            SeqPattern seq;
            p.value[(size_t) pid::ARP (pid::arp::Mode)] = 0.0f;
            input.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            arp.process (input, output, 512, p, &pattern, &seq, t);
            input.clear();
            output.clear();
            p.value[(size_t) pid::ARP (pid::arp::Mode)] = 1.0f;
            arp.process (input, output, 512, p, &pattern, &seq, t);
            bool directNoteReleased = false;
            for (const auto event : output)
                directNoteReleased |= event.getMessage().isNoteOff() && event.getMessage().getNoteNumber() == 60;
            expect (directNoteReleased, "Direct note must be released on enabling arp");

            output.clear();
            p.value[(size_t) pid::ARP (pid::arp::Mode)] = 0.0f;
            arp.process (input, output, 512, p, &pattern, &seq, t);
            bool directNoteRestored = false;
            for (const auto event : output)
                directNoteRestored |= event.getMessage().isNoteOn() && event.getMessage().getNoteNumber() == 60;
            expect (directNoteRestored, "Held note must return on disabling arp");

            input.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            output.clear();
            arp.process (input, output, 512, p, &pattern, &seq, t);
            input.clear();
            output.clear();
            p.value[(size_t) pid::ARP (pid::arp::Mode)] = 1.0f;
            arp.process (input, output, 512, p, &pattern, &seq, t);
            for (const auto event : output)
                expect (! event.getMessage().isNoteOn(), "Panic must clear all remembered held notes");
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
