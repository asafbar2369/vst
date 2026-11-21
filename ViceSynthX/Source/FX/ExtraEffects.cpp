#include "Effects.h"
#include "ReverbEngine.h"

/*
    Effects added 2026-11-21. Original implementations of published techniques (measurements in Tests/TestExtraEffects.cpp):
    - BBD Chorus: bucket-brigade-style dual chorus with modes I / II / I+II (LFO rates 0.513 / 0.863 / 9.75 Hz, the
      values widely documented for a classic 1982 polysynth chorus), inverted modulation left / right, BBD-style input and
      output filtering, compander-style saturation and adjustable BBD hiss. Not a circuit-level emulation.
    - BBD Delay: delay whose bandwidth follows bucket-brigade physics: N stages clocked at f = N / (2 t) pass up to
      f / 2 = N / (4 t), so longer delays are darker (4096 stages at 300 ms: about 3.4 kHz). Saturation, hiss and wow
      in the loop.
    - Shimmer: the algorithmic reverb with a pitch-shifted (+12 / +7 / +5 / +19 / +24 / -12 st) feedback path.
    - Rotary: rotating horn and drum after a Linkwitz-Riley crossover at 800 Hz: Doppler (rotor radius / speed of sound),
      amplitude modulation, two microphones; slow / fast speeds with the horn accelerating faster than the drum.
    - Resonator: tuned feedback-comb resonators (1-3 notes: unison, octave, fifth, major, minor, sus4) with T60 decay and
      brightness.
    - Transient Shaper: differential envelopes (fast vs slow, short vs long release) boost or cut attacks and sustain.
    - Spectral: STFT (2048, hop 512) blur (magnitude smoothing over time), freeze (held magnitudes, phase-vocoder phase
      advance) and spectral tilt. Latency 2048 samples on the wet signal.
*/
namespace vsx
{
using namespace pid;

namespace
{
    // 2nd-order Butterworth low / high pass (RBJ), used in pairs for 4th-order / Linkwitz-Riley responses
    struct Bw2
    {
        Biquad lp, hp;
    };

