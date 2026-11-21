#include "../Source/Synth/ExtraEngines.h"
#include "../Source/Analysis/SoundMatcher.h"
#include "../Source/AI/PatchOps.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    Physical (waveguide string), Modal, Phase Dist and Additive oscillator modes: measured behaviour.
    MEASURE|engines|... lines carry the numbers.
*/
namespace vsx
{
class ExtraEngineTests final : public juce::UnitTest
{
public:
    ExtraEngineTests() : juce::UnitTest ("Extra synthesis engines", "VICE SYNTH X") {}

    // period (samples) by normalised autocorrelation around the expected period, parabolic interpolation
    static double measurePeriod (const std::vector<float>& x, int start, int len, double expected)
    {
        const int lo = std::max (2, (int) (expected * 0.94)), hi = (int) (expected * 1.06) + 2;
        double best = -1.0; int bestLag = lo;
        std::vector<double> c ((size_t) hi + 2, 0.0);
        for (int lag = lo - 1; lag <= hi + 1; ++lag)
        {
            double num = 0, e0 = 0, e1 = 0;
            for (int i = start; i < start + len; ++i)
            {
                num += (double) x[(size_t) i] * x[(size_t) (i + lag)];
                e0 += (double) x[(size_t) i] * x[(size_t) i];
                e1 += (double) x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
            }
            c[(size_t) lag] = num / std::sqrt (e0 * e1 + 1e-30);
            if (lag >= lo && lag <= hi && c[(size_t) lag] > best) { best = c[(size_t) lag]; bestLag = lag; }
        }
        const double a = c[(size_t) bestLag - 1], b = c[(size_t) bestLag], d = c[(size_t) bestLag + 1];
        const double den = a - 2 * b + d;
        return bestLag + (std::abs (den) > 1e-12 ? 0.5 * (a - d) / den : 0.0);
    }

    static double rmsDb (const std::vector<float>& x, int start, int len)
    {
        double e = 0; for (int i = start; i < start + len; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
        return 10.0 * std::log10 (e / len + 1e-30);
    }

    // magnitude spectrum (Blackman-Harris), returns power per bin
    static std::vector<double> spectrum (const std::vector<float>& x, int start, int order)
    {
        const int n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buf ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / (n - 1), tp = 2.0 * juce::MathConstants<double>::pi * t;
            const double w = 0.35875 - 0.48829 * std::cos (tp) + 0.14128 * std::cos (2 * tp) - 0.01168 * std::cos (3 * tp);
            buf[(size_t) i] = (float) (x[(size_t) (start + i)] * w);
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        std::vector<double> p ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i) p[(size_t) i] = (double) buf[(size_t) i] * buf[(size_t) i];
        return p;
    }

    static double bandDb (const std::vector<double>& p, double hz, double binHz, int halfWidth = 3)
    {
        const int c = (int) std::lround (hz / binHz);
        double e = 0; for (int b = c - halfWidth; b <= c + halfWidth; ++b) if (b > 0 && b < (int) p.size()) e += p[(size_t) b];
        return 10.0 * std::log10 (e + 1e-30);
    }

    template <typename Fn> static std::vector<float> renderN (int n, Fn&& fn)
    {
        std::vector<float> v ((size_t) n);
        for (auto& s : v) s = fn();
        return v;
    }

    void runTest() override
    {
        string();
        modal();
        phaseDist();
        additiveBank();
        processor();
    }

