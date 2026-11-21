#include "../Source/FX/Effects.h"
#include "../Source/Presets/SoundRecipes.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    Effects added 2026-11-21, each measured on its own (MEASURE|fx2|...): BBD chorus modulation rate and swing, BBD
    delay bandwidth vs delay time, shimmer pitch of the regenerated tail, rotary horn speed slow / fast, resonator pitch,
    transient shaper attack / sustain gain, spectral identity / freeze, stability at extreme settings, and how older
    presets' rack order is completed.
*/
namespace vsx
{
class ExtraEffectTests final : public juce::UnitTest
{
public:
    ExtraEffectTests() : juce::UnitTest ("Extra effects", "VICE SYNTH X") {}

    static constexpr double sr = 48000.0;

    static ParamSnapshot params()
    {
        ParamSnapshot p;
        p.value = Recipes::init().values;
        return p;
    }
    static void set (ParamSnapshot& p, int type, int param, float v) { p.value[(size_t) pid::FX (type, param)] = v; }

    static juce::AudioBuffer<float> run (int type, juce::AudioBuffer<float> buf, ParamSnapshot& p, Effect* keep = nullptr)
    {
        std::unique_ptr<Effect> own;
        Effect* fx = keep;
        if (fx == nullptr) { own = createEffect (type); fx = own.get(); fx->prepare (sr, 256); }
        ModOffsets mods;
        FxContext ctx { &p, &mods, 120.0 };
        for (int s = 0; s < buf.getNumSamples(); s += 256)
            fx->process (buf.getWritePointer (0, s), buf.getWritePointer (1, s), std::min (256, buf.getNumSamples() - s), ctx);
        return buf;
    }

    static juce::AudioBuffer<float> noise (int n, uint32_t seed = 3)
    {
        juce::AudioBuffer<float> b (2, n);
        Rng r (seed);
        for (int i = 0; i < n; ++i) { const float v = 0.3f * r.bi(); b.setSample (0, i, v); b.setSample (1, i, v); }
        return b;
    }

    static double rmsDb (const juce::AudioBuffer<float>& b, int ch, int a, int z)
    {
        double e = 0; for (int i = a; i < z; ++i) e += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return 10.0 * std::log10 (e / std::max (1, z - a) + 1e-30);
    }

    // windowed Goertzel level (dB) of f in channel ch, samples a..z
    static double tone (const juce::AudioBuffer<float>& b, int ch, int a, int z, double f)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        const double n = z - a - 1;
        for (int i = a; i < z; ++i)
        {
            const double t = 2.0 * juce::MathConstants<double>::pi * (i - a) / n;
            const double win = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
            const double s0 = b.getSample (ch, i) * win + c * s1 - s2; s2 = s1; s1 = s0;
        }
        return 10.0 * std::log10 ((s1 * s1 + s2 * s2 - c * s1 * s2) / (n * n) + 1e-30);
    }

