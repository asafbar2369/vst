#include "../Source/FX/ReverbEngine.h"
#include "LegacyReverb.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

/*
    Reverb measurements (all objective, printed as MEASURE|reverb|<program>|<metric>|<value>|<unit>):
      rt60_500 / rt60_4k   Schroeder T30 (x2) in octave bands around 500 Hz and 4 kHz
      mixing_ms            time until the normalised echo density (Abel & Huang 2006) first reaches 0.9
      ring_corr            correlation of the detrended log spectra of two consecutive late windows
                           (0.3-0.6 s vs 0.6-0.9 s): persistent narrow modes ("metallic ringing") give high values,
                           a diffuse modulated tail gives low values
      tonal_bins           late-window FFT bins more than 12 dB above their 1/3-octave power average
      cpu_x_realtime       how many times faster than real time one instance runs (48 kHz stereo)
    Set VSX_AUDIO_DIR to also write matched-loudness WAV examples (legacy vs every program).
*/
namespace vsx
{
class ReverbMeasurementTests final : public juce::UnitTest
{
public:
    ReverbMeasurementTests() : juce::UnitTest ("Reverb measurements", "VICE SYNTH X") {}

    static constexpr double sr = 48000.0;

    template <typename Rev>
    static juce::AudioBuffer<float> render (Rev& rev, const juce::AudioBuffer<float>& in, const ReverbEngine::Settings& s)
    {
        juce::AudioBuffer<float> out (in);
        for (int pos = 0; pos < out.getNumSamples(); pos += 256)
        {
            const int n = std::min (256, out.getNumSamples() - pos);
            rev.process (out.getWritePointer (0) + pos, out.getWritePointer (1) + pos, n, s);
        }
        return out;
    }

    static juce::AudioBuffer<float> impulse (double seconds)
    {
        juce::AudioBuffer<float> b (2, (int) (seconds * sr));
        b.clear();
        b.setSample (0, 0, 1.0f);
        b.setSample (1, 0, 1.0f);
        return b;
    }

    static std::vector<float> mono (const juce::AudioBuffer<float>& b)
    {
        std::vector<float> m ((size_t) b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i) m[(size_t) i] = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        return m;
    }

    static std::vector<float> band (const std::vector<float>& x, float centre)
    {
        Biquad a, b;
        a.set (Biquad::BandPass, sr, centre, 1.41f);
        b.set (Biquad::BandPass, sr, centre, 1.41f);
        std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (a.process (x[i]));
        return y;
    }

    /** Schroeder T30 extrapolated to 60 dB; -1 if the decay never reaches -35 dB. */
    static float rt60 (const std::vector<float>& x)
    {
        std::vector<double> edc (x.size());
        double acc = 0.0;
        for (size_t i = x.size(); i-- > 0;) { acc += (double) x[i] * x[i]; edc[i] = acc; }
        if (acc <= 0.0) return -1.0f;
        int i5 = -1, i35 = -1;
        for (size_t i = 0; i < edc.size(); ++i)
        {
            const double db = 10.0 * std::log10 (std::max (1.0e-30, edc[i] / acc));
            if (i5 < 0 && db <= -5.0) i5 = (int) i;
            if (i35 < 0 && db <= -35.0) { i35 = (int) i; break; }
        }
        if (i5 < 0 || i35 < 0) return -1.0f;
        return (float) (2.0 * (i35 - i5) / sr);
    }

    /** Abel & Huang normalised echo density, 20 ms windows; returns ms when it first reaches 0.9 (or -1). */
    static float mixingTimeMs (const std::vector<float>& x)
    {
        const int w = (int) (0.02 * sr), hop = (int) (0.002 * sr);
        const double norm = std::erfc (1.0 / std::sqrt (2.0));
        for (int start = 0; start + w < (int) x.size(); start += hop)
        {
            double e = 0.0;
            for (int i = 0; i < w; ++i) e += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
            const double sd = std::sqrt (e / w);
            if (sd <= 0.0) continue;
            int outside = 0;
            for (int i = 0; i < w; ++i) outside += std::abs (x[(size_t) (start + i)]) > sd ? 1 : 0;
            if ((double) outside / w / norm >= 0.9) return (float) ((start + w / 2) * 1000.0 / sr);
        }
        return -1.0f;
    }

