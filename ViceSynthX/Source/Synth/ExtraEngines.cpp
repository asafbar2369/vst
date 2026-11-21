#include "ExtraEngines.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace vsx
{
namespace
{
    constexpr double kPiDbl = 3.14159265358979323846;

    // phase delay (samples) of the first-order allpass (a + z^-1) / (1 + a z^-1) at w (rad / sample)
    double allpassDelay (double a, double w) noexcept
    {
        const std::complex<double> z1 = std::polar (1.0, -w);
        const auto h = (a + z1) / (1.0 + a * z1);
        return -std::arg (h) / w;
    }

    // one-pole lowpass (1 - b) / (1 - b z^-1): phase delay and magnitude at w
    double lowpassDelay (double b, double w) noexcept { return std::atan2 (b * std::sin (w), 1.0 - b * std::cos (w)) / w; }
    double lowpassMag (double b, double w) noexcept { return (1.0 - b) / std::sqrt (1.0 - 2.0 * b * std::cos (w) + b * b); }

    struct SineTab
    {
        static constexpr int N = 4096;
        std::array<float, N + 1> t {};
        SineTab() { for (int i = 0; i <= N; ++i) t[(size_t) i] = (float) std::sin (2.0 * kPiDbl * i / N); }
        float operator() (uint32_t phase) const noexcept
        {
            const uint32_t i = phase >> 20;                                   // 12 bits of index
            const float f = (float) (phase & 0xFFFFFu) * (1.0f / 1048576.0f); // 20 bits of fraction
            return t[i] + (t[i + 1] - t[i]) * f;
        }
    };
    const SineTab& sineTab() { static const SineTab s; return s; }

    // ---- modal materials (mode-frequency ratios, first mode = 1) --------------------------------------------
    // Fletcher & Rossing, "The Physics of Musical Instruments" (2nd ed.): free-free bar (ch. 2), clamped-free bar /
    // tine (ch. 2), circular membrane Bessel zeros (ch. 3), tuned bars of marimba / vibraphone 1 : 4 : 10 (ch. 19),
    // church bell partials hum : prime : tierce : quint : nominal (ch. 21); thin ring / glass bending modes
    // n (n^2 - 1) / sqrt (n^2 + 1) normalised to n = 2 (Rayleigh); stiff string k sqrt (1 + B k^2), B = 4e-4.
    constexpr float kString[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 };
    constexpr float kFreeBar[] = { 1.0f, 2.756f, 5.404f, 8.933f, 13.344f, 18.638f, 24.814f, 31.873f };
    constexpr float kTunedBar[] = { 1.0f, 3.99f, 9.9f };
    constexpr float kGlass[] = { 1.0f, 2.828f, 5.423f, 8.771f, 12.87f, 17.72f };
    constexpr float kBell[] = { 0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f };
    constexpr float kMembrane[] = { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.600f, 3.652f, 4.060f, 4.154f };
    constexpr float kTine[] = { 1.0f, 6.267f, 17.547f, 34.386f };
    struct StiffTable
    {
        std::array<float, 24> r {};
        StiffTable() { const double B = 4.0e-4; for (int k = 1; k <= 24; ++k) r[(size_t) k - 1] = (float) (k * std::sqrt (1.0 + B * k * k) / std::sqrt (1.0 + B)); }
    };
    const StiffTable& stiff() { static const StiffTable s; return s; }
}

// =================================================================================================================
void StringModel::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    int need = (int) std::ceil (sr / kMinFreq) + 64, size = 1;
    while (size < need) size <<= 1;
    line.assign ((size_t) size, 0.0f);
    excitation.assign ((size_t) size, 0.0f);
    mask = size - 1;
    dcR = (float) (1.0 - 2.0 * kPiDbl * 10.0 / sr);
}

