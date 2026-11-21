#include "Effects.h"
#include "ReverbEngine.h"

namespace vsx
{
using namespace pid;

// =================================================================================
void Biquad::set (Type t, double sampleRate, float freq, float q, float gainDb)
{
    freq = juce::jlimit (5.0f, (float) (sampleRate * 0.49), freq);
    q = std::max (0.05f, q);
    const double w = 2.0 * kPiD * freq / sampleRate;
    const double cw = std::cos (w), sw = std::sin (w);
    const double alpha = sw / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);
    double nb0 = 1, nb1 = 0, nb2 = 0, na0 = 1, na1 = 0, na2 = 0;
    switch (t)
    {
        case LowPass: nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = nb0; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
        case HighPass: nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = nb0; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
        case BandPass: nb0 = alpha; nb1 = 0; nb2 = -alpha; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
        case Peak: nb0 = 1 + alpha * A; nb1 = -2 * cw; nb2 = 1 - alpha * A; na0 = 1 + alpha / A; na1 = -2 * cw; na2 = 1 - alpha / A; break;
        case AllPass: nb0 = 1 - alpha; nb1 = -2 * cw; nb2 = 1 + alpha; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
        case LowShelf:
        {
            const double s = 2 * std::sqrt (A) * alpha;
            nb0 = A * ((A + 1) - (A - 1) * cw + s); nb1 = 2 * A * ((A - 1) - (A + 1) * cw); nb2 = A * ((A + 1) - (A - 1) * cw - s);
            na0 = (A + 1) + (A - 1) * cw + s; na1 = -2 * ((A - 1) + (A + 1) * cw); na2 = (A + 1) + (A - 1) * cw - s;
            break;
        }
        case HighShelf:
        {
            const double s = 2 * std::sqrt (A) * alpha;
            nb0 = A * ((A + 1) + (A - 1) * cw + s); nb1 = -2 * A * ((A - 1) + (A + 1) * cw); nb2 = A * ((A + 1) + (A - 1) * cw - s);
            na0 = (A + 1) - (A - 1) * cw + s; na1 = 2 * ((A - 1) - (A + 1) * cw); na2 = (A + 1) - (A - 1) * cw - s;
            break;
        }
    }
    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
    a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}

void DelayLine::prepare (int maxSamples)
{
    int size = 16;
    while (size < maxSamples + 8) size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    mask = size - 1;
    write = 0;
}

float DelayLine::read (float d) const noexcept
{
    d = juce::jlimit (1.0f, (float) (mask - 4), d);
    const int i = (int) d;
    const float f = d - (float) i;
    const float x0 = buf[(size_t) ((write - i + 1) & mask)];
    const float x1 = buf[(size_t) ((write - i) & mask)];
    const float x2 = buf[(size_t) ((write - i - 1) & mask)];
    const float x3 = buf[(size_t) ((write - i - 2) & mask)];
    const float c1 = 0.5f * (x2 - x0);
    const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
    const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * f + c2) * f + c1) * f + x1;
}

namespace
{
    double syncedMs (const FxContext& c, int div)
    {
        return ParamRegistry::syncDivisionBeats (div) * 60000.0 / std::max (20.0, c.bpm);
    }