    static std::vector<float> logSpectrum (const std::vector<float>& x, int start, int order)
    {
        const int n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buf ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n && start + i < (int) x.size(); ++i)
            buf[(size_t) i] = x[(size_t) (start + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        std::vector<float> db ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i) db[(size_t) i] = 20.0f * std::log10 (std::max (1.0e-9f, buf[(size_t) i]));
        return db;
    }

    /** dB spectrum minus its 1/3-octave power average (fine structure only). */
    static std::vector<float> detrend (const std::vector<float>& db, int n)
    {
        std::vector<float> out (db.size(), 0.0f);
        for (size_t k = 1; k < db.size(); ++k)
        {
            const double f = k * sr / n;
            const int lo = std::max (1, (int) (f * 0.891 * n / sr)), hi = std::min ((int) db.size() - 1, (int) (f * 1.122 * n / sr) + 1);
            double p = 0.0;
            for (int j = lo; j <= hi; ++j) p += std::pow (10.0, db[(size_t) j] / 10.0);
            out[k] = db[k] - (float) (10.0 * std::log10 (p / (hi - lo + 1)));
        }
        return out;
    }

    struct Colour { float corr = 0.0f; int tonal = 0; };

    /** Comb periodicity: highest normalised autocorrelation of the 20-400 ms response at lags of 1-50 ms.
        Flutter / comb repetition (the "metallic" quality) gives high values; a diffuse response gives low values. */
    static float combPeak (const std::vector<float>& x)
    {
        const int a = (int) (0.02 * sr), b = std::min ((int) x.size() - (int) (0.05 * sr) - 1, (int) (0.4 * sr));
        double r0 = 0.0;
        for (int i = a; i < b; ++i) r0 += (double) x[(size_t) i] * x[(size_t) i];
        float best = 0.0f;
        for (int lag = (int) (0.001 * sr); lag <= (int) (0.05 * sr); ++lag)
        {
            double r = 0.0;
            for (int i = a; i < b; ++i) r += (double) x[(size_t) i] * x[(size_t) (i + lag)];
            best = std::max (best, (float) (r / std::max (1.0e-20, r0)));
        }
        return best;
    }

    static Colour colouration (const std::vector<float>& x)
    {
        constexpr int order = 14, n = 1 << order; // 0.34 s windows
        const auto a = detrend (logSpectrum (x, (int) (0.30 * sr), order), n);
        const auto b = detrend (logSpectrum (x, (int) (0.30 * sr) + n, order), n);
        const int k0 = (int) (200.0 * n / sr), k1 = (int) (8000.0 * n / sr);
        double ma = 0, mb = 0;
        for (int k = k0; k < k1; ++k) { ma += a[(size_t) k]; mb += b[(size_t) k]; }
        ma /= (k1 - k0); mb /= (k1 - k0);
        double sab = 0, saa = 0, sbb = 0;
        Colour c;
        for (int k = k0; k < k1; ++k)
        {
            const double da = a[(size_t) k] - ma, d2 = b[(size_t) k] - mb;
            sab += da * d2; saa += da * da; sbb += d2 * d2;
            c.tonal += a[(size_t) k] > 12.0f ? 1 : 0;
        }
        c.corr = (float) (sab / std::sqrt (std::max (1.0e-12, saa * sbb)));
        return c;
    }