void StringModel::retune (float freq) noexcept
{
    const double f = juce::jlimit ((double) kMinFreq, sr * 0.45, (double) freq);
    curFreq = (float) f;
    const double P = sr / f, w = 2.0 * kPiDbl * f / sr;

    // loss filter: Damping moves the one-pole lowpass from flat to strongly frequency-dependent
    double b = juce::jlimit (0.0, 0.95, (double) params.damp * 0.9);
    // stiffness: four first-order allpasses with a <= 0 delay low partials more than high ones -> stretched partials
    double a = -0.6 * juce::jlimit (0.0, 1.0, (double) params.inharm);
    double dDisp = kDisp * allpassDelay (a, w);
    while (dDisp > 0.4 * P && a < -1.0e-4) { a *= 0.7; dDisp = kDisp * allpassDelay (a, w); } // must fit the period
    if (a > -1.0e-4) { a = 0.0; dDisp = 0.0; } // render() skips the chain entirely when the coefficient is 0
    dispCoef = (float) a;
    lpCoef = (float) b;

    // total loop delay must be P samples at the fundamental: integer delay + Thiran fractional part
    const double rem = P - dDisp - lowpassDelay (b, w);
    dInt = std::max (1, (int) std::floor (rem - 0.5));
    double d = rem - dInt; // target phase delay of the Thiran stage
    double frac = d;
    for (int it = 0; it < 4; ++it) // refine: Thiran phase delay at w differs slightly from its DC value
    {
        const double at = (1.0 - frac) / (1.0 + frac);
        frac += d - allpassDelay (at, w);
        frac = juce::jlimit (0.05, 3.0, frac);
    }
    apCoef = (float) ((1.0 - frac) / (1.0 + frac));

    // loop gain: the fundamental decays by 60 dB in Decay seconds; never above 1 (the loss filter is 1 at DC)
    const double perPeriod = std::pow (10.0, -3.0 / (std::max (0.05, (double) params.decay) * f));
    loopGain = (float) std::min (0.99995, perPeriod / lowpassMag (b, w));
}

void StringModel::noteOn (float freq, int velocity, const PhysParams& p, uint32_t seed) noexcept
{
    params = p;
    rng.seed (seed);
    std::fill (line.begin(), line.end(), 0.0f);
    apState = apIn = lpState = dcX = dcY = bowLp = 0.0f;
    dispX.fill (0.0f); dispY.fill (0.0f);
    writePos = 0;
    retune (freq);

    const float vg = std::pow (juce::jlimit (1, 127, velocity) / 127.0f, 1.2f);
    const int L = juce::jlimit (2, (int) excitation.size() - 1, (int) std::lround (sr / curFreq));
    excLen = 0;
    excPos = 0;
    bowLevel = 0.0f;
    bowBright = 0.02f + 0.6f * p.bright * p.bright;
    if (p.excite == 2) { bowLevel = vg; return; }

    // excitation shape (one period), then the pick-position comb: plucking at x/L cancels every (L/x)-th partial
    if (p.excite == 0)
    {
        const float alpha = 0.03f + 0.97f * p.bright * p.bright; // brighter pluck = less smoothing of the noise
        float lp = 0.0f, mean = 0.0f;
        for (int k = 0; k < L; ++k) { lp += (rng.bi() - lp) * alpha; excitation[(size_t) k] = lp; mean += lp; }
        mean /= (float) L;
        for (int k = 0; k < L; ++k) excitation[(size_t) k] -= mean;
    }
    else
    {
        const int h = std::max (2, (int) std::lround ((1.0f - p.bright) * 0.3f * L + 2.0f)); // hammer contact time
        for (int k = 0; k < L; ++k)
            excitation[(size_t) k] = k < h ? 0.5f * (1.0f - std::cos (2.0f * kPi * (float) k / (float) h)) : 0.0f;
    }
    const float pos = juce::jlimit (0.02f, 0.98f, p.position);
    const int pick = juce::jlimit (1, L - 1, (int) std::lround (pos * (float) L));
    for (int k = L - 1; k >= pick; --k) excitation[(size_t) k] -= excitation[(size_t) (k - pick)];
    // normalise the excitation energy (not its peak): the level then no longer depends on the excitation shape
    double e2 = 0.0;
    for (int k = 0; k < L; ++k) e2 += (double) excitation[(size_t) k] * excitation[(size_t) k];
    const float rms = (float) std::sqrt (e2 / L) + 1.0e-9f;
    float peak = 1.0e-9f;
    for (int k = 0; k < L; ++k) peak = std::max (peak, std::abs (excitation[(size_t) k]));
    // ... but a short hammer pulse in a long period would need a huge peak for that energy: cap the peak
    const float scale = vg * std::min (0.5f / rms, 1.2f / peak);
    for (int k = 0; k < L; ++k) excitation[(size_t) k] *= scale;
    excLen = L;
    outGain = 3.2f; // loudest-400 ms level matched to the analog saw on INIT (Tests/TestExtraEngines.cpp)
}