    /** Helper for nonlinear effects: 2x oversampled waveshaping. */
    struct Oversampled
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        juce::AudioBuffer<float> tmp;
        void prepare (int maxBlock)
        {
            os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            os->initProcessing ((size_t) maxBlock);
            tmp.setSize (2, maxBlock);
        }
        template <typename Fn>
        void run (float* l, float* r, int n, Fn&& shaper)
        {
            tmp.copyFrom (0, 0, l, n);
            tmp.copyFrom (1, 0, r, n);
            juce::dsp::AudioBlock<float> block (tmp);
            block = block.getSubBlock (0, (size_t) n);
            auto up = os->processSamplesUp (block);
            float* ul = up.getChannelPointer (0);
            float* ur = up.getChannelPointer (1);
            for (size_t i = 0; i < up.getNumSamples(); ++i) { ul[i] = shaper (ul[i], 0); ur[i] = shaper (ur[i], 1); }
            os->processSamplesDown (block);
            std::copy (tmp.getReadPointer (0), tmp.getReadPointer (0) + n, l);
            std::copy (tmp.getReadPointer (1), tmp.getReadPointer (1) + n, r);
        }
    };

    // =============================================================================
    class Distortion : public Effect
    {
    public:
        Distortion() : Effect (FxDistortion) {}
        void prepare (double s, int maxBlock) override { sr = s; os.prepare (maxBlock); reset(); }
        void reset() override { for (auto& t : tone) t.reset(); dc[0].reset(); dc[1].reset(); dc[0].prepare (sr); dc[1].prepare (sr); hold[0] = hold[1] = 0; holdPhase = 0; os.os->reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const int type = geti (c, dist::Type);
            const float drive = dbToGain (get (c, dist::Drive));
            const float outG = dbToGain (get (c, dist::Output));
            const float toneHz = get (c, dist::Tone);
            for (int ch = 0; ch < 2; ++ch) tone[ch].setCutoff (toneHz, sr), tone[ch + 2].setCutoff (toneHz, sr);
            if (type <= 3)
            {
                os.run (l, r, n, [type, drive] (float x, int) noexcept
                {
                    x *= drive;
                    switch (type)
                    {
                        case 0: return fastTanh (x);
                        case 1: return juce::jlimit (-1.0f, 1.0f, x);
                        case 2: return std::sin (x * kPi * 0.5f);
                        default: return 0.5f * (x + std::abs (x)) * 1.5f - 0.25f * x;
                    }
                });
            }
            else if (type == 4)
            {
                const float bits = juce::jlimit (2.0f, 16.0f, 16.0f - get (c, dist::Drive) / 48.0f * 14.0f);
                const float q = std::pow (2.0f, bits - 1.0f);
                for (int i = 0; i < n; ++i) { l[i] = std::round (l[i] * q) / q; r[i] = std::round (r[i] * q) / q; }
            }
            else
            {
                const float rate = juce::jlimit (0.01f, 1.0f, 1.0f - get (c, dist::Drive) / 48.0f * 0.97f);
                for (int i = 0; i < n; ++i)
                {
                    holdPhase += rate;
                    if (holdPhase >= 1.0f) { holdPhase -= 1.0f; hold[0] = l[i]; hold[1] = r[i]; }
                    l[i] = hold[0]; r[i] = hold[1];
                }
            }
            for (int i = 0; i < n; ++i)
            {
                l[i] = dc[0].process (tone[2].process (tone[0].process (l[i]))) * outG;
                r[i] = dc[1].process (tone[3].process (tone[1].process (r[i]))) * outG;
            }
        }
    private:
        Oversampled os;
        OnePoleLP tone[4];
        DcBlocker dc[2];
        float hold[2] {}, holdPhase = 0.0f;
    };

    class TubeSaturation : public Effect
    {
    public:
        TubeSaturation() : Effect (FxTube) {}
        void prepare (double s, int maxBlock) override { sr = s; os.prepare (maxBlock); reset(); }
        void reset() override { for (auto& t : split) t.reset(); dc[0].prepare (sr); dc[1].prepare (sr); dc[0].reset(); dc[1].reset(); os.os->reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float drive = dbToGain (get (c, tube::Drive));
            const float bias = get (c, tube::Bias) * 0.6f;
            const float tone = get (c, tube::Tone);
            const float outG = dbToGain (get (c, tube::Output));
            const float norm = 1.0f / std::max (0.1f, fastTanh (drive * 0.5f) * 2.0f);
            os.run (l, r, n, [drive, bias, norm] (float x, int) noexcept
            {
                // asymmetric triode-like transfer: soft positive, harder negative clipping
                const float v = x * drive + bias;
                const float y = v >= 0.0f ? fastTanh (v) : fastTanh (v * 1.4f) / 1.4f;
                return (y - fastTanh (bias)) * norm;
            });
            split[0].setCutoff (700.0f, sr); split[1].setCutoff (700.0f, sr);
            for (int i = 0; i < n; ++i)
            {
                const float lo = split[0].process (l[i]), lo2 = split[1].process (r[i]);
                l[i] = dc[0].process (lo * (1.0f - 0.5f * tone) + (l[i] - lo) * (1.0f + 0.7f * tone)) * outG;
                r[i] = dc[1].process (lo2 * (1.0f - 0.5f * tone) + (r[i] - lo2) * (1.0f + 0.7f * tone)) * outG;
            }
        }
    private:
        Oversampled os;
        OnePoleLP split[2];
        DcBlocker dc[2];
    };

    class TapeSaturation : public Effect
    {
    public:
        TapeSaturation() : Effect (FxTape) {}
        void prepare (double s, int maxBlock) override { sr = s; os.prepare (maxBlock); reset(); }
        void reset() override { for (auto& b : bump) b.reset(); for (auto& h : hf) h.reset(); mem[0] = mem[1] = 0; os.os->reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float drive = dbToGain (get (c, tape::Drive));
            const float outG = dbToGain (get (c, tape::Output));
            for (auto& b : bump) b.set (Biquad::Peak, sr, 75.0f, 1.1f, get (c, tape::Bump) * 6.0f);
            for (auto& h : hf) h.set (Biquad::LowPass, sr, 20000.0f * std::pow (0.25f, get (c, tape::Tone)), 0.6f);
            float* m = mem;
            os.run (l, r, n, [drive, m] (float x, int ch) noexcept
            {
                // simple hysteresis approximation: the magnetisation lags the input
                const float target = fastTanh ((x * drive) - 0.15f * m[ch]);
                m[ch] += (target - m[ch]) * 0.6f;
                return m[ch] / std::max (0.3f, fastTanh (drive));
            });
            for (int i = 0; i < n; ++i)
            {
                l[i] = hf[0].process (bump[0].process (l[i])) * outG;
                r[i] = hf[1].process (bump[1].process (r[i])) * outG;
            }
        }
    private:
        Oversampled os;
        Biquad bump[2], hf[2];
        float mem[2] {};
    };

    class ParametricEq : public Effect
    {
    public:
        ParametricEq() : Effect (FxEq) {}
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { for (auto& ch : f) for (auto& b : ch) b.reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float lc = get (c, eq::LowCut), hc = get (c, eq::HighCut);
            for (auto& ch : f)
            {
                ch[0].set (Biquad::HighPass, sr, lc, 0.707f);
                ch[1].set (Biquad::LowShelf, sr, get (c, eq::LowFreq), 0.707f, get (c, eq::LowGain));
                ch[2].set (Biquad::Peak, sr, get (c, eq::Mid1Freq), get (c, eq::Mid1Q), get (c, eq::Mid1Gain));
                ch[3].set (Biquad::Peak, sr, get (c, eq::Mid2Freq), get (c, eq::Mid2Q), get (c, eq::Mid2Gain));
                ch[4].set (Biquad::HighShelf, sr, get (c, eq::HighFreq), 0.707f, get (c, eq::HighGain));
                ch[5].set (Biquad::LowPass, sr, hc, 0.707f);
            }
            const bool useLc = lc > 21.0f, useHc = hc < 19900.0f;
            for (int i = 0; i < n; ++i)
            {
                float x = l[i], y = r[i];
                if (useLc) { x = f[0][0].process (x); y = f[1][0].process (y); }
                for (int b = 1; b < 5; ++b) { x = f[0][(size_t) b].process (x); y = f[1][(size_t) b].process (y); }
                if (useHc) { x = f[0][5].process (x); y = f[1][5].process (y); }
                l[i] = x; r[i] = y;
            }
        }
    private:
        std::array<std::array<Biquad, 6>, 2> f;
    };

    class MultibandComp : public Effect
    {
    public:
        MultibandComp() : Effect (FxMbComp) {}
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { for (auto& ch : xo) for (auto& b : ch) b.reset(); env.fill (0.0f); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float lowX = get (c, mbc::LowXover), highX = get (c, mbc::HighXover);
            for (auto& ch : xo)
            {
                ch[0].set (Biquad::LowPass, sr, lowX, 0.7071f); ch[1].set (Biquad::LowPass, sr, lowX, 0.7071f);
                ch[2].set (Biquad::HighPass, sr, lowX, 0.7071f); ch[3].set (Biquad::HighPass, sr, lowX, 0.7071f);
                ch[4].set (Biquad::LowPass, sr, highX, 0.7071f); ch[5].set (Biquad::LowPass, sr, highX, 0.7071f);
                ch[6].set (Biquad::HighPass, sr, highX, 0.7071f); ch[7].set (Biquad::HighPass, sr, highX, 0.7071f);
            }
            const float thr[3] = { get (c, mbc::LowThresh), get (c, mbc::MidThresh), get (c, mbc::HighThresh) };
            const float mk[3] = { dbToGain (get (c, mbc::LowGain)), dbToGain (get (c, mbc::MidGain)), dbToGain (get (c, mbc::HighGain)) };
            const float ratio = get (c, mbc::Ratio);
            const float att = (float) std::exp (-1.0 / (get (c, mbc::Attack) * 0.001 * sr));
            const float rel = (float) std::exp (-1.0 / (get (c, mbc::Release) * 0.001 * sr));
            const float outG = dbToGain (get (c, mbc::Output));
            for (int i = 0; i < n; ++i)
            {
                float band[2][3];
                const float in[2] = { l[i], r[i] };
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& x = xo[(size_t) ch];
                    band[ch][0] = x[1].process (x[0].process (in[ch]));             // LR4 low
                    const float rest = x[3].process (x[2].process (in[ch]));        // LR4 high part
                    band[ch][1] = x[5].process (x[4].process (rest));
                    band[ch][2] = x[7].process (x[6].process (rest));
                }
                float outs[2] = { 0.0f, 0.0f };
                for (int b = 0; b < 3; ++b)
                {
                    const float peak = std::max (std::abs (band[0][b]), std::abs (band[1][b]));
                    float& e = env[(size_t) b];
                    e = peak > e ? peak + (e - peak) * att : peak + (e - peak) * rel;
                    const float lev = gainToDb (e);
                    const float over = lev - thr[b];
                    float grDb = 0.0f;
                    const float knee = 6.0f;
                    if (over > knee * 0.5f) grDb = over * (1.0f - 1.0f / ratio);
                    else if (over > -knee * 0.5f) grDb = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
                    const float g = dbToGain (-grDb) * mk[b];
                    outs[0] += band[0][b] * g;
                    outs[1] += band[1][b] * g;
                }
                l[i] = outs[0] * outG;
                r[i] = outs[1] * outG;
            }
        }
    private:
        std::array<std::array<Biquad, 8>, 2> xo;
        std::array<float, 3> env {};
    };

    /** Hilbert transformer: two 4-stage all-pass chains with ~90 degree phase difference
        (coefficients after Olli Niemitalo's published design). */
    struct Hilbert
    {
        static constexpr float c1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static constexpr float c2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        float x1a[4] {}, x2a[4] {}, y1a[4] {}, y2a[4] {};
        float x1b[4] {}, x2b[4] {}, y1b[4] {}, y2b[4] {};
        float delayed = 0.0f;
        void reset() { *this = Hilbert(); }
        void process (float x, float& i, float& q) noexcept
        {
            float a = x, b = x;
            for (int k = 0; k < 4; ++k)
            {
                const float ya = c1[k] * (a + y2a[k]) - x2a[k];
                x2a[k] = x1a[k]; x1a[k] = a; y2a[k] = y1a[k]; y1a[k] = ya; a = ya;
                const float yb = c2[k] * (b + y2b[k]) - x2b[k];
                x2b[k] = x1b[k]; x1b[k] = b; y2b[k] = y1b[k]; y1b[k] = yb; b = yb;
            }
            i = delayed;
            delayed = a;
            q = b;
        }
    };

    class FrequencyShifter : public Effect
    {
    public:
        FrequencyShifter() : Effect (FxFreqShift) {}
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { h[0].reset(); h[1].reset(); phase = 0.0; fb[0] = fb[1] = 0.0f; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const double shift = get (c, fshift::Shift);
            const float stereo = get (c, fshift::Stereo);
            const float feedback = get (c, fshift::Feedback);
            const double inc = shift / sr;
            for (int i = 0; i < n; ++i)
            {
                phase += inc;
                phase -= std::floor (phase);
                const float cs = std::cos (kTwoPi * (float) phase), sn = std::sin (kTwoPi * (float) phase);
                float I, Q;
                h[0].process (l[i] + fb[0] * feedback, I, Q);
                const float yl = I * cs - Q * sn;
                h[1].process (r[i] + fb[1] * feedback, I, Q);
                // right channel: blend towards the opposite shift direction
                const float up = I * cs - Q * sn, down = I * cs + Q * sn;
                const float yr = up + (down - up) * stereo;
                fb[0] = fastTanh (yl); fb[1] = fastTanh (yr);
                l[i] = yl; r[i] = yr;
            }
        }
    private:
        Hilbert h[2];
        double phase = 0.0;
        float fb[2] {};
    };

    class RingModulator : public Effect
    {
    public:
        RingModulator() : Effect (FxRingMod) {}
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { ph[0] = ph[1] = 0.0; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const double f = get (c, ring::Freq);
            const float spread = get (c, ring::Spread);
            const int shape = geti (c, ring::Shape);
            const double inc[2] = { f / sr, f * (1.0 + spread * 0.03) / sr };
            float* ch[2] = { l, r };
            for (int k = 0; k < 2; ++k)
                for (int i = 0; i < n; ++i)
                {
                    ph[k] += inc[k]; ph[k] -= std::floor (ph[k]);
                    const float p = (float) ph[k];
                    float car = shape == 1 ? (p < 0.5f ? 1.0f : -1.0f) : shape == 2 ? 1.0f - 4.0f * std::abs (p - 0.5f) : std::sin (kTwoPi * p);
                    ch[k][i] *= car;
                }
        }
    private:
        double ph[2] {};
    };

    class AnalogChorus : public Effect
    {
    public:
        AnalogChorus() : Effect (FxChorus) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (0.08 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); lfo = 0.0; fbs[0] = fbs[1] = 0; lp[0].reset(); lp[1].reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float rate = get (c, chorus::Rate), depth = get (c, chorus::Depth);
            const float baseMs = get (c, chorus::Delay), fb = get (c, chorus::Feedback), width = get (c, chorus::Width);
            lp[0].setCutoff (get (c, chorus::Tone), sr); lp[1].setCutoff (get (c, chorus::Tone), sr);
            const float base = baseMs * 0.001f * (float) sr;
            const float range = depth * 4.0f * 0.001f * (float) sr;
            for (int i = 0; i < n; ++i)
            {
                lfo += rate / sr; lfo -= std::floor (lfo);
                auto tri = [] (double p) { const double q = p - std::floor (p); return (float) (1.0 - 4.0 * std::abs (q - 0.5)); };
                const float m1 = tri (lfo), m2 = tri (lfo + 0.25 + 0.25 * width);
                dl[0].push (fastTanh (l[i] + fbs[0] * fb));
                dl[1].push (fastTanh (r[i] + fbs[1] * fb));
                const float wl = dl[0].read (base + range * (0.5f + 0.5f * m1));
                const float wr = dl[1].read (base + range * (0.5f + 0.5f * m2));
                fbs[0] = wl; fbs[1] = wr;
                const float ml = lp[0].process (wl), mr = lp[1].process (wr);
                const float mid = 0.5f * (ml + mr), side = 0.5f * (ml - mr) * width;
                l[i] = mid + side; r[i] = mid - side;
            }
        }
    private:
        DelayLine dl[2];
        double lfo = 0.0;
        float fbs[2] {};
        OnePoleLP lp[2];
    };

    class DimensionChorus : public Effect
    {
    public:
        DimensionChorus() : Effect (FxDimension) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (0.05 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); lfo = 0.0; for (auto& f : lp) f.reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            static const float rates[4] = { 0.25f, 0.25f, 0.5f, 0.5f };
            static const float depths[4] = { 0.6f, 1.0f, 1.4f, 2.0f }; // ms
            const int mode = juce::jlimit (0, 3, geti (c, dimension::Mode));
            const float width = get (c, dimension::Width);
            for (auto& f : lp) f.setCutoff (9000.0f, sr);
            const float base = 0.006f * (float) sr;
            const float range = depths[mode] * 0.001f * (float) sr;
            for (int i = 0; i < n; ++i)
            {
                lfo += rates[mode] / sr; lfo -= std::floor (lfo);
                const float tri = 1.0f - 4.0f * std::abs ((float) lfo - 0.5f);
                const float mono = 0.5f * (l[i] + r[i]);
                dl[0].push (mono); dl[1].push (mono);
                const float a = lp[0].process (dl[0].read (base + range * tri));
                const float b = lp[1].process (dl[1].read (base - range * tri * 0.9f + range));
                l[i] = l[i] + width * (0.7f * a - 0.35f * b);
                r[i] = r[i] + width * (0.7f * b - 0.35f * a);
            }
        }
    private:
        DelayLine dl[2];
        double lfo = 0.0;
        OnePoleLP lp[2];
    };

    class Flanger : public Effect
    {
    public:
        Flanger() : Effect (FxFlanger) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (0.03 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); lfo = 0.0; fbs[0] = fbs[1] = 0; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float rate = get (c, flanger::Rate), depth = get (c, flanger::Depth), base = get (c, flanger::Delay) * 0.001f * (float) sr;
            const float fb = get (c, flanger::Feedback), stereo = get (c, flanger::Stereo);
            float* ch[2] = { l, r };
            for (int i = 0; i < n; ++i)
            {
                lfo += rate / sr; lfo -= std::floor (lfo);
                for (int k = 0; k < 2; ++k)
                {
                    const float m = 0.5f + 0.5f * std::sin (kTwoPi * (float) (lfo + k * stereo * 0.5));
                    const float d = 1.0f + base * (1.0f - depth) + base * depth * 2.0f * m;
                    dl[k].push (ch[k][i] + fastTanh (fbs[k] * fb));
                    const float w = dl[k].read (d);
                    fbs[k] = w;
                    ch[k][i] = 0.5f * (ch[k][i] + w);
                }
            }
        }
    private:
        DelayLine dl[2];
        double lfo = 0.0;
        float fbs[2] {};
    };

    class Phaser : public Effect
    {
    public:
        Phaser() : Effect (FxPhaser) {}
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { for (auto& ch : st) ch.fill (0.0f); lfo = 0.0; fbs[0] = fbs[1] = 0; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float rate = get (c, phaser::Rate), depth = get (c, phaser::Depth), fb = get (c, phaser::Feedback);
            const int stages = juce::jlimit (2, 12, geti (c, phaser::Stages));
            const float centre = get (c, phaser::Center), stereo = get (c, phaser::Stereo);
            float* ch[2] = { l, r };
            for (int i = 0; i < n; ++i)
            {
                lfo += rate / sr; lfo -= std::floor (lfo);
                for (int k = 0; k < 2; ++k)
                {
                    const float m = std::sin (kTwoPi * (float) (lfo + k * stereo * 0.5));
                    const float f = juce::jlimit (30.0f, (float) (sr * 0.45), centre * std::exp2 (depth * 2.5f * m));
                    const float t = std::tan (kPi * f / (float) sr);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    float x = ch[k][i] + fbs[k] * fb;
                    for (int s = 0; s < stages; ++s)
                    {
                        float& z = st[(size_t) k][(size_t) s];
                        const float y = a * x + z;
                        z = x - a * y;
                        x = y;
                    }
                    fbs[k] = fastTanh (x);
                    ch[k][i] = 0.5f * (ch[k][i] + x);
                }
            }
        }
    private:
        std::array<std::array<float, 12>, 2> st {};
        double lfo = 0.0;
        float fbs[2] {};
    };

    class StereoDelay : public Effect
    {
    public:
        StereoDelay() : Effect (FxStereoDelay) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (2.6 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); tL.reset (-1); tR.reset (-1); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const bool sync = c.params->getb (FX (type, sdelay::Sync));
            const double msL = sync ? syncedMs (c, geti (c, sdelay::DivL)) : get (c, sdelay::TimeL);
            const double msR = sync ? syncedMs (c, geti (c, sdelay::DivR)) : get (c, sdelay::TimeR);
            const float dL = (float) juce::jlimit (1.0, 2.5 * sr, msL * 0.001 * sr), dR = (float) juce::jlimit (1.0, 2.5 * sr, msR * 0.001 * sr);
            if (tL.target < 0.0f) { tL.reset (dL); tR.reset (dR); }
            tL.setTime (0.08f, sr); tR.setTime (0.08f, sr);
            tL.setTarget (dL); tR.setTarget (dR);
            const float fb = get (c, sdelay::Feedback);
            const bool ping = c.params->getb (FX (type, sdelay::PingPong));
            for (auto& f : hp) f.set (Biquad::HighPass, sr, get (c, sdelay::LowCut), 0.7f);
            for (auto& f : lp) f.set (Biquad::LowPass, sr, get (c, sdelay::HighCut), 0.7f);
            for (int i = 0; i < n; ++i)
            {
                const float wl = dl[0].read (tL.next()), wr = dl[1].read (tR.next());
                const float fl = lp[0].process (hp[0].process (wl)) * fb, fr = lp[1].process (hp[1].process (wr)) * fb;
                if (ping) { dl[0].push (0.5f * (l[i] + r[i]) + fr); dl[1].push (fl); }
                else { dl[0].push (l[i] + fl); dl[1].push (r[i] + fr); }
                l[i] = wl; r[i] = wr;
            }
        }
    private:
        DelayLine dl[2];
        Biquad hp[2], lp[2];
        Smoother tL, tR;
    };

    class TapeDelay : public Effect
    {
    public:
        TapeDelay() : Effect (FxTapeDelay) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (2.6 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); wow = flutter = 0.0; tm.reset (-1); lp[0].reset(); lp[1].reset(); hp[0].reset(); hp[1].reset(); drift = 0; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const bool sync = c.params->getb (FX (type, tdelay::Sync));
            const double ms = sync ? syncedMs (c, geti (c, tdelay::Div)) : get (c, tdelay::Time);
            const float d = (float) juce::jlimit (1.0, 2.4 * sr, ms * 0.001 * sr);
            if (tm.target < 0.0f) tm.reset (d);
            tm.setTime (0.25f, sr);
            tm.setTarget (d);
            const float fb = get (c, tdelay::Feedback), wowA = get (c, tdelay::Wow), flA = get (c, tdelay::Flutter);
            const float satA = 1.0f + get (c, tdelay::Saturation) * 3.0f;
            lp[0].setCutoff (get (c, tdelay::Tone), sr); lp[1].setCutoff (get (c, tdelay::Tone) * 0.92f, sr);
            hp[0].setCutoff (90.0f, sr); hp[1].setCutoff (90.0f, sr);
            for (int i = 0; i < n; ++i)
            {
                wow += 0.55 / sr; flutter += 6.3 / sr;
                drift += (rng.bi() * 0.5f - drift) * 0.0005f;
                const float mod = wowA * 0.0035f * (float) sr * (std::sin (kTwoPi * (float) wow) + drift)
                                + flA * 0.0004f * (float) sr * std::sin (kTwoPi * (float) flutter);
                const float base = tm.next();
                const float wl = dl[0].read (base + mod), wr = dl[1].read (base * 1.013f + mod);
                const float fl = hp[0].process (lp[0].process (fastTanh (wl * satA) / satA)) * fb;
                const float fr = hp[1].process (lp[1].process (fastTanh (wr * satA) / satA)) * fb;
                dl[0].push (l[i] + fl);
                dl[1].push (r[i] + fr);
                l[i] = wl; r[i] = wr;
            }
        }
    private:
        DelayLine dl[2];
        Smoother tm;
        double wow = 0.0, flutter = 0.0;
        float drift = 0.0f;
        OnePoleLP lp[2];
        OnePoleHP hp[2];
        Rng rng;
    };

    class DigitalDelay : public Effect
    {
    public:
        DigitalDelay() : Effect (FxDigitalDelay) {}
        void prepare (double s, int) override { sr = s; for (auto& d : dl) d.prepare ((int) (3.8 * sr)); reset(); }
        void reset() override { for (auto& d : dl) d.reset(); tL.reset (-1); tR.reset (-1); hold[0] = hold[1] = 0; hp = 0; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const bool sync = c.params->getb (FX (type, ddelay::Sync));
            const double ms = sync ? syncedMs (c, geti (c, ddelay::Div)) : get (c, ddelay::Time);
            const float spread = get (c, ddelay::Spread);
            const float dL = (float) juce::jlimit (1.0, 2.5 * sr, ms * 0.001 * sr);
            const float dR = (float) juce::jlimit (1.0, 3.7 * sr, ms * (1.0 + spread * 0.5) * 0.001 * sr);
            if (tL.target < 0.0f) { tL.reset (dL); tR.reset (dR); }
            tL.setTime (0.05f, sr); tR.setTime (0.05f, sr);
            tL.setTarget (dL); tR.setTarget (dR);
            const float fb = get (c, ddelay::Feedback);
            const float crush = get (c, ddelay::Crush);
            const float q = std::pow (2.0f, 16.0f - crush * 12.0f);
            const float rate = 1.0f - crush * 0.85f;
            for (int i = 0; i < n; ++i)
            {
                float wl = dl[0].read (tL.next()), wr = dl[1].read (tR.next());
                if (crush > 0.001f)
                {
                    hp += rate;
                    if (hp >= 1.0f) { hp -= 1.0f; hold[0] = std::round (wl * q) / q; hold[1] = std::round (wr * q) / q; }
                    wl = hold[0]; wr = hold[1];
                }
                dl[0].push (l[i] + wl * fb);
                dl[1].push (r[i] + wr * fb);
                l[i] = wl; r[i] = wr;
            }
        }
    private:
        DelayLine dl[2];
        Smoother tL, tR;
        float hold[2] {}, hp = 0.0f;
    };

    /** Algorithmic reverb: eight programs on the ReverbEngine (see ReverbEngine.h). */
    class AlgorithmicReverb : public Effect
    {
    public:
        AlgorithmicReverb() : Effect (FxReverb) {}
        void prepare (double s, int) override { sr = s; engine.prepare (s); }
        void reset() override { engine.reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            ReverbEngine::Settings st;
            st.program = geti (c, reverb::Program);
            st.size = get (c, reverb::Size);
            st.decay = get (c, reverb::Decay);
            st.predelayMs = get (c, reverb::Predelay);
            st.damping = get (c, reverb::Damping);
            st.diffusion = get (c, reverb::Diffusion);
            st.modulation = get (c, reverb::Modulation);
            st.width = get (c, reverb::Width);
            st.lowCut = get (c, reverb::LowCut);
            engine.process (l, r, n, st);
        }
    private:
        ReverbEngine engine;
    };
}