    void string()
    {
        beginTest ("Physical: waveguide string tuning across the keyboard and sample rates");
        for (double sr : { 48000.0, 96000.0 })
            for (float stiffness : { 0.0f, 0.5f })
                for (int note : { 36, 48, 60, 72, 84, 96 })
                {
                    StringModel m;
                    m.prepare (sr);
                    PhysParams p;
                    p.decay = 4.0f; p.damp = 0.3f; p.inharm = stiffness; p.position = 0.21f; p.bright = 0.7f;
                    const float f = midiToHz ((float) note);
                    m.noteOn (f, 100, p, 7);
                    const auto x = renderN ((int) (sr * 0.6), [&] { return m.render(); });
                    const double period = measurePeriod (x, (int) (sr * 0.2), (int) (sr * 0.2), sr / f);
                    const double cents = 1200.0 * std::log2 ((sr / period) / f);
                    logMessage ("MEASURE|engines|string " + juce::String (sr / 1000.0, 0) + " kHz stiffness " + juce::String (stiffness, 1)
                                + " MIDI " + juce::String (note) + " pitch error|" + juce::String (cents, 2) + "|cents");
                    expect (std::abs (cents) < 3.0, "string tuning " + juce::String (cents, 2) + " cents at MIDI " + juce::String (note));
                }

        beginTest ("Physical: Decay control sets the 60 dB decay time (Damping 0: all partials decay together)");
        for (float t60 : { 0.5f, 1.0f, 3.0f })
        {
            const double sr = 48000.0;
            StringModel m;
            m.prepare (sr);
            PhysParams p;
            p.decay = t60; p.damp = 0.0f; p.inharm = 0.0f; p.position = 0.13f;
            m.noteOn (midiToHz (57.0f), 110, p, 3);
            const auto x = renderN ((int) (sr * (0.3 + t60)), [&] { return m.render(); });
            const int w = (int) (sr * 0.05);
            const double a = rmsDb (x, (int) (sr * 0.1), w), b = rmsDb (x, (int) (sr * (0.1 + t60 * 0.6)), w);
            const double slope = (b - a) / (t60 * 0.6); // dB per second
            const double measuredT60 = -60.0 / slope;
            logMessage ("MEASURE|engines|string requested T60 " + juce::String (t60, 1) + " s, measured|" + juce::String (measuredT60, 3) + "|s");
            expect (std::abs (measuredT60 - t60) < 0.15 * t60, "T60 " + juce::String (measuredT60, 3) + " for " + juce::String (t60));
        }

        beginTest ("Physical: stable and finite for every excitation and extreme settings");
        for (int ex = 0; ex < 3; ++ex)
            for (float damp : { 0.0f, 1.0f })
                for (float inh : { 0.0f, 1.0f })
                    for (int note : { 0, 24, 108, 127 })
                    {
                        StringModel m;
                        m.prepare (88200.0);
                        PhysParams p;
                        p.excite = ex; p.decay = 20.0f; p.damp = damp; p.inharm = inh; p.bright = 1.0f; p.position = 0.5f;
                        m.noteOn (midiToHz ((float) note), 127, p, 11);
                        // stability = no growth: after the attack the level must never rise (the loop gain is < 1 and
                        // the loss filter is 1 at DC). The peak bound is a sanity check for these extreme settings
                        // (no damping, 20 s decay, full brightness, MIDI 0 / 127): a plucked attack has a high crest factor.
                        float peak = 0.0f; bool finite = true;
                        double early = 0.0, late = 0.0;
                        for (int i = 0; i < 88200 * 2; ++i)
                        {
                            const float y = m.render();
                            finite &= std::isfinite (y);
                            peak = std::max (peak, std::abs (y));
                            if (i >= 8820 && i < 17640) early += (double) y * y;
                            if (i >= 88200 * 2 - 8820) late += (double) y * y;
                        }
                        expect (finite && peak < 8.0f, "excite " + juce::String (ex) + " note " + juce::String (note) + " peak " + juce::String (peak));
                        if (ex != 2) expect (late <= early * 1.001 + 1e-12, "no growth: excite " + juce::String (ex) + " note " + juce::String (note));
                    }
    }

    void modal()
    {
        beginTest ("Modal: spectral peaks at the material's mode ratios");
        for (int material : { 2, 6, 7 }) // free bar, membrane, tine
        {
            const double sr = 48000.0;
            ModalModel m;
            m.prepare (sr);
            PhysParams p;
            p.excite = 0; p.material = material; p.decay = 4.0f; p.damp = 0.0f; p.bright = 1.0f; p.position = 0.13f;
            const float f0 = 110.0f;
            m.noteOn (f0, 120, p, 5);
            const auto x = renderN ((1 << 16) + 2000, [&] { return m.render(); });
            const auto spec = spectrum (x, 1000, 16);
            const double binHz = sr / (1 << 16);
            const float* r = nullptr;
            const int n = ModalModel::ratios (material, r);
            for (int k = 0; k < std::min (3, n); ++k)
            {
                const double want = f0 * r[k];
                // the strongest bin within +/- 2 % of the expected mode, then parabolic refinement
                const int lo = (int) (want * 0.98 / binHz), hi = (int) (want * 1.02 / binHz) + 1;
                int best = lo;
                for (int b = lo; b <= hi; ++b) if (spec[(size_t) b] > spec[(size_t) best]) best = b;
                const double a = std::log (spec[(size_t) best - 1] + 1e-30), b0 = std::log (spec[(size_t) best] + 1e-30), c = std::log (spec[(size_t) best + 1] + 1e-30);
                const double peakHz = (best + 0.5 * (a - c) / (a - 2 * b0 + c)) * binHz;
                const double err = (peakHz - want) / want * 100.0;
                logMessage ("MEASURE|engines|modal material " + juce::String (material) + " mode " + juce::String (k + 1) + " frequency error|" + juce::String (err, 3) + "|%");
                expect (std::abs (err) < 0.5, "mode " + juce::String (k + 1) + " error " + juce::String (err, 3) + " %");
            }
        }

        beginTest ("Modal: stable for every material, excitation and the full pitch range");
        for (int material = 0; material < ModalModel::kMaterials; ++material)
            for (int ex = 0; ex < 3; ++ex)
                for (int note : { 0, 60, 127 })
                {
                    ModalModel m;
                    m.prepare (96000.0);
                    PhysParams p;
                    p.excite = ex; p.material = material; p.decay = 20.0f; p.damp = 1.0f; p.inharm = 1.0f; p.bright = 1.0f;
                    m.noteOn (midiToHz ((float) note), 127, p, 2);
                    float peak = 0.0f; bool finite = true;
                    for (int i = 0; i < 48000; ++i) { const float y = m.render(); finite &= std::isfinite (y); peak = std::max (peak, std::abs (y)); }
                    expect (finite && peak < 4.0f, "material " + juce::String (material) + " peak " + juce::String (peak));
                }
    }

