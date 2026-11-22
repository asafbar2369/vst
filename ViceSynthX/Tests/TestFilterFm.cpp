#include "../Source/PluginProcessor.h"
#include "../Source/Presets/SoundRecipes.h"
#include "../Source/Synth/Filters.h"
#include <cmath>

/*
    Audio-rate filter FM: OSC B (level 0, so only its signal reaches the filter as modulation) moves Filter 1's cutoff.
    The modulator (1975 Hz) is above the control-rate Nyquist (48 kHz / 16 / 2 = 1500 Hz), so FM sidebands at the
    carrier +/- the modulator can only come from per-sample modulation.
*/
namespace vsx
{
class FilterFmTests final : public juce::UnitTest
{
public:
    FilterFmTests() : juce::UnitTest ("Filter FM", "VICE SYNTH X") {}

    static double levelHz (const std::vector<float>& x, double f)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / 48000.0, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        const size_t a = 4096, n = x.size() - a;
        for (size_t i = 0; i < n; ++i)
        {
            const double t = 2.0 * juce::MathConstants<double>::pi * (double) i / (double) (n - 1);
            const double win = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
            const double s0 = x[a + i] * win + c * s1 - s2; s2 = s1; s1 = s0;
        }
        return 10.0 * std::log10 ((s1 * s1 + s2 * s2 - c * s1 * s2) / ((double) n * n) + 1e-30);
    }

    std::vector<float> render (float fmOct, int source)
    {
        auto s = Recipes::init ("FM test", "Test");
        for (const char* o : { "a", "b" })
        {
            const juce::String p = juce::String ("osc_") + o + "_";
            s.set (p + "position", 0.0f); s.set (p + "unison", 1.0f); s.set (p + "phase_rand", 0.0f);
        }
        s.set ("osc_drift", 0.0f);
        s.set ("osc_b_on", 1.0f);
        s.set ("osc_b_level", 0.0f);
        s.set ("osc_b_octave", 1.0f);
        s.set ("osc_b_semi", 7.0f);
        s.set ("osc_b_fine", 37.0f); // 19.37 st above: not a harmonic of the carrier
        s.set ("flt1_on", 1.0f); s.set ("flt1_mode", 1.0f); s.set ("flt1_cutoff", 700.0f); s.set ("flt1_resonance", 0.0f);
        s.set ("flt1_env_amount", 0.0f); s.set ("flt1_key_track", 0.0f); s.set ("flt1_drift", 0.0f); s.set ("flt1_tolerance", 0.0f);
        s.set ("flt1_fm_amount", fmOct); s.set ("flt1_fm_source", (float) source);
        s.set ("master_volume", -12.0f);
        ViceSynthProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.applyPatch (s, "fm");
        juce::AudioBuffer<float> block (2, 256);
        std::vector<float> x;
        for (int b = 0; b < 120; ++b)
        {
            juce::MidiBuffer m;
            if (b == 2) m.addEvent (juce::MidiMessage::noteOn (1, 76, (juce::uint8) 100), 0);
            block.clear();
            proc.processBlock (block, m);
            if (b >= 2) for (int i = 0; i < 256; ++i) x.push_back (block.getSample (0, i));
        }
        proc.releaseResources();
        return x;
    }

    void runTest() override
    {
        beginTest ("AnalogFilter::setFm directly");
        {
            const double fc0 = 659.26, fm0 = 1975.5;
            for (int fmOn = 0; fmOn < 2; ++fmOn)
            {
                AnalogFilter flt;
                flt.prepare (48000.0);
                flt.setModel (0, 1);
                flt.update (700.0f, 0.0f, 0.0f, 0.5f, 0);
                std::vector<float> x;
                for (int i = 0; i < 48000; ++i)
                {
                    const double t = i / 48000.0;
                    float l = 0.3f * (float) std::sin (2 * juce::MathConstants<double>::pi * fc0 * t), r = l;
                    if (fmOn) flt.setFm (std::exp2 ((float) std::sin (2 * juce::MathConstants<double>::pi * fm0 * t)));
                    flt.process (l, r);
                    x.push_back (l);
                }
                const double sb = levelHz (x, fm0 + fc0) - levelHz (x, fc0);
                logMessage ("MEASURE|filter fm|AnalogFilter alone, 1 oct FM " + juce::String (fmOn ? "on" : "off") + ": sideband vs carrier|" + juce::String (sb, 1) + "|dB");
                expect (fmOn ? sb > -30.0 : sb < -100.0, "sideband with / without FM");
            }
        }
        beginTest ("Sidebands at carrier +/- modulator above the control-rate Nyquist");
        const double fc = 440.0 * std::pow (2.0, 7.0 / 12.0), fmod = fc * std::pow (2.0, 19.37 / 12.0);
        const auto off = render (0.0f, 1), on = render (1.0f, 1), deep = render (3.0f, 1);
        auto side = [&] (const std::vector<float>& x) { return std::max (levelHz (x, fmod + fc), levelHz (x, fmod - fc)) - levelHz (x, fc); };
        logMessage ("MEASURE|filter fm|sideband level vs carrier, FM 0 / 1 / 3 oct|" + juce::String (side (off), 1) + " / "
                    + juce::String (side (on), 1) + " / " + juce::String (side (deep), 1) + "|dB");
        expect (side (off) < -80.0, "no sidebands without FM");
        expect (side (on) > -45.0, "FM 1 octave creates sidebands");
        expect (side (deep) > side (on) + 6.0, "deeper FM, stronger sidebands");
        float peak = 0.0f; bool finite = true;
        for (float v : deep) { peak = std::max (peak, std::abs (v)); finite = finite && std::isfinite (v); }
        logMessage ("MEASURE|filter fm|peak at FM 3 oct|" + juce::String (juce::Decibels::gainToDecibels (peak), 1) + "|dBFS");
        expect (finite && peak < 1.0f, "bounded");
    }
};

static FilterFmTests filterFmTests;
} // namespace vsx