void StringModel::setParams (float freq, const PhysParams& p) noexcept
{
    const bool changed = std::abs (freq - curFreq) > curFreq * 1.0e-5f || p.damp != params.damp || p.inharm != params.inharm
                         || p.decay != params.decay;
    const int excite = params.excite;
    params = p;
    params.excite = excite; // the excitation type is chosen at note-on
    bowBright = 0.02f + 0.6f * p.bright * p.bright;
    if (changed) retune (freq);
}

float StringModel::render() noexcept
{
    const float x = line[(size_t) ((writePos - dInt) & mask)];
    // first-order Thiran allpass (fractional delay)
    const float yf = apCoef * x + apIn - apCoef * apState;
    apIn = x;
    apState = yf;
    // dispersion chain
    float y = yf;
    if (dispCoef != 0.0f)
        for (int i = 0; i < kDisp; ++i)
        {
            const float o = dispCoef * y + dispX[(size_t) i] - dispCoef * dispY[(size_t) i];
            dispX[(size_t) i] = y;
            dispY[(size_t) i] = o;
            y = o;
        }
    // loss filter and loop gain
    lpState = (1.0f - lpCoef) * y + lpCoef * lpState;
    float v = loopGain * lpState;
    if (excPos < excLen) v += excitation[(size_t) excPos++];
    if (bowLevel > 0.0f)
    {
        // sustained excitation: band-limited noise fed into the loop while the key is held (a "bowed / blown"
        // texture; not a friction model)
        bowLp += (rng.bi() - bowLp) * bowBright;
        // scaled by sqrt (1 - g^2): the sustained level of a lossy loop driven by noise is then independent of Decay
        v += bowLp * bowLevel * 0.6f * std::sqrt (std::max (0.0f, 1.0f - loopGain * loopGain));
    }
    v = flushDenormal (v);
    line[(size_t) writePos] = v;
    writePos = (writePos + 1) & mask;
    // DC blocker on the output
    const float o = v - dcX + dcR * dcY;
    dcX = v;
    dcY = flushDenormal (o);
    return o * outGain;
}

// =================================================================================================================
int ModalModel::ratios (int material, const float*& out) noexcept
{
    switch (material)
    {
        case 0: out = kString; return (int) std::size (kString);
        case 1: out = stiff().r.data(); return (int) stiff().r.size();
        case 2: out = kFreeBar; return (int) std::size (kFreeBar);
        case 3: out = kTunedBar; return (int) std::size (kTunedBar);
        case 4: out = kGlass; return (int) std::size (kGlass);
        case 5: out = kBell; return (int) std::size (kBell);
        case 6: out = kMembrane; return (int) std::size (kMembrane);
        default: out = kTine; return (int) std::size (kTine);
    }
}

void ModalModel::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    (void) stiff();
}

void ModalModel::update (float freq, const PhysParams& p) noexcept
{
    curFreq = freq;
    const float* r = nullptr;
    const int n = std::min (kModes, ratios (p.material, r));
    const float pos = juce::jlimit (0.02f, 0.98f, p.position);
    const float stretch = 1.0f + 0.25f * juce::jlimit (-1.0f, 1.0f, p.inharm);
    active = 0;
    float ampSum = 0.0f;
    for (int k = 0; k < n; ++k)
    {
        const float rk = std::pow (r[k], stretch);
        const double fk = (double) freq * rk;
        if (fk >= 0.45 * sr) break; // no mode above 0.45 x sample rate: no aliasing at the source
        const double w = 2.0 * kPiDbl * fk / sr;
        // higher modes decay faster with Damping
        const double t60 = std::max (0.02, (double) p.decay / (1.0 + p.damp * 4.0 * std::max (0.0f, rk - 1.0f)));
        const double rr = std::pow (10.0, -3.0 / (t60 * sr));
        a1[(size_t) k] = (float) (2.0 * rr * std::cos (w));
        a2[(size_t) k] = (float) (rr * rr);
        // amplitude: natural fall-off, Brightness tilt, strike-position weighting; sin (w) normalises the impulse peak
        float amp = 1.0f / (1.0f + 0.35f * (float) k);
        amp *= std::pow (std::max (0.25f, rk), -(1.0f - p.bright) * 1.2f);
        amp *= std::max (0.08f, std::abs (std::sin (kPi * (float) (k + 1) * pos)));
        gain[(size_t) k] = amp * (float) std::sin (w);
        ampSum += amp;
        active = k + 1;
    }
    // modes start in phase at the strike, so the peak grows with the sum of the mode amplitudes: normalise it
    const float norm = 2.0f / std::max (2.0f, ampSum);
    for (int k = 0; k < active; ++k) gain[(size_t) k] *= norm;
}