    void measure (const juce::String& name, const std::vector<float>& ir)
    {
        auto line = [&] (const juce::String& item, double v, const juce::String& unit)
        { logMessage ("MEASURE|reverb|" + name + " " + item + "|" + juce::String (v, 3) + "|" + unit); };
        line ("rt60_500", rt60 (band (ir, 500.0f)), "s");
        line ("rt60_4k", rt60 (band (ir, 4000.0f)), "s");
        line ("mixing_ms", mixingTimeMs (ir), "ms");
        const auto c = colouration (ir);
        line ("ring_corr", c.corr, "");
        line ("tonal_bins", c.tonal, "bins");
        line ("comb_peak", combPeak (ir), "");
    }

    static double energy (const juce::AudioBuffer<float>& b)
    {
        double e = 0.0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i) e += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return e;
    }

    /** Dry test phrase: plucked harmonic tones and a noise burst (synthetic, original). */
    static juce::AudioBuffer<float> phrase()
    {
        juce::AudioBuffer<float> b (2, (int) (6.0 * sr));
        b.clear();
        juce::Random rng (7);
        const float notes[] = { 220.0f, 277.18f, 329.63f, 440.0f };
        for (int k = 0; k < 4; ++k)
        {
            const int start = (int) (k * 0.55 * sr);
            for (int i = 0; i < (int) (0.5 * sr); ++i)
            {
                const float t = (float) i / (float) sr;
                float v = 0.0f;
                for (int h = 1; h <= 12; ++h) v += std::sin (juce::MathConstants<float>::twoPi * notes[k] * (float) h * t) / (float) h * std::exp (-t * (3.0f + (float) h));
                b.addSample (0, start + i, 0.25f * v);
                b.addSample (1, start + i, 0.25f * v);
            }
        }
        const int burst = (int) (2.4 * sr);
        for (int i = 0; i < (int) (0.08 * sr); ++i)
        {
            const float env = std::exp (-(float) i / (float) (0.015 * sr));
            b.addSample (0, burst + i, 0.35f * env * (rng.nextFloat() * 2.0f - 1.0f));
            b.addSample (1, burst + i, 0.35f * env * (rng.nextFloat() * 2.0f - 1.0f));
        }
        return b;
    }