// =================================================================================
class ConvolutionReverb : public Effect
{
public:
    ConvolutionReverb() : Effect (FxConvolution) {}

    void prepare (double s, int maxBlock) override
    {
        sr = s;
        conv.prepare ({ sr, (juce::uint32) maxBlock, 2 });
        pre[0].prepare ((int) (0.3 * sr)); pre[1].prepare ((int) (0.3 * sr));
        tmp.setSize (2, maxBlock);
        reset();
        if (pendingState.load() == 0 && ! hasIr)
        {
            pendingBuffer = generateImpulse (0, sr);
            pendingRate = sr;
            pendingState.store (1);
        }
    }

    void reset() override
    {
        conv.reset();
        pre[0].reset(); pre[1].reset();
        for (auto& f : filt) f.reset();
    }

    /** Message thread: queue a new impulse for the audio thread to install. */
    void queueImpulse (juce::AudioBuffer<float>&& ir, double rate)
    {
        if (pendingState.load() != 0) return; // previous one not yet consumed
        pendingBuffer = std::move (ir);
        pendingRate = rate;
        pendingState.store (1);
    }

    void process (float* l, float* r, int n, const FxContext& c) noexcept override
    {
        if (pendingState.load() == 1)
        {
            conv.loadImpulseResponse (std::move (pendingBuffer), pendingRate, juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
            hasIr = true;
            pendingState.store (0);
        }
        const float preSamples = std::max (1.0f, c.params->get (FX (type, conv::Predelay)) * 0.001f * (float) sr);
        for (int i = 0; i < n; ++i)
        {
            pre[0].push (l[i]); pre[1].push (r[i]);
            tmp.setSample (0, i, pre[0].read (preSamples));
            tmp.setSample (1, i, pre[1].read (preSamples));
        }
        juce::dsp::AudioBlock<float> block (tmp);
        block = block.getSubBlock (0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctxr (block);
        conv.process (ctxr);
        for (int ch = 0; ch < 2; ++ch)
        {
            filt[(size_t) ch].set (Biquad::HighPass, sr, get (c, conv::LowCut), 0.7f);
            filt[(size_t) ch + 2].set (Biquad::LowPass, sr, get (c, conv::HighCut), 0.7f);
        }
        for (int i = 0; i < n; ++i)
        {
            l[i] = filt[2].process (filt[0].process (tmp.getSample (0, i))) * 0.7f;
            r[i] = filt[3].process (filt[1].process (tmp.getSample (1, i))) * 0.7f;
        }
    }

    int currentChoice = -1;
    float currentLength = -1.0f;
    juce::String currentUser;

private:
    juce::dsp::Convolution conv;
    DelayLine pre[2];
    std::array<Biquad, 4> filt;
    juce::AudioBuffer<float> tmp, pendingBuffer;
    double pendingRate = 44100.0;
    std::atomic<int> pendingState { 0 };
    bool hasIr = false;
};

juce::AudioBuffer<float> generateImpulse (int choice, double rate)
{
    // Synthetic impulse responses: decorrelated noise with frequency-dependent exponential decay,
    // early reflections, and type-specific colouring. Original content.
    struct Spec { float seconds, decayLow, decayHigh, er, density; };
    static const Spec specs[] = {
        { 3.2f, 2.6f, 1.2f, 0.6f, 1.0f },   // concert hall
        { 2.4f, 2.0f, 1.6f, 0.0f, 1.0f },   // plate
        { 0.9f, 0.6f, 0.3f, 0.8f, 1.0f },   // room
        { 1.6f, 1.3f, 0.7f, 0.5f, 1.0f },   // chamber
        { 2.2f, 1.8f, 1.0f, 0.0f, 0.3f },   // spring
        { 7.0f, 6.0f, 2.5f, 0.4f, 1.0f },   // cathedral
        { 2.0f, 1.6f, 1.0f, 0.0f, 1.0f },   // reverse swell
    };
    const Spec& s = specs[juce::jlimit (0, 6, choice)];
    const int n = (int) (s.seconds * rate);
    juce::AudioBuffer<float> ir (2, n);
    ir.clear();
    Rng rng ((uint32_t) (choice * 7919 + 13));
    for (int ch = 0; ch < 2; ++ch)
    {
        OnePoleLP lp; lp.setCutoff (choice == 1 ? 9000.0f : 6000.0f, rate);
        float* d = ir.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) rate;
            float noise = rng.bi();
            if (s.density < 1.0f && rng.uni() > s.density) noise *= 0.1f;
            const float lo = lp.process (noise);
            const float hi = noise - lo;
            const float envLo = std::exp (-6.9f * t / s.decayLow);
            const float envHi = std::exp (-6.9f * t / s.decayHigh);
            const float onset = std::min (1.0f, t / 0.004f);
            d[i] = (lo * envLo + hi * envHi * 0.6f) * onset;
        }
        // early reflections
        if (s.er > 0.0f)
            for (int k = 0; k < 14; ++k)
            {
                const int pos = (int) ((0.005f + rng.uni() * 0.07f) * rate);
                if (pos < n) d[pos] += s.er * (rng.uni() > 0.5f ? 1.0f : -1.0f) * (1.0f - (float) k / 16.0f);
            }
        if (choice == 4)
        {
            // spring: dispersive all-pass chain gives the characteristic "chirp"
            float z[12] {};
            const float a = 0.6f;
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                for (int k = 0; k < 12; ++k) { const float y = -a * x + z[k]; z[k] = x + a * y; x = y; }
                d[i] = x;
            }
        }
        if (choice == 6)
            std::reverse (d, d + n);
    }
    return ir;
}