void ModalModel::noteOn (float freq, int velocity, const PhysParams& p, uint32_t seed) noexcept
{
    params = p;
    rng.seed (seed);
    y1.fill (0.0f); y2.fill (0.0f);
    update (freq, p);
    const float vg = std::pow (juce::jlimit (1, 127, velocity) / 127.0f, 1.2f);
    excAmp = vg;
    excPos = 0;
    bowLevel = 0.0f;
    bowLp = 0.0f;
    bowBright = 0.02f + 0.6f * p.bright * p.bright;
    if (p.excite == 0) excLen = 1; // impulse
    else if (p.excite == 1)
    {
        // mallet: raised-cosine contact, 0.25 ms (hard) .. 3.25 ms (soft); harder with velocity
        hammerWidth = (float) std::max (2.0, (0.25 + 3.0 * (1.0 - p.bright) * (1.0 - 0.4 * vg)) * 1.0e-3 * sr);
        excLen = (int) hammerWidth;
    }
    else { excLen = 0; bowLevel = vg; }
    outGain = 0.45f; // level-matched to the analog saw on INIT (Tests/TestExtraEngines.cpp)
}

void ModalModel::setParams (float freq, const PhysParams& p) noexcept
{
    const int excite = params.excite;
    params = p;
    params.excite = excite;
    bowBright = 0.02f + 0.6f * p.bright * p.bright;
    update (freq, params);
}

float ModalModel::render() noexcept
{
    float x = 0.0f;
    if (excPos < excLen)
    {
        if (params.excite == 0) x = excAmp * 4.0f;
        else x = excAmp * 0.5f * (1.0f - std::cos (2.0f * kPi * (float) excPos / hammerWidth)) * (8.0f / hammerWidth);
        ++excPos;
    }
    if (bowLevel > 0.0f)
    {
        bowLp += (rng.bi() - bowLp) * bowBright;
        x += bowLp * bowLevel * 0.05f;
    }
    float out = 0.0f;
    for (int k = 0; k < active; ++k)
    {
        const float y = a1[(size_t) k] * y1[(size_t) k] - a2[(size_t) k] * y2[(size_t) k] + gain[(size_t) k] * x;
        y2[(size_t) k] = y1[(size_t) k];
        y1[(size_t) k] = flushDenormal (y);
        out += y;
    }
    return out * outGain;
}

// =================================================================================================================
float PhaseDistortion::shapeValue (float p, int shape, float amount) noexcept
{
    const float d = juce::jlimit (0.0f, 1.0f, amount);
    auto sawWarp = [] (float q) // full distortion: phase races through the first half in 1 % of the cycle
    {
        constexpr float m = 0.01f;
        return q < m ? 0.5f * q / m : 0.5f + 0.5f * (q - m) / (1.0f - m);
    };
    if (shape >= 5)
    {
        // resonance shapes: a sine at 1 .. 16 x the fundamental under a window that reaches 0 at the cycle end, so the
        // wave is continuous at the wrap (no hard-sync step): -1 + w (p) (1 - cos (2 pi r p)); r = 1, w = 1 is a sine
        const float ratio = 1.0f + 15.0f * d;
        const float w = shape == 5 ? 1.0f - p : shape == 6 ? 1.0f - std::abs (2.0f * p - 1.0f) : std::min (1.0f, 2.0f * (1.0f - p));
        // crossfade from the pure sine (Amount 0) to the windowed resonance (Amount 1): both terms are 0 at p = 0 and
        // p -> 1, so the wave is continuous at the wrap for every Amount
        return -1.0f + (1.0f - d) * (1.0f - std::cos (kTwoPi * p)) + d * w * (1.0f - std::cos (kTwoPi * ratio * p));
    }
    float full;
    switch (shape)
    {
        case 0: full = sawWarp (p); break;                                                   // saw
        case 1: { const float h = p < 0.5f ? 0.0f : 0.5f; const float q = p < 0.5f ? 2.0f * p : 2.0f * p - 1.0f;
                  full = h + 0.5f * std::min (1.0f, q / 0.02f); break; }                    // square
        case 2: full = std::min (1.0f, p / 0.05f); break;                                    // pulse
        case 3: full = p < 0.5f ? 0.5f * sawWarp (2.0f * p) : 0.5f + 0.5f * sawWarp (2.0f * p - 1.0f); break; // double sine
        default: full = p < 0.5f ? sawWarp (2.0f * p) : 1.0f; break;                         // saw-pulse
    }
    // Amount (DCW) moves the phase map from linear (a pure sine) to the full shape
    const float phi = p + d * (full - p);
    return -std::cos (kTwoPi * phi);
}

