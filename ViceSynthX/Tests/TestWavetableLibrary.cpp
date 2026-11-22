#include "../Source/Synth/Wavetable.h"
#include <cmath>

/*
    Factory wavetable library: 500 tables (27 classic + generated families). Every table is finite, not silent and
    distinct: the fingerprint of a table is the level (dB, relative to the frame's strongest harmonic, floor -60 dB) of
    harmonics 1-64 in four frames (start, 1/3, 2/3, end); two tables are near-duplicates when their fingerprints
    differ by less than the limit (RMS over the 256 values).
*/
namespace vsx
{
class WavetableLibraryTests final : public juce::UnitTest
{
public:
    WavetableLibraryTests() : juce::UnitTest ("Wavetable library", "VICE SYNTH X") {}

    static std::vector<float> fingerprint (const std::vector<std::vector<float>>& frames)
    {
        std::vector<float> fp;
        juce::dsp::FFT fft (11);
        const int n = (int) frames.size();
        for (int q = 0; q < 4; ++q)
        {
            const auto& fr = frames[(size_t) std::min (n - 1, (n - 1) * q / 3)];
            std::vector<float> buf (4096, 0.0f);
            for (size_t i = 0; i < 2048 && i < fr.size(); ++i) buf[i] = fr[i];
            fft.performFrequencyOnlyForwardTransform (buf.data());
            float mx = 1.0e-12f;
            for (int k = 1; k <= 64; ++k) mx = std::max (mx, buf[(size_t) k]);
            for (int k = 1; k <= 64; ++k) fp.push_back (std::max (-60.0f, 20.0f * std::log10 (buf[(size_t) k] / mx + 1.0e-9f)));
        }
        return fp;
    }

    void runTest() override
    {
        beginTest ("500 tables: unique names, categories, finite, not silent, distinct");
        const auto names = WavetableFactory::names();
        logMessage ("MEASURE|wavetables|factory tables|" + juce::String (names.size()) + "|count");
        expectEquals (names.size(), 500, "500 factory wavetables");
        juce::StringArray unique;
        for (const auto& n : names) unique.addIfNotAlreadyThere (n);
        expectEquals (unique.size(), names.size(), "unique names");
        for (const auto& c : WavetableFactory::categories())
        {
            int count = 0;
            for (const auto& n : names) if (WavetableFactory::category (n) == c) ++count;
            logMessage ("MEASURE|wavetables|category " + c + "|" + juce::String (count) + "|tables");
        }

        std::vector<std::vector<float>> fps;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        int bad = 0;
        for (const auto& n : names)
        {
            expect (WavetableFactory::category (n).isNotEmpty(), n + ": no category");
            const auto frames = WavetableFactory::generateFrames (n);
            bool ok = frames.size() >= 2;
            float energy = 0.0f;
            for (const auto& f : frames)
            {
                ok = ok && (int) f.size() == Wavetable::kSize;
                for (float v : f) { ok = ok && std::isfinite (v); energy += v * v; }
            }
            ok = ok && energy > 1.0f;
            if (! ok) { ++bad; expect (false, n + ": empty, non-finite or silent"); }
            fps.push_back (ok ? fingerprint (frames) : std::vector<float> (256, 0.0f));
        }
        logMessage ("MEASURE|wavetables|generate all 500 (frames only)|" + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 2) + "|s");
        expectEquals (bad, 0);

        struct Pair { double d; int a, b; };
        std::vector<Pair> pairs;
        for (int a = 0; a < (int) fps.size(); ++a)
            for (int b = a + 1; b < (int) fps.size(); ++b)
            {
                double s = 0.0;
                for (size_t i = 0; i < fps[(size_t) a].size(); ++i) { const double d = fps[(size_t) a][i] - fps[(size_t) b][i]; s += d * d; }
                pairs.push_back ({ std::sqrt (s / (double) fps[(size_t) a].size()), a, b });
            }
        std::sort (pairs.begin(), pairs.end(), [] (const Pair& x, const Pair& y) { return x.d < y.d; });
        for (int i = 0; i < (int) pairs.size() && (i < 5 || pairs[(size_t) i].d < 3.0); ++i)
            logMessage ("MEASURE|wavetables|closest pair " + juce::String (i + 1) + ": " + names[pairs[(size_t) i].a] + " / " + names[pairs[(size_t) i].b]
                        + "|" + juce::String (pairs[(size_t) i].d, 2) + "|dB RMS");
        const double limit = 3.0;
        int dup = 0;
        for (const auto& p : pairs) if (p.d < limit) ++dup;
        logMessage ("MEASURE|wavetables|pairs closer than " + juce::String (limit, 1) + " dB RMS|" + juce::String (dup) + "|pairs");
        expectEquals (dup, 0, "near-duplicate tables");

        beginTest ("A generated table loads through the library (mip levels built)");
        juce::String err;
        auto wt = WavetableLibrary::instance().load ("factory:" + names[names.size() - 1], err);
        expect (wt != nullptr && wt->numFrames >= 2, err);
    }
};

static WavetableLibraryTests wavetableLibraryTests;
} // namespace vsx