    void phaseDist()
    {
        beginTest ("Phase Dist: Amount 0 is a pure sine for every shape; all shapes continuous at the wrap");
        for (int shape = 0; shape < PhaseDistortion::kShapes; ++shape)
        {
            const double sr = 48000.0;
            PhaseDistortion pd;
            pd.prepare (sr);
            pd.noteOn (0.0f);
            const float f = 375.0f; // exactly 128 samples per cycle at 48 kHz: harmonics fall on FFT bins
            const auto x = renderN (1 << 15, [&] { return pd.render (f / (float) sr, shape, 0.0f); });
            const auto spec = spectrum (x, 0, 15);
            const double binHz = sr / (1 << 15);
            const double h1 = bandDb (spec, f, binHz);
            double hRest = 0;
            for (int k = 2; k <= 20; ++k) hRest += std::pow (10.0, bandDb (spec, f * k, binHz) / 10.0);
            const double thd = 10.0 * std::log10 (hRest + 1e-30) - h1;
            logMessage ("MEASURE|engines|phase dist shape " + juce::String (shape) + " THD at Amount 0|" + juce::String (thd, 1) + "|dB");
            expect (thd < -60.0, "shape " + juce::String (shape) + " THD " + juce::String (thd, 1));

            for (float amt : { 0.0f, 0.3f, 0.7f, 1.0f })
            {
                const float jump = std::abs (PhaseDistortion::shapeValue (0.99999f, shape, amt) - PhaseDistortion::shapeValue (0.0f, shape, amt));
                expect (jump < 0.02f, "shape " + juce::String (shape) + " amount " + juce::String (amt) + " jump " + juce::String (jump));
            }
            // full Amount must actually distort: 2nd or 3rd harmonic well above the sine's
            PhaseDistortion pd2;
            pd2.prepare (sr);
            pd2.noteOn (0.0f);
            const auto y = renderN (1 << 15, [&] { return pd2.render (f / (float) sr, shape, 1.0f); });
            const auto sp2 = spectrum (y, 0, 15);
            const double rel = std::max (bandDb (sp2, 2 * f, binHz), bandDb (sp2, 3 * f, binHz)) - bandDb (sp2, f, binHz);
            logMessage ("MEASURE|engines|phase dist shape " + juce::String (shape) + " strongest of H2/H3 at Amount 1|" + juce::String (rel, 1) + "|dB re H1");
            expect (rel > -25.0, "shape " + juce::String (shape) + " at full Amount adds harmonics: " + juce::String (rel, 1));
        }
    }