float PhaseDistortion::render (float inc, int shape, float amount) noexcept
{
    const float y = shapeValue (phase, shape, amount);
    phase += inc;
    phase -= std::floor (phase);
    return y;
}

// =================================================================================================================
void AdditiveBank::prepare (double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    (void) sineTab();
}

void AdditiveBank::computeTargets (float freq) noexcept
{
    freqNow = freq;
    count = juce::jlimit (1, kMaxPartials, prm.partials);
    const float formantShift = std::exp2 (-prm.formant / 12.0f);
    double energy = 0.0;
    for (int k = 1; k <= count; ++k)
    {
        const size_t i = (size_t) k - 1;
        const float ratio = std::pow ((float) k, 1.0f + 0.12f * prm.stretch);
        const double fk = (double) freq * ratio;
        inc[i] = fk < 0.45 * sr ? (uint32_t) (fk / sr * 4294967296.0) : 0u;
        float a = 0.0f;
        if (inc[i] != 0u)
        {
            const float kk = std::max (1.0f, (float) k * formantShift); // the spectral envelope moves with Formant
            a = 1.0f / kk * std::pow (kk, prm.tilt * 1.5f);
            if (k % 2 == 0) a *= juce::jlimit (0.0f, 2.0f, 1.0f + prm.oddEven);
            a *= 1.0f + prm.motion * 0.6f * wobble[i];
        }
        target[i] = a;
        energy += (double) a * a;
        // partial decay: higher partials die away faster (rate grows with the partial number)
        const double rate = (double) prm.decay * 1.5 * (k - 1); // 1/s
        decayMul[i] = (float) std::exp (-rate / sr);
    }
    for (int k = count; k < kMaxPartials; ++k) target[(size_t) k] = 0.0f;
    norm = energy > 0.0 ? (float) (0.8 / std::sqrt (energy)) : 0.0f; // equal RMS whatever the spectrum (level-matched to the saw)
}

void AdditiveBank::noteOn (float freq, const Params& p, uint32_t seed) noexcept
{
    prm = p;
    rng.seed (seed);
    for (int i = 0; i < kMaxPartials; ++i)
    {
        ph[(size_t) i] = rng.next();
        envelope[(size_t) i] = 1.0f;
        wobble[(size_t) i] = rng.bi();
        wobbleRate[(size_t) i] = 0.2f + 1.3f * rng.uni();
    }
    computeTargets (freq);
    amp = target;
}

void AdditiveBank::setParams (float freq, const Params& p) noexcept
{
    prm = p;
    // slow per-partial motion: each partial's random level drifts (random walk, bounded)
    if (p.motion > 0.0f)
        for (int i = 0; i < count; ++i)
            wobble[(size_t) i] = juce::jlimit (-1.0f, 1.0f, wobble[(size_t) i] + 0.05f * wobbleRate[(size_t) i] * rng.bi());
    computeTargets (freq);
}

float AdditiveBank::render() noexcept
{
    const auto& s = sineTab();
    float out = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        auto& a = amp[(size_t) i];
        a += (target[(size_t) i] - a) * 0.002f; // ~10 ms smoothing at 48 kHz
        auto& e = envelope[(size_t) i];
        out += s (ph[(size_t) i]) * a * e;
        ph[(size_t) i] += inc[(size_t) i];
        e = flushDenormal (e * decayMul[(size_t) i]);
    }
    return out * norm;
}
} // namespace vsx