    // -------------------------------------------------------------------------------------------------------------
    class BbdChorus : public Effect
    {
    public:
        BbdChorus() : Effect (FxBbdChorus) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (0.03 * sr)); reset(); }
        void reset() override
        {
            for (auto& d : dl) d.reset();
            for (auto& f : pre) f.reset();
            for (auto& f : post) f.reset();
            lfo = 0.0;
        }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            // mode I, II, I+II: LFO rate (Hz), centre delay and swing (ms)
            static const float rate[3] = { 0.513f, 0.863f, 9.75f }, centre[3] = { 3.5f, 3.5f, 3.3f }, swing[3] = { 1.85f, 1.85f, 0.3f };
            const int mode = juce::jlimit (0, 2, geti (c, bbdchorus::Mode));
            const float depth = get (c, bbdchorus::Depth), noiseAmt = get (c, bbdchorus::Noise), width = get (c, bbdchorus::Width);
            const float tone = get (c, bbdchorus::Tone);
            for (int k = 0; k < 2; ++k)
            {
                pre[k].set (Biquad::LowPass, sr, tone, 0.7071f);
                post[k].set (Biquad::LowPass, sr, tone, 0.7071f);
            }
            const float base = centre[mode] * 0.001f * (float) sr, sw = swing[mode] * depth * 0.001f * (float) sr;
            for (int i = 0; i < n; ++i)
            {
                lfo += rate[mode] / sr; lfo -= std::floor (lfo);
                const float tri = (float) (1.0 - 4.0 * std::abs (lfo - 0.5)); // -1..1
                const float in = 0.5f * (l[i] + r[i]);
                // BBD input: anti-alias filter, compander-style soft saturation, hiss
                const float x = pre[0].process (fastTanh (in * 0.9f) / 0.9f) + noiseAmt * 0.003f * rng.bi();
                dl[0].push (x);
                const float a = post[0].process (dl[0].read (base + sw * tri));
                const float b = post[1].process (dl[0].read (base - sw * tri)); // inverted modulation for the second BBD
                const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * (0.4f + 1.6f * width);
                l[i] = mid + side;
                r[i] = mid - side;
                (void) pre[1];
            }
        }
    private:
        DelayLine dl[2];
        Biquad pre[2], post[2];
        double lfo = 0.0;
        Rng rng;
    };

    // -------------------------------------------------------------------------------------------------------------
    class BbdDelay : public Effect
    {
    public:
        BbdDelay() : Effect (FxBbdDelay) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (1.2 * sr)); reset(); }
        void reset() override
        {
            for (auto& d : dl) d.reset();
            for (auto& f : lp) f.reset();
            fb[0] = fb[1] = 0.0f; wow = 0.0; smoothDelay = -1.0f;
        }
        static float bandwidthHz (int stages, float delaySec) { return (float) stages / (4.0f * std::max (0.001f, delaySec)); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            float timeSec = get (c, bbddelay::Time) * 0.001f;
            if (geti (c, bbddelay::Sync) != 0)
                timeSec = (float) (ParamRegistry::syncDivisionBeats (geti (c, bbddelay::Div)) * 60.0 / std::max (20.0, c.bpm));
            timeSec = juce::jlimit (0.005f, 1.0f, timeSec);
            static const int stagesFor[3] = { 1024, 2048, 4096 };
            const int stages = stagesFor[juce::jlimit (0, 2, geti (c, bbddelay::Stages))];
            const float fbAmt = juce::jlimit (0.0f, 0.95f, get (c, bbddelay::Feedback));
            const float modAmt = get (c, bbddelay::Mod), noiseAmt = get (c, bbddelay::Noise);
            // clock-limited bandwidth (a little under the BBD Nyquist), two 2nd-order sections = 4th order
            const float bw = juce::jlimit (300.0f, 0.45f * (float) sr, 0.9f * bandwidthHz (stages, timeSec));
            for (auto& f : lp) f.set (Biquad::LowPass, sr, bw, 0.7071f);
            const float target = timeSec * (float) sr;
            if (smoothDelay < 0.0f) smoothDelay = target;
            for (int i = 0; i < n; ++i)
            {
                smoothDelay += (target - smoothDelay) * 0.0005f;
                wow += 0.6 / sr; wow -= std::floor (wow);
                const float d = smoothDelay * (1.0f + modAmt * 0.004f * std::sin (6.2831853f * (float) wow));
                float out[2];
                const float in[2] = { l[i], r[i] };
                for (int k = 0; k < 2; ++k)
                {
                    const float y = dl[k].read (d);
                    const float filtered = lp[k * 2 + 1].process (lp[k * 2].process (y));
                    out[k] = filtered;
                    dl[k].push (fastTanh (in[k] + fbAmt * filtered) + noiseAmt * 0.002f * rng.bi());
                }
                l[i] = out[0];
                r[i] = out[1];
            }
        }
    private:
        DelayLine dl[2];
        Biquad lp[4];
        float fb[2] {}, smoothDelay = -1.0f;
        double wow = 0.0;
        Rng rng;
    };

    // -------------------------------------------------------------------------------------------------------------
    // two-tap granular pitch shifter (delay-line read heads sweeping at 1 - ratio, Hann crossfade)
    struct PitchShifter
    {
        DelayLine dl;
        double phase = 0.0;
        float win = 2048.0f;
        void prepare (double sr) { dl.prepare ((int) (0.2 * sr)); win = (float) (0.05 * sr); }
        void reset() { dl.reset(); phase = 0.0; }
        float process (float x, float ratio) noexcept
        {
            dl.push (x);
            phase += (1.0 - ratio) / win;
            phase -= std::floor (phase);
            float y = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                double p = phase + 0.5 * k;
                p -= std::floor (p);
                const float g = 0.5f - 0.5f * std::cos (6.2831853f * (float) p); // Hann: the two taps sum to 1
                y += g * dl.read (2.0f + (float) p * win);
            }
            return y;
        }
    };

    class Shimmer : public Effect
    {
    public:
        Shimmer() : Effect (FxShimmer) {}
        void prepare (double s, int maxBlock) override
        {
            sr = s;
            reverb.prepare (s);
            for (auto& p : shift) p.prepare (s);
            fbL.assign ((size_t) std::max (64, maxBlock), 0.0f);
            fbR.assign ((size_t) std::max (64, maxBlock), 0.0f);
            reset();
        }
        void reset() override
        {
            reverb.reset();
            for (auto& p : shift) p.reset();
            std::fill (fbL.begin(), fbL.end(), 0.0f);
            std::fill (fbR.begin(), fbR.end(), 0.0f);
            for (auto& f : hp) f.reset();
        }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            static const float semis[6] = { 12.0f, 7.0f, 5.0f, 19.0f, 24.0f, -12.0f };
            const float ratio = std::exp2 (semis[juce::jlimit (0, 5, geti (c, shimmer::Pitch))] / 12.0f);
            const float amount = juce::jlimit (0.0f, 1.0f, get (c, shimmer::Amount));
            ReverbEngine::Settings st;
            st.program = ReverbEngine::Ambient;
            st.size = get (c, shimmer::Size);
            st.decay = get (c, shimmer::Decay);
            st.damping = get (c, shimmer::Damping);
            st.predelayMs = get (c, shimmer::Predelay);
            // the reverb's wet RMS grows with sqrt (T60): scale the feedback by 1 / sqrt (T60) so the loop gain of the
            // regenerating octave is the same at every decay (0.45 at 8 s), and saturate inside the loop
            const float fbGain = amount * kFeedback * std::sqrt (8.0f / std::max (0.5f, st.decay));
            const float outGain = 0.34f * std::sqrt (6.0f / std::max (0.5f, st.decay));
            for (auto& f : hp) f.set (Biquad::HighPass, sr, 150.0f, 0.7071f);
            n = std::min (n, (int) fbL.size());
            // the pitch-shifted tail of the previous block is fed back into the reverb input (one block of delay)
            for (int i = 0; i < n; ++i) { l[i] += fbL[(size_t) i]; r[i] += fbR[(size_t) i]; }
            reverb.process (l, r, n, st);
            for (int i = 0; i < n; ++i)
            {
                // the reverb's wet output is well below its input level, so the shifted tail needs gain to regenerate;
                // the loop stays below unity (bounded at every setting: Tests/TestExtraEffects.cpp)
                fbL[(size_t) i] = fastTanh (hp[0].process (shift[0].process (l[i], ratio)) * fbGain);
                fbR[(size_t) i] = fastTanh (hp[1].process (shift[1].process (r[i], ratio)) * fbGain);
                // wet level: this program at long decays is hot (measured +9.7 dB wet gain at the defaults); normalise
                // to the level of the algorithmic reverb, scaling with sqrt (T60) (applied after the feedback tap, so the
                // loop is unchanged)
                l[i] *= outGain;
                r[i] *= outGain;
            }
        }
    private:
        static constexpr float kFeedback = 0.45f;
        ReverbEngine reverb;
        PitchShifter shift[2];
        std::vector<float> fbL, fbR;
        Biquad hp[2];
    };

    // -------------------------------------------------------------------------------------------------------------
    class Rotary : public Effect
    {
    public:
        Rotary() : Effect (FxRotary) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (0.01 * sr)); reset(); }
        void reset() override
        {
            for (auto& d : dl) d.reset();
            for (auto& b : xo) b.reset();
            hornAngle = drumAngle = 0.0; hornHz = 0.8; drumHz = 0.67;
        }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            // rotor speeds (Hz): slow / fast, horn and drum; the horn speeds up and slows down faster than the drum
            const bool fast = geti (c, rotary::Speed) == 1;
            const double hornTarget = fast ? 6.8 : 0.8, drumTarget = fast ? 5.7 : 0.67;
            const double accel = std::max (0.05, (double) get (c, rotary::Accel));
            const double hornCoef = 1.0 - std::exp (-1.0 / (0.7 * accel * sr)), drumCoef = 1.0 - std::exp (-1.0 / (3.5 * accel * sr));
            const float balance = get (c, rotary::Balance), drive = get (c, rotary::Drive), width = get (c, rotary::Width);
            const float hornGain = std::min (1.0f, 1.0f + balance), drumGain = std::min (1.0f, 1.0f - balance);
            for (int k = 0; k < 2; ++k) { xo[k].set (Biquad::LowPass, sr, 800.0f, 0.7071f); xo[k + 2].set (Biquad::HighPass, sr, 800.0f, 0.7071f); }
            const float hornDepth = (float) (0.15 / 343.0 * sr), drumDepth = (float) (0.2 / 343.0 * sr); // rotor radius / c
            const float micAngle = 1.5707963f * width;
            for (int i = 0; i < n; ++i)
            {
                hornHz += (hornTarget - hornHz) * hornCoef;
                drumHz += (drumTarget - drumHz) * drumCoef;
                hornAngle += hornHz / sr; hornAngle -= std::floor (hornAngle);
                drumAngle += drumHz / sr; drumAngle -= std::floor (drumAngle);
                float x = 0.5f * (l[i] + r[i]);
                if (drive > 1.0e-3f) x = fastTanh (x * (1.0f + 6.0f * drive)) / (1.0f + 2.0f * drive);
                const float lo = xo[1].process (xo[0].process (x)), hi = xo[3].process (xo[2].process (x)); // Linkwitz-Riley 4
                dl[0].push (hi);
                dl[1].push (lo);
                float out[2];
                for (int m = 0; m < 2; ++m)
                {
                    const float a = 6.2831853f * (float) hornAngle + (m == 0 ? -micAngle : micAngle);
                    const float b = 6.2831853f * (float) drumAngle + (m == 0 ? -micAngle : micAngle);
                    const float horn = dl[0].read (2.0f + hornDepth * (1.0f + std::sin (a))) * (0.65f + 0.35f * std::cos (a));
                    const float drum = dl[1].read (2.0f + drumDepth * (1.0f + std::sin (b))) * (0.8f + 0.2f * std::cos (b));
                    out[m] = horn * hornGain + drum * drumGain;
                }
                l[i] = out[0];
                r[i] = out[1];
            }
        }
        double getHornHz() const noexcept { return hornHz; }
    private:
        DelayLine dl[2];
        Biquad xo[4];
        double hornAngle = 0.0, drumAngle = 0.0, hornHz = 0.8, drumHz = 0.67;
    };

    // -------------------------------------------------------------------------------------------------------------
    class Resonator : public Effect
    {
    public:
        Resonator() : Effect (FxResonator) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (sr / 25.0) + 8); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); for (auto& f : lp) f.reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            static const int chords[6][3] = { { 0, 0, 0 }, { 0, 12, 0 }, { 0, 7, 0 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 5, 7 } };
            static const int counts[6] = { 1, 2, 2, 3, 3, 3 };
            const int chord = juce::jlimit (0, 5, geti (c, resonator::Chord));
            const float note = get (c, resonator::Note), t60 = std::max (0.02f, get (c, resonator::Decay));
            const float spreadCents = get (c, resonator::Spread) * 12.0f;
            const float tone = get (c, resonator::Tone);
            const int count = counts[chord];
            float delay[2][3], g[2][3];
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < count; ++k)
                {
                    const float f = midiToHz (note + (float) chords[chord][k] + (ch == 0 ? -1.0f : 1.0f) * spreadCents * 0.01f);
                    auto& filt = lp[ch * 3 + k];
                    filt.setCutoff (500.0f + 15000.0f * tone * tone, sr);
                    // loop delay = read delay + 1 (read before push) + the tone filter's phase delay at f: tune it out
                    const double w = 2.0 * 3.14159265358979 * f / sr, a = filt.a;
                    const double lpDelay = std::atan2 (a * std::sin (w), 1.0 - a * std::cos (w)) / w;
                    delay[ch][k] = juce::jlimit (1.0f, (float) dl[0].maxDelay() - 4.0f, (float) (sr / f - 1.0 - lpDelay));
                    g[ch][k] = (float) std::pow (10.0, -3.0 / (t60 * f)); // per pass, from T60
                }
            const float norm = 1.0f / (float) count;
            for (int i = 0; i < n; ++i)
            {
                const float in[2] = { l[i], r[i] };
                float out[2] = { 0.0f, 0.0f };
                for (int ch = 0; ch < 2; ++ch)
                    for (int k = 0; k < count; ++k)
                    {
                        auto& d = dl[ch * 3 + k];
                        const float y = lp[ch * 3 + k].process (d.read (delay[ch][k]));
                        const float v = in[ch] * (1.0f - g[ch][k]) * 4.0f + g[ch][k] * y; // feedback comb, input scaled by the loss
                        d.push (flushDenormal (v));
                        out[ch] += v;
                    }
                // x4: the comb's wet level measured -12 dB re dry on broadband input (Tests/TestFxLaws.cpp)
                l[i] = out[0] * norm * 4.0f;
                r[i] = out[1] * norm * 4.0f;
            }
        }
    private:
        DelayLine dl[6];
        OnePoleLP lp[6];
    };

    // -------------------------------------------------------------------------------------------------------------
    class TransientShaper : public Effect
    {
    public:
        TransientShaper() : Effect (FxTransient) {}
        void prepare (double s, int) override
        {
            sr = s;
            auto coef = [this] (double ms) { return (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr))); };
            fastAtt = coef (0.5); slowAtt = coef (25.0); rel = coef (60.0); longRel = coef (600.0);
            reset();
        }
        void reset() override { envFast = envSlow = envShort = envLong = 0.0f; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float attackAmt = get (c, transient::Attack), sustainAmt = get (c, transient::Sustain);
            const float outGain = juce::Decibels::decibelsToGain (get (c, transient::Output));
            auto follow = [] (float& e, float x, float up, float down) { e += (x - e) * (x > e ? up : down); };
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (l[i]), std::abs (r[i]));
                follow (envFast, x, fastAtt, rel);   // fast attack: leads during transients
                follow (envSlow, x, slowAtt, rel);   // slow attack
                follow (envShort, x, fastAtt, rel);  // short release
                follow (envLong, x, fastAtt, longRel); // long release: leads during decays
                constexpr float eps = 1.0e-6f;
                const float attackDb = juce::jlimit (0.0f, 24.0f, 20.0f * std::log10 ((envFast + eps) / (envSlow + eps)));
                const float sustainDb = juce::jlimit (0.0f, 24.0f, 20.0f * std::log10 ((envLong + eps) / (envShort + eps)));
                const float gDb = juce::jlimit (-24.0f, 12.0f, attackAmt * attackDb + sustainAmt * sustainDb); // shaping range +12 / -24 dB
                const float g = juce::Decibels::decibelsToGain (gDb) * outGain;
                l[i] *= g;
                r[i] *= g;
            }
        }
    private:
        float fastAtt = 0, slowAtt = 0, rel = 0, longRel = 0;
        float envFast = 0, envSlow = 0, envShort = 0, envLong = 0;
    };

    // -------------------------------------------------------------------------------------------------------------
    class Spectral : public Effect
    {
    public:
        static constexpr int kOrder = 11, kN = 1 << kOrder, kHop = kN / 4;
        Spectral() : Effect (FxSpectral), fft (kOrder) {}
        void prepare (double s, int) override
        {
            sr = s;
            for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) kN);
            reset();
        }
        void reset() override
        {
            for (auto& ch : chans)
            {
                ch.in.fill (0.0f); ch.out.fill (0.0f); ch.mag.fill (0.0f); ch.frozen.fill (0.0f); ch.phase.fill (0.0f);
                ch.lastPhase.fill (0.0f); ch.advance.fill (0.0f);
                ch.pos = 0;
            }
            hopCount = 0;
            wasFrozen = false;
        }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float blur = juce::jlimit (0.0f, 0.98f, get (c, spectral::Blur));
            const bool freeze = geti (c, spectral::Freeze) != 0;
            const float tilt = get (c, spectral::Tilt);
            float* io[2] = { l, r };
            for (int i = 0; i < n; ++i)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& s = chans[(size_t) ch];
                    s.in[(size_t) s.pos] = io[ch][i];
                    io[ch][i] = s.out[(size_t) s.pos];
                    s.out[(size_t) s.pos] = 0.0f;
                    s.pos = (s.pos + 1) & (kN - 1);
                }
                if (++hopCount >= kHop)
                {
                    hopCount = 0;
                    for (int ch = 0; ch < 2; ++ch) frame (chans[(size_t) ch], blur, freeze, tilt);
                    wasFrozen = freeze;
                }
            }
        }
    private:
        struct Chan
        {
            std::array<float, kN> in {}, out {};
            std::array<float, kN / 2 + 1> mag {}, frozen {}, phase {}, lastPhase {}, advance {};
            int pos = 0;
        };
        void frame (Chan& s, float blur, bool freeze, float tilt) noexcept
        {
            // analysis: the last kN input samples (oldest first), Hann window
            for (int i = 0; i < kN; ++i) work[(size_t) i] = s.in[(size_t) ((s.pos + i) & (kN - 1))] * window[(size_t) i];
            std::fill (work.begin() + kN, work.end(), 0.0f);
            fft.performRealOnlyForwardTransform (work.data());
            auto* bins = reinterpret_cast<std::complex<float>*> (work.data());
            for (int k = 0; k <= kN / 2; ++k)
            {
                const float m = std::abs (bins[k]);
                float ph = std::arg (bins[k]);
                constexpr float twoPi = 6.2831853f;
                const float expected = twoPi * (float) k * (float) kHop / (float) kN;
                if (freeze)
                {
                    if (! wasFrozen) s.frozen[(size_t) k] = s.mag[(size_t) k];
                    // held magnitude; the phase advances by the bin's measured increment per hop (phase vocoder), so a
                    // frozen partial keeps its true frequency instead of moving to the bin centre
                    s.phase[(size_t) k] += s.advance[(size_t) k];
                    s.phase[(size_t) k] -= twoPi * std::floor (s.phase[(size_t) k] / twoPi);
                    ph = s.phase[(size_t) k];
                    s.mag[(size_t) k] = s.frozen[(size_t) k];
                }
                else
                {
                    // instantaneous frequency: expected advance + the wrapped deviation of the measured phase difference
                    float dev = ph - s.lastPhase[(size_t) k] - expected;
                    dev -= twoPi * std::floor ((dev + 3.14159265f) / twoPi);
                    s.advance[(size_t) k] = expected + dev;
                    s.lastPhase[(size_t) k] = ph;
                    s.mag[(size_t) k] = blur * s.mag[(size_t) k] + (1.0f - blur) * m; // temporal smoothing of the magnitude
                    s.phase[(size_t) k] = ph;
                }
                const float f = (float) k * (float) sr / (float) kN;
                const float tiltGain = tilt == 0.0f ? 1.0f : std::pow (std::max (20.0f, f) / 1000.0f, tilt * 0.5f); // +/- 3 dB per octave at full tilt
                bins[k] = std::polar (s.mag[(size_t) k] * tiltGain, ph);
            }
            fft.performRealOnlyInverseTransform (work.data());
            // synthesis: Hann window again; Hann^2 at 75 % overlap sums to 1.5, so scale by 2/3
            for (int i = 0; i < kN; ++i)
                s.out[(size_t) ((s.pos + i) & (kN - 1))] += work[(size_t) i] * window[(size_t) i] * (2.0f / 3.0f);
        }
        juce::dsp::FFT fft;
        std::array<float, kN> window {};
        std::array<float, kN * 2> work {};
        std::array<Chan, 2> chans {};
        int hopCount = 0;
        bool wasFrozen = false;
    };
}

std::unique_ptr<Effect> createExtraEffect (int t)
{
    switch (t)
    {
        case FxBbdChorus: return std::make_unique<BbdChorus>();
        case FxBbdDelay: return std::make_unique<BbdDelay>();
        case FxShimmer: return std::make_unique<Shimmer>();
        case FxRotary: return std::make_unique<Rotary>();
        case FxResonator: return std::make_unique<Resonator>();
        case FxTransient: return std::make_unique<TransientShaper>();
        case FxSpectral: return std::make_unique<Spectral>();
        default: return nullptr;
    }
}
} // namespace vsx