    void additiveBank()
    {
        beginTest ("Additive: odd-only spectrum, partial decay, equal level across spectra");
        const double sr = 48000.0;
        const float f = 187.5f; // 256 samples per cycle at 48 kHz
        {
            AdditiveBank a;
            a.prepare (sr);
            AdditiveBank::Params p;
            p.partials = 32; p.oddEven = -1.0f;
            a.noteOn (f, p, 1);
            const auto x = renderN (1 << 15, [&] { return a.render(); });
            const auto spec = spectrum (x, 0, 15);
            const double binHz = sr / (1 << 15);
            double worstEven = -300;
            for (int k = 2; k <= 16; k += 2) worstEven = std::max (worstEven, bandDb (spec, f * k, binHz) - bandDb (spec, f, binHz));
            logMessage ("MEASURE|engines|additive odd-only: strongest even partial|" + juce::String (worstEven, 1) + "|dB re fundamental");
            expect (worstEven < -50.0, "even partials " + juce::String (worstEven, 1));
        }
        {
            AdditiveBank a;
            a.prepare (sr);
            AdditiveBank::Params p;
            p.partials = 32; p.decay = 0.6f;
            a.noteOn (f, p, 1);
            const auto x = renderN ((int) sr * 2, [&] { return a.render(); });
            const auto early = spectrum (x, 4800, 14), late = spectrum (x, (int) (sr * 1.5), 14);
            const double binHz = sr / (1 << 14);
            const double lowDrop = bandDb (late, f, binHz) - bandDb (early, f, binHz);
            const double highDrop = bandDb (late, f * 12, binHz) - bandDb (early, f * 12, binHz);
            logMessage ("MEASURE|engines|additive partial decay over 1.4 s: fundamental / 12th partial|" + juce::String (lowDrop, 1) + " / " + juce::String (highDrop, 1) + "|dB");
            expect (highDrop < lowDrop - 20.0, "high partials decay faster");
        }
        {
            std::vector<double> levels;
            for (float tilt : { -1.0f, 0.0f, 1.0f })
            {
                AdditiveBank a;
                a.prepare (sr);
                AdditiveBank::Params p;
                p.partials = 48; p.tilt = tilt;
                a.noteOn (f, p, 1);
                const auto x = renderN ((int) sr / 2, [&] { return a.render(); });
                levels.push_back (rmsDb (x, 4800, 9600));
            }
            const double spread = *std::max_element (levels.begin(), levels.end()) - *std::min_element (levels.begin(), levels.end());
            logMessage ("MEASURE|engines|additive level spread across tilt -1 / 0 / +1|" + juce::String (spread, 2) + "|dB");
            expect (spread < 1.5, "level normalised: spread " + juce::String (spread, 2));
        }
    }

    void processor()
    {
        beginTest ("New oscillator modes in the synth: render, level vs the analog saw, preset round trip");
        auto r = std::make_unique<OfflineRenderer> (48000.0);
        auto base = PatchOps::initPatch();
        base.set ("osc_drift", 0.0f);
        // loudest 400 ms RMS, the measure of the library calibration: percussive engines decay, so an average over a
        // fixed window would compare a plucked attack with a sustained saw
        auto loudest = [] (const juce::AudioBuffer<float>& b)
        {
            const int win = 19200, hop = 2400;
            double best = 0.0;
            for (int st = 0; st + win <= b.getNumSamples(); st += hop)
            {
                double e = 0.0;
                for (int i = st; i < st + win; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i);
                best = std::max (best, e / win);
            }
            return (float) (10.0 * std::log10 (best + 1e-30));
        };
        const auto saw = r->render (base, 57, 100, 1.0, 0.3, 1);
        const float sawDb = loudest (saw);
        for (const char* mode : { "Physical", "Modal", "Phase Dist", "Additive" })
        {
            auto s = base;
            expect (s.setChoice ("osc_a_mode", mode), mode);
            s.set ("osc_a_phys_decay", 6.0f); // sustained enough to compare levels in the first 0.5 s
            const auto out = r->render (s, 57, 100, 1.0, 0.3, 1);
            float pk = 0.0f; bool finite = true;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < out.getNumSamples(); ++i) { const float v = out.getSample (c, i); finite &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
            const float db = loudest (out);
            logMessage ("MEASURE|engines|" + juce::String (mode) + " INIT loudest-400 ms level vs analog saw|" + juce::String (db - sawDb, 2) + "|dB");
            expect (finite, mode);
            expect (std::abs (db - sawDb) < 6.0f, juce::String (mode) + " level " + juce::String (db - sawDb, 2) + " dB vs saw");

            s.set ("osc_a_phys_material", 5.0f);
            s.set ("osc_a_pd_shape", 3.0f);
            s.set ("osc_a_pd_amount", 0.77f);
            s.set ("osc_a_add_decay", 0.4f);
            PatchSpec back;
            juce::String err;
            expect (PatchSpec::fromJson (s.toJson(), back, err), "json: " + err);
            expect (back.get ("osc_a_mode") == s.get ("osc_a_mode") && back.get ("osc_a_phys_material") == 5.0f
                    && std::abs (back.get ("osc_a_pd_amount") - 0.77f) < 1.0e-4f && std::abs (back.get ("osc_a_add_decay") - 0.4f) < 1.0e-4f,
                    juce::String (mode) + ": preset round trip");
        }
    }
};

static ExtraEngineTests extraEngineTests;
} // namespace vsx