void convolutionLoadImpulse (Effect& e, int choice, float lengthFraction, const juce::String& userPath)
{
    auto* cr = dynamic_cast<ConvolutionReverb*> (&e);
    if (cr == nullptr) return;
    if (cr->currentChoice == choice && std::abs (cr->currentLength - lengthFraction) < 1.0e-3f && cr->currentUser == userPath)
        return;

    juce::AudioBuffer<float> ir;
    double rate = 48000.0;
    if (choice == 7 && userPath.isNotEmpty())
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (userPath)));
        if (reader != nullptr)
        {
            const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 12.0));
            ir.setSize (2, len);
            reader->read (&ir, 0, len, 0, true, true);
            if (reader->numChannels == 1) ir.copyFrom (1, 0, ir, 0, 0, len);
            rate = reader->sampleRate;
        }
    }
    if (ir.getNumSamples() == 0)
        ir = generateImpulse (choice == 7 ? 0 : choice, rate);

    const int keep = std::max (64, (int) (ir.getNumSamples() * juce::jlimit (0.05f, 1.0f, lengthFraction)));
    if (keep < ir.getNumSamples())
    {
        juce::AudioBuffer<float> trimmed (2, keep);
        for (int ch = 0; ch < 2; ++ch)
        {
            trimmed.copyFrom (ch, 0, ir, ch, 0, keep);
            const int fade = std::min (keep / 4, (int) (0.05 * rate));
            trimmed.applyGainRamp (ch, keep - fade, fade, 1.0f, 0.0f);
        }
        ir = std::move (trimmed);
    }
    cr->queueImpulse (std::move (ir), rate);
    cr->currentChoice = choice;
    cr->currentLength = lengthFraction;
    cr->currentUser = userPath;
}

