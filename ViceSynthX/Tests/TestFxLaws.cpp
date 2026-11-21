#include "../Source/FX/Effects.h"
#include "../Source/FX/EffectsRack.h"
#include "../Source/Presets/SoundRecipes.h"
#include <cmath>

/*
    Effect gain-law and reverb predelay evidence (owner directive, issue #11):
      - per effect at default settings: wet gain re dry, dry/wet correlation, and the level the rack's mix law produces at
        50 % (MEASURE|fxlaw|<effect> ...)
      - reverb wet-only predelay matrix: 0 / 20 / 100 / 200 ms x mono / left / right / anti-phase x 44.1 / 48 / 96 kHz,
        every program: no energy before the predelay, first onset timing, both channels and the side signal survive
      - INIT patch: every effect is off (no accidental wet signal)
*/
namespace vsx
{
class FxLawTests final : public juce::UnitTest
{
public:
    FxLawTests() : juce::UnitTest ("Effect gain laws and predelay", "VICE SYNTH X") {}

    static juce::AudioBuffer<float> run (int type, const juce::AudioBuffer<float>& in, double sr, ParamSnapshot& p)
    {
        auto out = in;
        auto fx = createEffect (type);
        fx->prepare (sr, 256);
        ModOffsets mods;
        FxContext ctx { &p, &mods, 120.0 };
        for (int s = 0; s < out.getNumSamples(); s += 256)
            fx->process (out.getWritePointer (0, s), out.getWritePointer (1, s), std::min (256, out.getNumSamples() - s), ctx);
        return out;
    }

    static double sumSq (const juce::AudioBuffer<float>& b, int ch, int a, int z)
    {
        double e = 0.0;
        for (int i = std::max (0, a); i < std::min (z, b.getNumSamples()); ++i) e += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return e;
    }

    void runTest() override
    {
        beginTest ("Per-effect wet semantics and level at 50 % mix (rack law)");
        {
            const double sr = 48000.0;
            juce::AudioBuffer<float> dry (2, (int) sr);
            for (int i = 0; i < dry.getNumSamples(); ++i)
            {
                float v = 0.0f;
                for (int h = 1; h <= 10; ++h) v += std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) h * (float) i / (float) sr) / (float) h;
                dry.setSample (0, i, 0.2f * v);
                dry.setSample (1, i, 0.2f * v);
            }
            for (int t = 0; t < pid::NumFx; ++t)
            {
                ParamSnapshot p;
                p.value = Recipes::init().values;
                const auto wet = run (t, dry, sr, p);
                const int a = (int) (0.2 * sr), z = dry.getNumSamples();
                double ed = 0, ew = 0, cross = 0, e50 = 0;
                float dg = 1.0f, wg = 1.0f;
                EffectsRack::mixLaw (t, 0.5f, dg, wg);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = a; i < z; ++i)
                    {
                        const double d = dry.getSample (ch, i), w = wet.getSample (ch, i);
                        ed += d * d; ew += w * w; cross += d * w;
                        const double o = dg * d + wg * w; // the rack's law at mix 0.5
                        e50 += o * o;
                    }
                const double gain = 10.0 * std::log10 (std::max (1.0e-20, ew / ed));
                const double corr = cross / std::sqrt (std::max (1.0e-30, ed * ew));
                const double at50 = 10.0 * std::log10 (std::max (1.0e-20, e50 / ed));
                const auto name = choices::fxTypeNames[t];
                logMessage ("MEASURE|fxlaw|" + name + " wet gain|" + juce::String (gain, 2) + "|dB");
                logMessage ("MEASURE|fxlaw|" + name + " dry/wet correlation|" + juce::String (corr, 3) + "|");
                logMessage ("MEASURE|fxlaw|" + name + " level at 50 % mix re dry|" + juce::String (at50, 2) + "|dB");
                bool finite = true;
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < z; ++i) finite &= std::isfinite (wet.getSample (ch, i));
                expect (finite, name);
                expect (at50 < 4.5, name + ": " + juce::String (at50, 1) + " dB build-up at 50 % mix");
            }
        }

        beginTest ("Reverb predelay matrix: wet-only, 4 inputs x 4 predelays x 3 rates x 8 programs");
        {
            const char* inputs[] = { "mono", "left", "right", "anti-phase" };
            int checks = 0;
            for (const double sr : { 44100.0, 48000.0, 96000.0 })
                for (int prog = 0; prog < choices::reverbPrograms.size(); ++prog)
                    for (const float pre : { 0.0f, 20.0f, 100.0f, 200.0f })
                        for (int in = 0; in < 4; ++in)
                        {
                            ParamSnapshot p;
                            p.value = Recipes::init().values;
                            p.value[(size_t) pid::FX (pid::FxReverb, pid::reverb::Predelay)] = pre;
                            p.value[(size_t) pid::FX (pid::FxReverb, pid::reverb::Program)] = (float) prog;
                            p.value[(size_t) pid::FX (pid::FxReverb, pid::reverb::Decay)] = 1.5f;
                            juce::AudioBuffer<float> imp (2, (int) (sr * 0.9));
                            imp.clear();
                            const float l = in == 2 ? 0.0f : 0.5f, r = in == 1 ? 0.0f : (in == 3 ? -0.5f : 0.5f);
                            imp.setSample (0, 0, l);
                            imp.setSample (1, 0, r);
                            const auto out = run (pid::FxReverb, imp, sr, p);
                            const int preS = (int) std::floor (pre * 0.001 * sr);
                            const double before = sumSq (out, 0, 0, preS) + sumSq (out, 1, 0, preS);
                            const double after = sumSq (out, 0, preS, out.getNumSamples()) + sumSq (out, 1, preS, out.getNumSamples());
                            int onset = -1;
                            for (int i = 0; i < out.getNumSamples() && onset < 0; ++i)
                                if (std::abs (out.getSample (0, i)) > 1.0e-6f || std::abs (out.getSample (1, i)) > 1.0e-6f) onset = i;
                            const juce::String id = choices::reverbPrograms[prog] + " " + inputs[in] + " " + juce::String (pre, 0) + " ms @ " + juce::String ((int) sr);
                            expect (before <= 1.0e-20, id + ": energy before the predelay " + juce::String (before));
                            expect (onset >= preS && onset <= preS + (int) (0.03 * sr) + 2, id + ": first onset at " + juce::String (onset) + " samples, predelay " + juce::String (preS));
                            expect (after > 1.0e-6, id + ": silent");
                            // both channels carry the tail; an anti-phase (side-only) input is not cancelled
                            expect (sumSq (out, 0, preS, out.getNumSamples()) > 1.0e-7 && sumSq (out, 1, preS, out.getNumSamples()) > 1.0e-7, id + ": one channel silent");
                            if (prog == 0 && pre == 20.0f && sr == 48000.0)
                                logMessage ("MEASURE|predelay|Hall " + juce::String (inputs[in]) + " 20 ms onset|" + juce::String ((onset - preS) * 1000.0 / sr, 3) + "|ms after predelay");
                            ++checks;
                        }
            logMessage ("MEASURE|predelay|configurations checked|" + juce::String (checks) + "|");
        }

        beginTest ("INIT patch: no effect is active (no accidental wet signal)");
        {
            const auto init = Recipes::init();
            for (int t = 0; t < pid::NumFx; ++t)
                expect (init.values[(size_t) pid::FX (t, pid::fx::On)] < 0.5f, choices::fxTypeNames[t] + " is on in INIT");
        }
    }
};

static FxLawTests fxLawTests;
} // namespace vsx
