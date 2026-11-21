#include "../Source/Synth/Filters.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    Filter quality measurements (MEASURE|filterq|<model> <metric>|value|unit), run at 88.2 kHz (2x of 44.1 kHz, the
    default High quality). Set VSX_FILTER_CSV=<dir> to also write frequency-response families for plotting
    (tools/plot_filters.py).
      thd_d<drive>_<level>   THD (%) of a 110 Hz sine at the given drive and input level (fc 8 kHz, res 0)
      bass_res<r>            LP24 level at 100 Hz (fc 1 kHz) relative to resonance 0 (bass retention)
      peak_ratio_res0.9      measured resonance-peak frequency / cutoff setting (fc 1 kHz, resonance 0.9)
      hp_slope               HP attenuation one octave and three octaves below fc (dB)
      mod_peak               output peak under random cutoff / resonance modulation (noise input, dBFS)
*/
namespace vsx
{
class FilterQualityTests final : public juce::UnitTest
{
public:
    FilterQualityTests() : juce::UnitTest ("Filter quality", "VICE SYNTH X") {}
    static constexpr double sr = 88200.0;

    static float thd (int model, float drive, float level)
    {
        AnalogFilter f;
        f.prepare (sr);
        f.setModel (model, (int) FilterMode::LP24);
        f.setTolerance (0.0f, 1);
        f.update (8000.0f, 0.0f, drive, 0.5f, 0);
        constexpr int n = 1 << 15;
        const double hz = sr / n * 41.0; // bin-centred ~110 Hz
        std::vector<float> y ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n + 8192; ++i)
        {
            float l = level * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / sr), r = l;
            f.process (l, r);
            if (i >= 8192) y[(size_t) (i - 8192)] = l;
        }
        juce::dsp::FFT fft (15);
        fft.performFrequencyOnlyForwardTransform (y.data());
        const double fund = y[41];
        double harm = 0.0;
        for (int h = 2; h <= 15; ++h) harm += (double) y[(size_t) (41 * h)] * y[(size_t) (41 * h)];
        return (float) (100.0 * std::sqrt (harm) / std::max (1.0e-9, fund));
    }

    static float levelAt (int model, int mode, float cutoff, float res, float hz, float character = 0.5f)
    {
        float f[1] { hz }, m[1] {};
        measureFilterResponse (model, mode, cutoff, res, 0.0f, character, sr, f, m, 1);
        return m[0];
    }

    static float peakFrequency (int model, float cutoff, float res)
    {
        constexpr int n = 400;
        float fr[n], m[n];
        for (int i = 0; i < n; ++i) fr[i] = 100.0f * std::pow (100.0f, (float) i / (n - 1)); // 100 Hz .. 10 kHz
        measureFilterResponse (model, (int) FilterMode::LP24, cutoff, res, 0.0f, 0.5f, sr, fr, m, n);
        int best = 0;
        for (int i = 1; i < n; ++i) if (m[i] > m[best]) best = i;
        return fr[best];
    }

    static float modPeak (int model)
    {
        AnalogFilter f;
        f.prepare (sr);
        f.setModel (model, (int) FilterMode::LP24);
        f.setTolerance (0.5f, 3);
        juce::Random rng (9);
        float peak = 0.0f;
        for (int block = 0; block < 4000; ++block)
        {
            f.update (40.0f * std::pow (400.0f, rng.nextFloat()), rng.nextFloat(), 0.3f, rng.nextFloat(), 32);
            for (int i = 0; i < 32; ++i)
            {
                float l = (rng.nextFloat() * 2.0f - 1.0f) * 0.7f, r = l;
                f.process (l, r);
                peak = std::max (peak, std::abs (l));
            }
        }
        return juce::Decibels::gainToDecibels (peak);
    }

    void runTest() override
    {
        const char* names[] = { "MG Ladder", "Diode Ladder", "Acid Diode", "MS Sallen-Key", "JP OTA", "JN OTA", "P5 Cascade", "SEM State-Var", "Analog SVF" };
        auto line = [this] (const juce::String& item, double v, const juce::String& unit)
        { logMessage ("MEASURE|filterq|" + item + "|" + juce::String (v, 3) + "|" + unit); };

        beginTest ("Saturation: gentle at low drive, rising with drive (THD of a 110 Hz sine)");
        for (int m = 0; m < (int) FilterModel::Count; ++m)
        {
            float prev = -1.0f;
            for (float d : { 0.0f, 0.3f, 0.6f, 1.0f })
            {
                const float t = thd (m, d, 0.5f);
                line (juce::String (names[m]) + " thd_d" + juce::String (d, 1) + "_0.5FS", t, "%");
                expect (std::isfinite (t));
                expect (t >= prev * 0.7f, juce::String (names[m]) + " distortion should grow with drive");
                prev = t;
            }
            const float clean = thd (m, 0.0f, 0.5f);
            expect (clean < 3.0f, juce::String (names[m]) + " THD at drive 0, 0.5 FS = " + juce::String (clean, 2) + " % (should be clean)");
            const float hot = thd (m, 0.0f, 1.0f);
            line (juce::String (names[m]) + " thd_d0.0_1.0FS", hot, "%");
            const float driven = thd (m, 1.0f, 0.5f);
            expect (driven > clean * 2.0f, juce::String (names[m]) + " drive must still add audible saturation");
        }

        beginTest ("Resonance: bass retention and peak frequency follows the cutoff");
        for (int m = 0; m < (int) FilterModel::Count; ++m)
        {
            const float ref = levelAt (m, 0, 1000.0f, 0.0f, 100.0f);
            for (float r : { 0.5f, 0.9f })
            {
                const float b = levelAt (m, 0, 1000.0f, r, 100.0f) - ref;
                line (juce::String (names[m]) + " bass_res" + juce::String (r, 1), b, "dB");
                expect (b > -9.0f, juce::String (names[m]) + " loses " + juce::String (-b, 1) + " dB of bass at resonance " + juce::String (r));
            }
            const float pf = peakFrequency (m, 1000.0f, 0.9f);
            line (juce::String (names[m]) + " peak_ratio_res0.9", pf / 1000.0f, "x");
            expect (pf > 700.0f && pf < 1400.0f, juce::String (names[m]) + " resonance peak at " + juce::String (pf, 0) + " Hz for a 1 kHz cutoff");
        }

        beginTest ("High-pass slopes");
        for (int m = 0; m < (int) FilterModel::Count; ++m)
        {
            const float pass = levelAt (m, (int) FilterMode::HP, 1000.0f, 0.0f, 8000.0f);
            const float oct1 = levelAt (m, (int) FilterMode::HP, 1000.0f, 0.0f, 500.0f) - pass;
            const float oct3 = levelAt (m, (int) FilterMode::HP, 1000.0f, 0.0f, 125.0f) - pass;
            line (juce::String (names[m]) + " hp_1oct_below", oct1, "dB");
            line (juce::String (names[m]) + " hp_3oct_below", oct3, "dB");
            expect (oct3 < -24.0f, juce::String (names[m]) + " HP only " + juce::String (oct3, 1) + " dB down three octaves below (needs >= 12 dB/oct)");
        }

        beginTest ("Modulation robustness (random cutoff / resonance, noise input)");
        for (int m = 0; m < (int) FilterModel::Count; ++m)
        {
            const float p = modPeak (m);
            line (juce::String (names[m]) + " mod_peak", p, "dBFS");
            expect (std::isfinite (p) && p < 6.0f, juce::String (names[m]) + " peak " + juce::String (p, 1) + " dBFS under modulation");
        }

        if (const auto dir = juce::SystemStats::getEnvironmentVariable ("VSX_FILTER_CSV", {}); dir.isNotEmpty())
        {
            const juce::File d (dir);
            d.createDirectory();
            constexpr int n = 300;
            float fr[n], mag[n];
            for (int i = 0; i < n; ++i) fr[i] = 20.0f * std::pow (1000.0f, (float) i / (n - 1));
            for (int m = 0; m < (int) FilterModel::Count; ++m)
            {
                juce::String csv = "hz";
                for (float r : { 0.0f, 0.3f, 0.6f, 0.9f }) csv << ",res" << juce::String (r, 1);
                csv << "\n";
                std::vector<std::array<float, n>> rows;
                for (float r : { 0.0f, 0.3f, 0.6f, 0.9f })
                {
                    measureFilterResponse (m, 0, 1000.0f, r, 0.0f, 0.5f, sr, fr, mag, n);
                    std::array<float, n> a {};
                    std::copy (mag, mag + n, a.begin());
                    rows.push_back (a);
                }
                for (int i = 0; i < n; ++i)
                {
                    csv << juce::String (fr[i], 2);
                    for (auto& rr : rows) csv << "," << juce::String (rr[(size_t) i], 2);
                    csv << "\n";
                }
                d.getChildFile ("filter_" + juce::String (names[m]).replace (" ", "_").toLowerCase() + ".csv").replaceWithText (csv);
            }
            juce::String thdCsv = "model,d0.0,d0.3,d0.6,d1.0\n";
            for (int m = 0; m < (int) FilterModel::Count; ++m)
            {
                thdCsv << names[m];
                for (float dd : { 0.0f, 0.3f, 0.6f, 1.0f }) thdCsv << "," << juce::String (thd (m, dd, 0.5f), 3);
                thdCsv << "\n";
            }
            d.getChildFile ("filter_thd.csv").replaceWithText (thdCsv);
        }
    }
};

static FilterQualityTests filterQualityTests;
} // namespace vsx