namespace
{
    class StereoEnhancer : public Effect
    {
    public:
        StereoEnhancer() : Effect (FxStereo) {}
        void prepare (double s, int) override { sr = s; haas.prepare ((int) (0.04 * sr)); reset(); }
        void reset() override { haas.reset(); for (auto& f : lp) f.reset(); }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float width = get (c, stereo::Width);
            const float haasS = get (c, stereo::Haas) * 0.001f * (float) sr;
            const float bassHz = get (c, stereo::BassMono);
            for (auto& f : lp) f.set (Biquad::LowPass, sr, bassHz, 0.7071f);
            for (int i = 0; i < n; ++i)
            {
                float m = 0.5f * (l[i] + r[i]), sd = 0.5f * (l[i] - r[i]) * width;
                float ol = m + sd, orr = m - sd;
                if (haasS >= 1.0f) { haas.push (orr); orr = haas.read (haasS); }
                if (bassHz > 21.0f)
                {
                    // LR2-style split: low band summed to mono
                    const float lowL = lp[1].process (lp[0].process (ol)), lowR = lp[3].process (lp[2].process (orr));
                    const float lowM = 0.5f * (lowL + lowR);
                    ol = ol - lowL + lowM;
                    orr = orr - lowR + lowM;
                }
                l[i] = ol; r[i] = orr;
            }
        }
    private:
        DelayLine haas;
        std::array<Biquad, 4> lp;
    };

    class Limiter : public Effect
    {
    public:
        Limiter() : Effect (FxLimiter) {}
        void prepare (double s, int) override { sr = s; reset(); }
        void reset() override { gain = 1.0f; }
        void process (float* l, float* r, int n, const FxContext& c) noexcept override
        {
            const float in = dbToGain (get (c, limiter::Input));
            const float ceil = dbToGain (c.params->get (FX (type, limiter::Ceiling)));
            const float rel = (float) std::exp (-1.0 / (c.params->get (FX (type, limiter::Release)) * 0.001 * sr));
            const float att = (float) std::exp (-1.0 / (0.0005 * sr));
            for (int i = 0; i < n; ++i)
            {
                const float xl = l[i] * in, xr = r[i] * in;
                const float peak = std::max (std::abs (xl), std::abs (xr));
                const float target = peak > ceil ? ceil / peak : 1.0f;
                gain = target < gain ? target + (gain - target) * att : target + (gain - target) * rel;
                // gain-reduced signal plus a soft safety clip at the ceiling
                l[i] = fastTanh (xl * gain / ceil) * ceil;
                r[i] = fastTanh (xr * gain / ceil) * ceil;
            }
        }
    private:
        float gain = 1.0f;
    };
}