    // energy (dB) in f0 +/- bw Hz, 16384-point FFT starting at sample a (Blackman-Harris)
    static double band (const juce::AudioBuffer<float>& b, int a, double f0, double bw)
    {
        juce::dsp::FFT fft (14);
        std::vector<float> buf (32768, 0.0f);
        for (int i = 0; i < 16384; ++i)
        {
            const double t = 2.0 * juce::MathConstants<double>::pi * i / 16383.0;
            const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
            buf[(size_t) i] = (float) (b.getSample (0, a + i) * w);
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        const double binHz = sr / 16384.0;
        double e = 0;
        for (int k = (int) ((f0 - bw) / binHz); k <= (int) ((f0 + bw) / binHz); ++k) e += (double) buf[(size_t) k] * buf[(size_t) k];
        return 10.0 * std::log10 (e + 1e-30);
    }

    void runTest() override
    {
        bbdChorus();
        bbdDelay();
        shimmer();
        rotary();
        resonator();
        transient();
        spectral();
        stability();
        rackOrder();
    }

    void bbdChorus()
    {
        beginTest ("BBD Chorus: modulation rate and delay swing per mode (cross-correlation delay tracking)");
        static const float rates[3] = { 0.513f, 0.863f, 9.75f };
        for (int mode : { 0, 1 })
        {
            auto p = params();
            set (p, pid::FxBbdChorus, pid::bbdchorus::Mode, (float) mode);
            set (p, pid::FxBbdChorus, pid::bbdchorus::Noise, 0.0f);
            set (p, pid::FxBbdChorus, pid::bbdchorus::Tone, 14000.0f);
            const auto in = noise ((int) (sr * 6));
            const auto out = run (pid::FxBbdChorus, in, p);
            // delay of the left output vs the input every 20 ms (peak of the cross-correlation, parabolic refinement)
            std::vector<double> delays;
            const int win = 1920;
            for (int s = 4800; s + win + 400 < in.getNumSamples(); s += 960)
            {
                double best = -1e30; int lag = 0; std::array<double, 400> cc {};
                for (int d = 40; d < 360; ++d)
                {
                    double acc = 0; for (int i = 0; i < win; ++i) acc += (double) in.getSample (0, s + i) * out.getSample (0, s + i + d);
                    cc[(size_t) d] = acc;
                    if (acc > best) { best = acc; lag = d; }
                }
                const double a = cc[(size_t) lag - 1], m = cc[(size_t) lag], c = cc[(size_t) lag + 1];
                delays.push_back ((lag + 0.5 * (a - c) / (a - 2 * m + c)) / sr * 1000.0);
            }
            const auto [mn, mx] = std::minmax_element (delays.begin(), delays.end());
            const double swing = 0.5 * (*mx - *mn);
            // rate: count crossings of the mean delay
            double mean = 0; for (double d : delays) mean += d; mean /= (double) delays.size();
            int crossings = 0; for (size_t i = 1; i < delays.size(); ++i) if ((delays[i - 1] - mean) * (delays[i] - mean) < 0) ++crossings;
            const double seconds = (double) delays.size() * 0.02;
            const double rate = crossings / 2.0 / seconds;
            logMessage ("MEASURE|fx2|BBD chorus mode " + juce::String (mode == 0 ? "I" : "II") + " rate / swing|" + juce::String (rate, 3) + " Hz / "
                        + juce::String (swing, 2) + " ms|");
            expectWithinAbsoluteError (rate, (double) rates[mode], 0.12, "LFO rate");
            expectWithinAbsoluteError (swing, 1.85, 0.25, "delay swing");
        }
    }

    void bbdDelay()
    {
        beginTest ("BBD Delay: echo bandwidth follows stages / (4 x time) - longer delays are darker");
        for (float ms : { 100.0f, 300.0f })
        {
            auto p = params();
            set (p, pid::FxBbdDelay, pid::bbddelay::Time, ms);
            set (p, pid::FxBbdDelay, pid::bbddelay::Feedback, 0.0f);
            set (p, pid::FxBbdDelay, pid::bbddelay::Noise, 0.0f);
            set (p, pid::FxBbdDelay, pid::bbddelay::Mod, 0.0f);
            set (p, pid::FxBbdDelay, pid::bbddelay::Stages, 2.0f); // 4096
            juce::AudioBuffer<float> in (2, (int) sr);
            in.clear();
            in.setSample (0, 100, 0.25f); in.setSample (1, 100, 0.25f);
            const auto out = run (pid::FxBbdDelay, in, p);
            // magnitude response of the first echo (8192-point FFT around it)
            const int start = 100 + (int) (ms * 0.001 * sr) - 2000;
            juce::dsp::FFT fft (13);
            std::vector<float> buf (16384, 0.0f);
            for (int i = 0; i < 8192; ++i) buf[(size_t) i] = out.getSample (0, start + i);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            const double binHz = sr / 8192.0;
            const double ref = buf[(size_t) std::lround (200.0 / binHz)];
            double cutoff = 0;
            for (int k = (int) (200.0 / binHz); k < 4096; ++k) if (buf[(size_t) k] < ref * 0.7071) { cutoff = k * binHz; break; }
            const double expected = 0.9 * 4096.0 / (4.0 * ms * 0.001); // the -3 dB point of two 2nd-order Butterworth sections is about 0.8 x fc
            logMessage ("MEASURE|fx2|BBD delay 4096 stages " + juce::String (ms, 0) + " ms: -3 dB bandwidth|" + juce::String (cutoff, 0) + "|Hz (fc "
                        + juce::String (expected, 0) + ")");
            expect (cutoff > 0.65 * expected && cutoff < 1.05 * expected, "bandwidth " + juce::String (cutoff, 0));
        }
    }

    void shimmer()
    {
        beginTest ("Shimmer: the regenerated tail contains the octave (+12 st); Shimmer 0 does not");
        double octave[2] {};
        for (float amount : { 0.0f, 1.0f })
        {
            auto p = params();
            set (p, pid::FxShimmer, pid::shimmer::Amount, amount);
            set (p, pid::FxShimmer, pid::shimmer::Decay, 8.0f);
            set (p, pid::FxShimmer, pid::shimmer::Pitch, 0.0f);
            juce::AudioBuffer<float> in (2, (int) (sr * 8));
            in.clear();
            for (int i = 0; i < (int) (0.3 * sr); ++i)
            {
                const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) i / (float) sr);
                in.setSample (0, i, v); in.setSample (1, i, v);
            }
            const auto out = run (pid::FxShimmer, in, p);
            // band energies around the fundamental and the octave (a granular shifter and a modulated reverb spread
            // each partial over several hertz, so single-bin levels under-read)
            const int a = (int) (2.0 * sr);
            const double f1 = band (out, a, 220.0, 40.0), f2 = band (out, a, 440.0, 40.0);
            logMessage ("MEASURE|fx2|shimmer " + juce::String (amount, 0) + ": 440 +/- 40 Hz re 220 +/- 40 Hz in the tail at 2.0-2.34 s|" + juce::String (f2 - f1, 1) + "|dB");
            octave[amount > 0.5f ? 1 : 0] = f2;
            if (amount < 0.5f) expect (f2 - f1 < -30.0, "no octave without shimmer: " + juce::String (f2 - f1, 1));
            // stability: the regenerating loop must decay (late tail below the early one), stay finite and within the
            // sanity bound used for every effect (this reverb program alone turns a 0.3 input into a 1.4 peak)
            bool finite = true; for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i));
            const double early = rmsDb (out, 0, (int) (1.0 * sr), (int) (2.0 * sr)), late = rmsDb (out, 0, (int) (7.0 * sr), (int) (8.0 * sr));
            const float peak = out.getMagnitude (0, out.getNumSamples());
            logMessage ("MEASURE|fx2|shimmer " + juce::String (amount, 0) + ": tail 1-2 s / 7-8 s, peak|" + juce::String (early, 1) + " / " + juce::String (late, 1)
                        + " dB, " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS|");
            expect (finite && peak < 8.0f && late < early - 20.0, "the shimmer loop decays");
        }
        // (first written as 'octave within 12 dB of the fundamental', a guess made before measuring: the property is
        // that the shimmer path creates the octave, measured as the difference Shimmer 100 % vs 0 %)
        logMessage ("MEASURE|fx2|shimmer octave band at 2.0 s, Shimmer 100 % vs 0 %|" + juce::String (octave[1] - octave[0], 1) + "|dB");
        expect (octave[1] - octave[0] > 20.0, "the shimmer path regenerates the octave: +" + juce::String (octave[1] - octave[0], 1) + " dB");
    }

    void rotary()
    {
        beginTest ("Rotary: horn amplitude-modulation rate slow / fast (2 kHz tone, horn band)");
        auto measureAm = [&] (const juce::AudioBuffer<float>& out, int a, int z)
        {
            // envelope = peak per 2 ms; count crossings of its mean
            std::vector<double> env;
            for (int s = a; s + 96 <= z; s += 96) env.push_back (out.getMagnitude (0, s, 96));
            double mean = 0; for (double e : env) mean += e; mean /= (double) env.size();
            int crossings = 0; for (size_t i = 1; i < env.size(); ++i) if ((env[i - 1] - mean) * (env[i] - mean) < 0) ++crossings;
            return crossings / 2.0 / ((double) (z - a) / sr);
        };
        auto p = params();
        set (p, pid::FxRotary, pid::rotary::Balance, 1.0f); // horn only
        set (p, pid::FxRotary, pid::rotary::Drive, 0.0f);
        auto fx = createEffect (pid::FxRotary);
        fx->prepare (sr, 256);
        juce::AudioBuffer<float> in (2, (int) (sr * 4));
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 2000.0f * (float) i / (float) sr);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        const auto slow = run (pid::FxRotary, in, p, fx.get());
        const double slowHz = measureAm (slow, (int) (0.5 * sr), slow.getNumSamples());
        set (p, pid::FxRotary, pid::rotary::Speed, 1.0f);
        run (pid::FxRotary, in, p, fx.get()); // 4 s to spin up
        const auto fast = run (pid::FxRotary, in, p, fx.get());
        const double fastHz = measureAm (fast, 0, fast.getNumSamples());
        logMessage ("MEASURE|fx2|rotary horn rate slow / fast|" + juce::String (slowHz, 2) + " / " + juce::String (fastHz, 2) + "|Hz (0.8 / 6.8)");
        expectWithinAbsoluteError (slowHz, 0.8, 0.15, "slow");
        expectWithinAbsoluteError (fastHz, 6.8, 0.5, "fast");
    }

    void resonator()
    {
        beginTest ("Resonator: noise in, resonance at the set note");
        auto p = params();
        set (p, pid::FxResonator, pid::resonator::Note, 57.0f); // A3 = 220 Hz
        set (p, pid::FxResonator, pid::resonator::Spread, 0.0f);
        set (p, pid::FxResonator, pid::resonator::Decay, 2.0f);
        const auto out = run (pid::FxResonator, noise ((int) (sr * 2)), p);
        double best = -1e9, bestHz = 0;
        for (double f = 180.0; f <= 260.0; f += 0.5) { const double l = tone (out, 0, (int) (0.5 * sr), (int) (2.0 * sr), f); if (l > best) { best = l; bestHz = f; } }
        const double cents = 1200.0 * std::log2 (bestHz / 220.0);
        const double offPeak = tone (out, 0, (int) (0.5 * sr), (int) (2.0 * sr), 220.0 * std::pow (2.0, 6.0 / 12.0));
        logMessage ("MEASURE|fx2|resonator A3 peak|" + juce::String (bestHz, 1) + " Hz (" + juce::String (cents, 1) + " ct), " + juce::String (best - offPeak, 1) + " dB above a tritone|");
        expect (std::abs (cents) < 10.0, "peak at the note");
        expect (best - offPeak > 15.0, "resonant");
    }

    void transient()
    {
        beginTest ("Transient Shaper: attack boost / cut and sustain cut on struck tones");
        juce::AudioBuffer<float> in (2, (int) sr);
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            const int k = i % 12000; // four hits per second: instant attack, 80 ms decay
            const float v = 0.4f * std::exp (-(float) k / (0.08f * (float) sr)) * std::sin (2.0f * juce::MathConstants<float>::pi * 330.0f * (float) i / (float) sr);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        auto measure = [&] (float attack, float sustain)
        {
            auto p = params();
            set (p, pid::FxTransient, pid::transient::Attack, attack);
            set (p, pid::FxTransient, pid::transient::Sustain, sustain);
            const auto out = run (pid::FxTransient, in, p);
            // hit 3: onset 0-5 ms and tail 60-200 ms
            const int h = 24000;
            return std::pair<double, double> { rmsDb (out, 0, h, h + 240), rmsDb (out, 0, h + 2880, h + 9600) };
        };
        const auto flat = measure (0.0f, 0.0f), up = measure (1.0f, 0.0f), down = measure (-1.0f, 0.0f), dry = measure (0.0f, -1.0f);
        logMessage ("MEASURE|fx2|transient attack +1 / -1: onset change|" + juce::String (up.first - flat.first, 1) + " / " + juce::String (down.first - flat.first, 1) + "|dB");
        logMessage ("MEASURE|fx2|transient sustain -1: tail change|" + juce::String (dry.second - flat.second, 1) + "|dB");
        expectWithinAbsoluteError (flat.first, rmsDb (in, 0, 24000, 24240), 0.01, "neutral settings are transparent");
        expect (up.first - flat.first > 4.0, "attack boost");
        expect (down.first - flat.first < -4.0, "attack cut");
        expect (dry.second - flat.second < -4.0, "sustain cut");
    }

    void spectral()
    {
        beginTest ("Spectral: transparent at Blur 0 (2048-sample latency); Freeze holds the sound after the input stops");
        {
            auto p = params();
            set (p, pid::FxSpectral, pid::spectral::Blur, 0.0f);
            const auto in = noise ((int) sr);
            const auto out = run (pid::FxSpectral, in, p);
            double e = 0, r = 0;
            for (int i = 8192; i < in.getNumSamples(); ++i) { const double d = out.getSample (0, i) - in.getSample (0, i - 2048); e += d * d; r += (double) in.getSample (0, i - 2048) * in.getSample (0, i - 2048); }
            const double res = 10.0 * std::log10 (e / r);
            logMessage ("MEASURE|fx2|spectral identity residual (Blur 0, delay 2048)|" + juce::String (res, 1) + "|dB");
            expect (res < -60.0, "identity: " + juce::String (res, 1));
        }
        {
            auto p = params();
            set (p, pid::FxSpectral, pid::spectral::Blur, 0.0f);
            auto fx = createEffect (pid::FxSpectral);
            fx->prepare (sr, 256);
            juce::AudioBuffer<float> in (2, (int) sr);
            for (int i = 0; i < in.getNumSamples(); ++i)
            {
                const float v = 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sr);
                in.setSample (0, i, v); in.setSample (1, i, v);
            }
            run (pid::FxSpectral, in, p, fx.get());              // tone for 1 s
            set (p, pid::FxSpectral, pid::spectral::Freeze, 1.0f);
            juce::AudioBuffer<float> silence (2, (int) sr);
            silence.clear();
            const auto frozen = run (pid::FxSpectral, silence, p, fx.get()); // input stops while frozen
            const double held = tone (frozen, 0, (int) (0.3 * sr), (int) sr, 440.0);
            set (p, pid::FxSpectral, pid::spectral::Freeze, 0.0f);
            const auto after = run (pid::FxSpectral, silence, p, fx.get());
            const double gone = rmsDb (after, 0, 8192, after.getNumSamples());
            logMessage ("MEASURE|fx2|spectral freeze: 440 Hz held after the input stopped / unfrozen tail|" + juce::String (held, 1) + " / " + juce::String (gone, 1) + "|dB");
            expect (held > -30.0, "frozen spectrum sustains");
            expect (gone < -100.0, "unfreezing with silent input decays to silence");
        }
    }

    void stability()
    {
        beginTest ("All new effects: finite and bounded at extreme settings");
        for (int t = pid::FxBbdChorus; t < pid::NumFx; ++t)
        {
            auto p = params();
            const auto& reg = ParamRegistry::get();
            for (int k = pid::fx::CommonCount; k < pid::kFxParamCounts[t]; ++k)
            {
                const auto& d = reg[pid::FX (t, k)];
                p.value[(size_t) pid::FX (t, k)] = d.maxValue; // every control at its maximum
            }
            if (t == pid::FxSpectral) p.value[(size_t) pid::FX (t, pid::spectral::Freeze)] = 0.0f;
            const auto out = run (t, noise ((int) (sr * 3)), p);
            bool finite = true;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (c, i));
            const float peak = out.getMagnitude (0, out.getNumSamples());
            logMessage ("MEASURE|fx2|" + choices::fxTypeNames[t] + " peak at maximum settings (noise -10.5 dBFS peak)|" + juce::String (juce::Decibels::gainToDecibels (peak), 1) + "|dBFS");
            expect (finite && peak < 8.0f, choices::fxTypeNames[t] + " peak " + juce::String (peak));
        }
    }

    void rackOrder()
    {
        beginTest ("Rack order: older presets (18 effects) keep their order; new effects go to their default places");
        std::vector<int> old;
        for (int t = 0; t <= pid::FxLimiter; ++t) old.push_back (t);
        std::swap (old[0], old[5]); // a custom order: frequency shifter first
        const auto o = pid::completeFxOrder (old);
        auto pos = [&o] (int t) { return (int) (std::find (o.begin(), o.end(), t) - o.begin()); };
        std::array<bool, pid::NumFx> seen {};
        for (int t : o) seen[(size_t) t] = true;
        expect (std::all_of (seen.begin(), seen.end(), [] (bool b) { return b; }), "every effect present once");
        for (size_t i = 1; i < old.size(); ++i) expect (pos (old[i - 1]) < pos (old[i]), "old relative order kept");
        expect (pos (pid::FxLimiter) == pid::NumFx - 1, "limiter still last");
        expect (pos (pid::FxShimmer) > pos (pid::FxReverb) && pos (pid::FxBbdDelay) > pos (pid::FxDigitalDelay), "new effects beside their kind");
        const auto def = pid::completeFxOrder ({});
        expect (def == pid::defaultFxOrder(), "empty -> default order");
    }
};

static ExtraEffectTests extraEffectTests;
} // namespace vsx