    static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, int bits = 16)
    {
        f.deleteFile();
        auto os = std::make_unique<juce::FileOutputStream> (f);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, bits, {}, 0));
        if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
    }

    void runTest() override
    {
        const char* names[] = { "Hall", "Plate", "Room", "Chamber", "Ambient", "Nonlinear", "Gated", "Reverse" };
        ReverbEngine::Settings defaults;
        defaults.predelayMs = 0.0f;

        beginTest ("Legacy reverb (before) measurements");
        double legacyEnergy = 0.0;
        {
            LegacyReverb legacy;
            legacy.prepare (sr);
            const auto out = render (legacy, impulse (4.0), defaults);
            legacyEnergy = energy (out);
            measure ("Legacy", mono (out));
        }

        beginTest ("Every program: bounded, finite, decays, measured");
        std::array<double, ReverbEngine::NumPrograms> energies {};
        for (int p = 0; p < ReverbEngine::NumPrograms; ++p)
        {
            ReverbEngine rev;
            rev.prepare (sr);
            auto s = defaults;
            s.program = p;
            const auto out = render (rev, impulse (5.0), s);
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (ch, i));
            expect (finite, names[p]);
            expect (out.getMagnitude (0, out.getNumSamples()) < 2.0f, juce::String (names[p]) + " impulse response bounded");
            const auto tailRms = out.getRMSLevel (0, out.getNumSamples() - (int) (0.2 * sr), (int) (0.2 * sr));
            expect (tailRms < 1.0e-3f, juce::String (names[p]) + " decays within 5 s: " + juce::String (tailRms));
            energies[(size_t) p] = energy (out);
            logMessage ("MEASURE|reverb|" + juce::String (names[p]) + " energy_vs_legacy|"
                        + juce::String (10.0 * std::log10 (energies[(size_t) p] / legacyEnergy), 2) + "|dB");
            measure (names[p], mono (out));
        }

        if (const auto irDir = juce::SystemStats::getEnvironmentVariable ("VSX_IR_DIR", {}); irDir.isNotEmpty())
        {
            const juce::File d (irDir);
            d.createDirectory();
            LegacyReverb legacy;
            legacy.prepare (sr);
            writeWav (d.getChildFile ("ir_legacy.wav"), render (legacy, impulse (4.0), defaults), 24);
            for (int p = 0; p < ReverbEngine::NumPrograms; ++p)
            {
                ReverbEngine rev;
                rev.prepare (sr);
                auto s = defaults;
                s.program = p;
                writeWav (d.getChildFile ("ir_" + juce::String (names[p]).toLowerCase() + ".wav"), render (rev, impulse (4.0), s), 24);
            }
        }

        beginTest ("FDN programs follow the Decay setting (500 Hz band)");
        for (int p : { 0, 1, 3, 4 })
            for (float decay : { 1.5f, 3.0f, 6.0f })
            {
                ReverbEngine rev;
                rev.prepare (sr);
                auto s = defaults;
                s.program = p;
                s.decay = decay;
                const auto ir = mono (render (rev, impulse (decay * 1.6 + 1.0), s));
                const float t = rt60 (band (ir, 500.0f));
                expect (t > decay * 0.7f && t < decay * 1.3f, juce::String (names[p]) + " RT60 " + juce::String (t, 2) + " s for " + juce::String (decay) + " s");
            }

        beginTest ("Less comb periodicity than the legacy reverb; dense within 150 ms");
        {
            LegacyReverb legacy;
            legacy.prepare (sr);
            const float oldComb = combPeak (mono (render (legacy, impulse (2.0), defaults)));
            for (int p = 0; p < ReverbEngine::NumPrograms; ++p)
            {
                ReverbEngine rev;
                rev.prepare (sr);
                auto s = defaults;
                s.program = p;
                const auto ir = mono (render (rev, impulse (2.0), s));
                const float comb = combPeak (ir);
                expect (comb < oldComb * 0.8f, juce::String (names[p]) + " comb periodicity " + juce::String (comb, 3) + " vs legacy " + juce::String (oldComb, 3));
                if (p <= ReverbEngine::Ambient)
                {
                    const float mix = mixingTimeMs (ir);
                    expect (mix > 0.0f && mix < 150.0f, juce::String (names[p]) + " mixing time " + juce::String (mix) + " ms");
                    expect (colouration (ir).corr < 0.6f, juce::String (names[p]) + " late spectral persistence");
                }
            }
        }

        beginTest ("Gated, Nonlinear and Reverse envelopes");
        {
            ReverbEngine rev;
            rev.prepare (sr);
            auto s = defaults;
            s.program = ReverbEngine::Gated;
            juce::AudioBuffer<float> burst (2, (int) (2.0 * sr));
            burst.clear();
            for (int i = 0; i < (int) (0.05 * sr); ++i) { burst.setSample (0, i, 0.5f); burst.setSample (1, i, -0.5f); }
            const auto g = render (rev, burst, s);
            const float during = g.getRMSLevel (0, (int) (0.1 * sr), (int) (0.2 * sr));
            const float after = g.getRMSLevel (0, (int) (0.7 * sr), (int) (0.5 * sr));
            expect (during > 0.01f && after < during * 0.01f, "gate closes after the hold time: " + juce::String (during) + " -> " + juce::String (after));

            ReverbEngine r2;
            r2.prepare (sr);
            s.program = ReverbEngine::Reverse;
            const auto rv = render (r2, impulse (3.0), s);
            const float first = rv.getRMSLevel (0, 0, (int) (0.3 * sr)), second = rv.getRMSLevel (0, (int) (0.3 * sr), (int) (0.3 * sr));
            expect (second > first * 1.5f, "reverse swells: " + juce::String (first) + " -> " + juce::String (second));

            ReverbEngine r3;
            r3.prepare (sr);
            s.program = ReverbEngine::Nonlinear;
            const auto nl = render (r3, impulse (3.0), s);
            const float body = nl.getRMSLevel (0, (int) (0.05 * sr), (int) (0.25 * sr));
            const float end = nl.getRMSLevel (0, (int) (1.0 * sr), (int) (0.5 * sr));
            expect (body > 0.0f && end < body * 0.01f, "nonlinear ends abruptly: " + juce::String (body) + " -> " + juce::String (end));
        }

        beginTest ("Program switching and automation stay finite");
        {
            ReverbEngine rev;
            rev.prepare (sr);
            juce::Random rng (3);
            juce::AudioBuffer<float> noise (2, 256);
            bool finite = true;
            float peak = 0.0f;
            for (int block = 0; block < 2000; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i) noise.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.3f);
                ReverbEngine::Settings s;
                s.program = (block / 150) % ReverbEngine::NumPrograms;
                s.size = 0.5f + 0.5f * std::sin ((float) block * 0.05f);
                s.decay = 0.2f + 29.0f * (float) ((block * 7) % 100) / 100.0f;
                s.damping = rng.nextFloat();
                s.diffusion = rng.nextFloat();
                s.modulation = rng.nextFloat();
                rev.process (noise.getWritePointer (0), noise.getWritePointer (1), 256, s);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i)
                    {
                        finite &= std::isfinite (noise.getSample (ch, i));
                        peak = std::max (peak, std::abs (noise.getSample (ch, i)));
                    }
            }
            expect (finite);
            expect (peak < 20.0f, "peak " + juce::String (peak));
        }

        beginTest ("CPU");
        for (int p = 0; p < ReverbEngine::NumPrograms; ++p)
        {
            ReverbEngine rev;
            rev.prepare (sr);
            ReverbEngine::Settings s;
            s.program = p;
            juce::AudioBuffer<float> buf (2, (int) (10.0 * sr));
            juce::Random rng (1);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i) buf.setSample (ch, i, rng.nextFloat() * 0.2f - 0.1f);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            render (rev, buf, s);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("MEASURE|reverb|" + juce::String (names[p]) + " cpu_x_realtime|" + juce::String (10000.0 / std::max (0.001, ms), 1) + "|x");
        }

        const auto dir = juce::SystemStats::getEnvironmentVariable ("VSX_AUDIO_DIR", {});
        if (dir.isNotEmpty())
        {
            beginTest ("Matched-loudness WAV examples");
            const juce::File out (dir);
            out.createDirectory();
            const auto dry = phrase();
            const float targetWetRms = 0.05f;
            auto writeMix = [&] (const juce::String& file, juce::AudioBuffer<float> wet)
            {
                const float rms = std::max (1.0e-6f, 0.5f * (wet.getRMSLevel (0, 0, wet.getNumSamples()) + wet.getRMSLevel (1, 0, wet.getNumSamples())));
                wet.applyGain (targetWetRms / rms);
                juce::AudioBuffer<float> mix (dry);
                for (int ch = 0; ch < 2; ++ch) mix.addFrom (ch, 0, wet, ch, 0, mix.getNumSamples());
                writeWav (out.getChildFile (file + ".wav"), mix);
                logMessage ("wrote " + file + ".wav (wet RMS matched to " + juce::String (juce::Decibels::gainToDecibels (targetWetRms), 1) + " dBFS)");
            };
            auto s = defaults;
            s.predelayMs = 20.0f;
            {
                LegacyReverb legacy;
                legacy.prepare (sr);
                writeMix ("reverb_00_legacy_before", render (legacy, dry, s));
            }
            for (int p = 0; p < ReverbEngine::NumPrograms; ++p)
            {
                ReverbEngine rev;
                rev.prepare (sr);
                s.program = p;
                writeMix ("reverb_" + juce::String (p + 1).paddedLeft ('0', 2) + "_" + juce::String (names[p]).toLowerCase(), render (rev, dry, s));
            }
        }
    }
};

static ReverbMeasurementTests reverbMeasurementTests;
} // namespace vsx