std::unique_ptr<Effect> createEffect (int t)
{
    switch (t)
    {
        case FxDistortion: return std::make_unique<Distortion>();
        case FxTube: return std::make_unique<TubeSaturation>();
        case FxTape: return std::make_unique<TapeSaturation>();
        case FxEq: return std::make_unique<ParametricEq>();
        case FxMbComp: return std::make_unique<MultibandComp>();
        case FxFreqShift: return std::make_unique<FrequencyShifter>();
        case FxRingMod: return std::make_unique<RingModulator>();
        case FxChorus: return std::make_unique<AnalogChorus>();
        case FxDimension: return std::make_unique<DimensionChorus>();
        case FxFlanger: return std::make_unique<Flanger>();
        case FxPhaser: return std::make_unique<Phaser>();
        case FxStereoDelay: return std::make_unique<StereoDelay>();
        case FxTapeDelay: return std::make_unique<TapeDelay>();
        case FxDigitalDelay: return std::make_unique<DigitalDelay>();
        case FxReverb: return std::make_unique<AlgorithmicReverb>();
        case FxConvolution: return std::make_unique<ConvolutionReverb>();
        case FxStereo: return std::make_unique<StereoEnhancer>();
        case FxLimiter: return std::make_unique<Limiter>();
        default: return createExtraEffect (t);
    }
}

} // namespace vsx
