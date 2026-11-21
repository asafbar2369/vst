#include "../Source/Synth/Filters.h"
#include "LegacyFilters.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    Matched A/B of every filter model, previous implementation (bf6d707, Tests/LegacyFilters.*) vs current, on the same
    dry deterministic fixture: band-limited saw bass line + triad (original synthetic material), LP24, cutoff sweep
    200 Hz -> 6 kHz -> 200 Hz, resonance 0.6, drive 0.3, character 0.5, effects off, at 44.1 / 48 / 96 kHz (filters run
    at 2x, as in the default High quality).
    MEASURE|filterab|<model> @<rate> <metric>. With VSX_AUDIO_DIR, writes RMS-matched (-18 dBFS) WAV pairs
    filterab_<model>_{before,after}_<rate>.wav for listening (stereo-RMS matched, not LUFS).
*/
namespace vsx
{
class FilterABTests final : public juce::UnitTest
{
public:
    FilterABTests() : juce::UnitTest ("Filter A/B evidence", "VICE SYNTH X") {}

    static float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    /** Dry fixture at the oversampled rate: three seconds, saw bass line plus a sustained triad. */
    static std::vector<float> fixture (double srOs, int frames)
    {
        std::vector<float> x ((size_t) frames, 0.0f);
        const float bass[] = { 36, 36, 43, 41 }; // C2 C2 G2 F2, 0.75 s each
        const float triad[] = { 48, 52, 55 };
        std::array<float, 4> ph {};
        for (int i = 0; i < frames; ++i)
        {
            const double t = i / srOs;
            const int step = std::min (3, (int) (t / 0.75));
            const float notes[4] = { bass[step], triad[0], triad[1], triad[2] };
            float v = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                const float dt = (float) (440.0 * std::pow (2.0, (notes[k] - 69.0) / 12.0) / srOs);
                v += (k == 0 ? 0.5f : 0.22f) * (2.0f * ph[(size_t) k] - 1.0f - polyBlep (ph[(size_t) k], dt));
                ph[(size_t) k] += dt;
                if (ph[(size_t) k] >= 1.0f) ph[(size_t) k] -= 1.0f;
            }
            const float env = (float) std::min (1.0, t * 200.0) * (float) std::min (1.0, (3.0 - t) * 50.0);
            x[(size_t) i] = v * env;
        }
        return x;
    }

    template <typename F>
    static juce::AudioBuffer<float> run (F& f, int model, const std::vector<float>& x, double srOs, double srOut)
    {
        f.prepare (srOs);
        f.setModel (model, 0);
        f.setTolerance (0.0f, 1);
        const int frames = (int) x.size();
        std::vector<float> y ((size_t) frames);
        for (int i = 0; i < frames; i += 16)
        {
            const double t = i / srOs;
            const double u = t < 1.5 ? t / 1.5 : (3.0 - t) / 1.5; // up and down
            f.update ((float) (200.0 * std::pow (30.0, u)), 0.6f, 0.3f, 0.5f, 16);
            for (int j = i; j < std::min (frames, i + 16); ++j)
            {
                float l = x[(size_t) j], r = l;
                f.process (l, r);
                y[(size_t) j] = l;
            }
        }
        // 2:1 decimation with a simple half-band average (identical for both versions)
        const int outFrames = (int) (frames * srOut / srOs);
        juce::AudioBuffer<float> b (2, outFrames);
        for (int i = 0; i < outFrames; ++i)
        {
            const int k = i * 2;
            const float v = 0.25f * y[(size_t) k] + 0.5f * y[(size_t) std::min (frames - 1, k + 1)] + 0.25f * y[(size_t) std::min (frames - 1, k + 2)];
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    static float rmsDb (const juce::AudioBuffer<float>& b) { return juce::Decibels::gainToDecibels (b.getRMSLevel (0, 0, b.getNumSamples()), -150.0f); }

    static float centroid (const juce::AudioBuffer<float>& b, double sr)
    {
        constexpr int order = 14, n = 1 << order;
        juce::dsp::FFT fft (order);
        double num = 0, den = 0;
        for (int start = 0; start + n <= b.getNumSamples(); start += n / 2)
        {
            std::vector<float> d ((size_t) n * 2, 0.0f);
            for (int i = 0; i < n; ++i) d[(size_t) i] = b.getSample (0, start + i) * (0.5f - 0.5f * std::cos (6.2831853f * i / n));
            fft.performFrequencyOnlyForwardTransform (d.data());
            for (int k = 1; k < n / 2; ++k) { num += k * sr / n * d[(size_t) k]; den += d[(size_t) k]; }
        }
        return (float) (num / std::max (1.0e-9, den));
    }

    static void writeWav (const juce::File& f, juce::AudioBuffer<float> b, double sr)
    {
        const float g = juce::Decibels::decibelsToGain (-18.0f) / std::max (1.0e-9f, b.getRMSLevel (0, 0, b.getNumSamples()));
        b.applyGain (g);
        f.deleteFile();
        auto os = std::make_unique<juce::FileOutputStream> (f);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 16, {}, 0));
        if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
    }

    void runTest() override
    {
        const char* names[] = { "mg_ladder", "diode_ladder", "acid_diode", "ms_sallen_key", "jp_ota", "jn_ota", "p5_cascade", "sem_state_var", "analog_svf" };
        const auto dir = juce::SystemStats::getEnvironmentVariable ("VSX_AUDIO_DIR", {});
        for (const double sr : { 44100.0, 48000.0, 96000.0 })
        {
            beginTest ("Matched A/B fixture through every filter model @ " + juce::String ((int) sr));
            const double srOs = sr * 2.0;
            const auto x = fixture (srOs, (int) (3.0 * srOs));
            for (int m = 0; m < (int) FilterModel::Count; ++m)
            {
                legacy::AnalogFilter before;
                AnalogFilter after;
                const auto a = run (before, m, x, srOs, sr);
                const auto b = run (after, m, x, srOs, sr);
                bool finite = true;
                for (int i = 0; i < b.getNumSamples(); ++i) finite &= std::isfinite (b.getSample (0, i));
                expect (finite, names[m]);
                const auto id = juce::String (names[m]) + " @" + juce::String ((int) sr);
                const float pa = juce::Decibels::gainToDecibels (a.getMagnitude (0, 0, a.getNumSamples()));
                const float pb = juce::Decibels::gainToDecibels (b.getMagnitude (0, 0, b.getNumSamples()));
                logMessage ("MEASURE|filterab|" + id + " RMS before / after|" + juce::String (rmsDb (a), 2) + " / " + juce::String (rmsDb (b), 2) + "|dBFS");
                logMessage ("MEASURE|filterab|" + id + " peak before / after|" + juce::String (pa, 2) + " / " + juce::String (pb, 2) + "|dBFS");
                logMessage ("MEASURE|filterab|" + id + " centroid before / after|" + juce::String (centroid (a, sr), 0) + " / " + juce::String (centroid (b, sr), 0) + "|Hz");
                expect (pb < 6.0f, id + " peak " + juce::String (pb, 1));
                if (dir.isNotEmpty())
                {
                    const juce::File d (dir);
                    d.createDirectory();
                    writeWav (d.getChildFile ("filterab_" + juce::String (names[m]) + "_before_" + juce::String ((int) sr) + ".wav"), a, sr);
                    writeWav (d.getChildFile ("filterab_" + juce::String (names[m]) + "_after_" + juce::String ((int) sr) + ".wav"), b, sr);
                }
            }
        }
    }
};

static FilterABTests filterABTests;
} // namespace vsx
