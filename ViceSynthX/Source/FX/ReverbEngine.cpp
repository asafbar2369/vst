#include "ReverbEngine.h"

namespace vsx
{
namespace
{
    bool isPrime (int n) noexcept
    {
        if (n < 2) return false;
        if (n % 2 == 0) return n == 2;
        for (int d = 3; d * d <= n; d += 2)
            if (n % d == 0) return false;
        return true;
    }

    int nearestPrime (int n) noexcept
    {
        n = std::max (3, n);
        for (int off = 0; off < 1000; ++off)
        {
            if (isPrime (n + off)) return n + off;
            if (n - off > 2 && isPrime (n - off)) return n - off;
        }
        return n;
    }

    // Walsh functions used as orthogonal output / input sign patterns
    inline float walsh (int k, int mask) noexcept
    {
        unsigned v = (unsigned) (k & mask), parity = 0;
        while (v != 0) { parity ^= v & 1u; v >>= 1; }
        return parity != 0 ? -1.0f : 1.0f;
    }

    void hadamard16 (float* a) noexcept
    {
        for (int h = 1; h < 16; h <<= 1)
            for (int i = 0; i < 16; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float x = a[j], y = a[j + h];
                    a[j] = x + y;
                    a[j + h] = x - y;
                }
        for (int i = 0; i < 16; ++i) a[i] *= 0.25f; // orthonormal
    }

    constexpr float kApMs[ReverbEngine::kLines] = { 1.13f, 1.37f, 1.61f, 1.89f, 2.17f, 2.39f, 2.71f, 2.93f,
                                                    3.17f, 3.43f, 3.71f, 3.97f, 4.27f, 4.51f, 4.79f, 5.03f };
    // Dattorro-style input diffusers (ms), right side slightly longer for decorrelation
    constexpr float kInDiffMs[4] = { 4.77f, 3.59f, 12.73f, 9.30f };
    constexpr float kInDiffG[4] = { 0.75f, 0.75f, 0.625f, 0.625f };
    // early reflection pattern (fractions of the ER window), deterministic
    constexpr float kErPos[14] = { 0.00f, 0.07f, 0.12f, 0.19f, 0.26f, 0.31f, 0.38f, 0.47f, 0.53f, 0.62f, 0.71f, 0.79f, 0.88f, 1.00f };
}

const ReverbEngine::ProgramDef& ReverbEngine::def (int program)
{
    //                                min    max    ER   erMin erMax loopAP  inDiff modMs  hf     highCut  gain
    static const ProgramDef defs[NumPrograms] = {
        /* Hall      */ {         31.0f, 113.0f, 0.30f, 9.0f, 74.0f, 0.62f, 1.00f, 0.90f, 0.85f, 10500.0f, 3.144f },
        /* Plate     */ {         11.0f,  47.0f, 0.00f, 0.0f,  0.0f, 0.65f, 1.10f, 0.85f, 1.00f, 13500.0f, 1.860f },
        /* Room      */ {          7.0f,  31.0f, 0.55f, 2.5f, 23.0f, 0.60f, 0.85f, 0.70f, 0.85f,  8500.0f, 1.762f },
        /* Chamber   */ {         15.0f,  59.0f, 0.45f, 4.5f, 41.0f, 0.60f, 0.95f, 0.85f, 0.90f,  9500.0f, 2.234f },
        /* Ambient   */ {         57.0f, 191.0f, 0.12f, 13.0f, 97.0f, 0.60f, 1.10f, 1.40f, 0.75f,  7200.0f, 5.321f },
        /* Nonlinear */ {          0.0f,   0.0f, 0.00f, 0.0f,  0.0f, 0.00f, 1.20f, 0.00f, 1.00f, 11000.0f, 1.948f },
        /* Gated     */ {          9.0f,  41.0f, 0.35f, 2.0f, 18.0f, 0.65f, 1.10f, 0.40f, 0.95f, 11000.0f, 2.231f },
        /* Reverse   */ {          0.0f,   0.0f, 0.00f, 0.0f,  0.0f, 0.00f, 1.20f, 0.00f, 1.00f, 11000.0f, 1.948f },
    };
    return defs[juce::jlimit (0, NumPrograms - 1, program)];
}

void ReverbEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& l : lines) l.prepare ((int) (0.31 * sr) + 64);
    for (auto& a : loopAp) a.prepare ((int) (0.008 * sr) + 16);
    for (auto& d : inDiff) d.prepare ((int) (0.016 * sr) + 16);
    preL.prepare ((int) (0.26 * sr) + 8);
    preR.prepare ((int) (0.26 * sr) + 8);
    erL.prepare ((int) (0.16 * sr) + 8);
    erR.prepare ((int) (0.16 * sr) + 8);
    tapL.prepare ((int) (1.62 * sr) + 8);
    tapR.prepare ((int) (1.62 * sr) + 8);
    for (auto& d : tapDiff) d.prepare ((int) (0.02 * sr) + 8);
    for (int i = 0; i < 8; ++i) inDiffLen[(size_t) i] = nearestPrime ((int) (kInDiffMs[i & 3] * (i < 4 ? 1.0f : 1.071f) * 0.001 * sr));
    for (int k = 0; k < kLines; ++k)
    {
        mod[(size_t) k].state = 0x9E3779B9u * (uint32_t) (k + 1) | 1u;
        mod[(size_t) k].inc = (float) ((0.23 + 0.87 * std::fmod (k * 0.618034, 1.0)) / sr);
        auto& m = mod[(size_t) k];
        m.phase = (float) std::fmod (k * 0.37, 1.0);
        m.from = m.uniform(); // start moving at once (the first segment used to be flat for up to 4 s)
        m.to = m.uniform();
    }
    lenSmooth = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
    lastProgram = -1;
    lastTapSize = lastTapDecay = -1.0f;
    reset();
}

void ReverbEngine::reset()
{
    for (auto& l : lines) l.reset();
    for (auto& a : loopAp) a.reset();
    for (auto& d : inDiff) d.reset();
    for (auto& d : tapDiff) d.reset();
    preL.reset(); preR.reset(); erL.reset(); erR.reset(); tapL.reset(); tapR.reset();
    for (auto& a : absorb) a.reset();
    for (auto& f : lowCutF) f.reset();
    for (auto& f : highCutF) f.reset();
    erTone[0].reset(); erTone[1].reset();
    gateEnv = gateGain = gateHold = 0.0f;
    lastProgram = -1;
}

std::array<float, ReverbEngine::kLines> ReverbEngine::targetLengths (const Settings& s) const
{
    std::array<float, kLines> out {};
    const auto& d = def (s.program);
    const float sizeScale = 0.5f + juce::jlimit (0.0f, 1.0f, s.size);
    for (int k = 0; k < kLines; ++k)
    {
        const float ms = d.minMs * std::pow (d.maxMs / std::max (1.0f, d.minMs), (float) k / (float) (kLines - 1)) * sizeScale;
        out[(size_t) k] = (float) nearestPrime ((int) (ms * 0.001f * (float) sr));
    }
    return out;
}

void ReverbEngine::updateNetwork (const Settings& s, const ProgramDef& d) noexcept
{
    lenTarget = targetLengths (s);
    const float rt = s.program == Gated ? 1.8f : juce::jlimit (0.1f, 30.0f, s.decay);
    const float hfRatio = juce::jlimit (0.08f, 1.0f, (1.0f - 0.85f * std::sqrt (juce::jlimit (0.0f, 1.0f, s.damping))) * d.hfFactor);
    const float crossover = 4200.0f * std::pow (0.3f, juce::jlimit (0.0f, 1.0f, s.damping));
    const float depth = juce::jlimit (0.0f, 1.0f, s.modulation) * d.modMs * 0.001f * (float) sr;
    for (int k = 0; k < kLines; ++k)
    {
        apLen[(size_t) k] = (float) nearestPrime ((int) (kApMs[k] * (0.55f + 0.45f * (0.5f + s.size)) * 0.001f * (float) sr));
        const float loop = lenTarget[(size_t) k] + apLen[(size_t) k];
        gLow[(size_t) k] = std::pow (10.0f, -3.0f * loop / (rt * (float) sr));
        gHigh[(size_t) k] = std::pow (10.0f, -3.0f * loop / (rt * hfRatio * (float) sr));
        absorb[(size_t) k].setCutoff (crossover, sr);
        modDepth[(size_t) k] = depth * (0.6f + 0.4f * (float) std::fmod (k * 0.618034, 1.0));
    }
    const float sizeScale = 0.5f + s.size;
    for (int i = 0; i < 14; ++i)
    {
        erTime[(size_t) i] = (d.erMinMs + (d.erMaxMs - d.erMinMs) * kErPos[i]) * sizeScale * 0.001f * (float) sr;
        erGain[(size_t) i] = (1.0f - 0.7f * kErPos[i]) * ((i % 3 == 1) ? -1.0f : 1.0f) * 0.36f;
    }
}

void ReverbEngine::updateTaps (const Settings& s) noexcept
{
    const bool rev = s.program == Reverse;
    const float base = rev ? juce::jlimit (0.15f, 1.5f, 0.15f + s.decay * 0.15f) : juce::jlimit (0.06f, 0.9f, 0.06f + s.decay * 0.1f);
    const float T = base * (0.7f + 0.6f * juce::jlimit (0.0f, 1.0f, s.size)) * (float) sr;
    uint32_t st = 0x1234567u;
    auto rnd = [&st] { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return (float) (st & 0xffffff) / 16777216.0f; };
    float energy = 0.0f;
    for (int i = 0; i < kTaps; ++i)
    {
        const float u = ((float) i + 0.5f + 0.9f * (rnd() - 0.5f)) / (float) kTaps;
        tapTimeL[(size_t) i] = std::max (1.0f, T * u);
        tapTimeR[(size_t) i] = std::max (1.0f, T * ((float) i + 0.5f + 0.9f * (rnd() - 0.5f)) / (float) kTaps);
        const float env = rev ? std::pow (u, 1.6f) : 1.0f - 0.35f * u;
        tapGain[(size_t) i] = env * (rnd() < 0.5f ? -1.0f : 1.0f);
        energy += tapGain[(size_t) i] * tapGain[(size_t) i];
    }
    const float norm = 1.0f / std::sqrt (std::max (1.0e-6f, energy));
    for (auto& g : tapGain) g *= norm;
}

float ReverbEngine::processFdn (float inL, float inR, float& outL, float& outR, const ProgramDef& d) noexcept
{
    float y[kLines];
    const float apG = d.loopDiffusion;
    for (int k = 0; k < kLines; ++k)
    {
        auto& lc = lenCur[(size_t) k];
        lc += (lenTarget[(size_t) k] - lc) * lenSmooth;
        float v = lines[(size_t) k].read (lc + modDepth[(size_t) k] * mod[(size_t) k].next());
        v = allpass (loopAp[(size_t) k], v, (int) apLen[(size_t) k], apG);
        const float lp = absorb[(size_t) k].process (v);
        y[k] = gHigh[(size_t) k] * v + (gLow[(size_t) k] - gHigh[(size_t) k]) * lp;
    }
    float m[kLines];
    std::copy (y, y + kLines, m);
    hadamard16 (m);
    float l = 0.0f, r = 0.0f;
    for (int k = 0; k < kLines; ++k)
    {
        const float in = (k & 1) ? inR : inL;
        lines[(size_t) k].push (flushDenormal (m[k] + in * walsh (k, 6)));
        l += y[k] * walsh (k, 5);
        r += y[k] * walsh (k, 10);
    }
    outL = l * 0.25f;
    outR = r * 0.25f;
    return 0.0f;
}

void ReverbEngine::process (float* l, float* r, int n, const Settings& s) noexcept
{
    const auto& d = def (s.program);
    const bool tapped = s.program == Nonlinear || s.program == Reverse;
    if (s.program != lastProgram)
    {
        // a new program starts from silence (no pitch sweep of the old network)
        for (auto& line : lines) line.reset();
        for (auto& a : loopAp) a.reset();
        tapL.reset(); tapR.reset();
        lastTapSize = -1.0f;
        lastProgram = s.program;
        lenTarget = targetLengths (s);
        lenCur = lenTarget;
    }
    if (tapped)
    {
        if (std::abs (s.size - lastTapSize) > 0.01f || std::abs (s.decay - lastTapDecay) > 0.01f * std::max (0.1f, lastTapDecay))
        {
            updateTaps (s);
            lastTapSize = s.size;
            lastTapDecay = s.decay;
        }
    }
    else
        updateNetwork (s, d);

    const float diffScale = d.inputDiffusion * (0.3f + 0.7f * juce::jlimit (0.0f, 1.0f, s.diffusion));
    const float preSamples = std::max (1.0f, s.predelayMs * 0.001f * (float) sr);
    lowCutF[0].set (Biquad::HighPass, sr, juce::jlimit (20.0f, 1000.0f, s.lowCut), 0.707f);
    lowCutF[1] = Biquad { lowCutF[0].b0, lowCutF[0].b1, lowCutF[0].b2, lowCutF[0].a1, lowCutF[0].a2, lowCutF[1].z1, lowCutF[1].z2 };
    const float hc = std::min ((float) sr * 0.45f, d.highCut * std::pow (0.55f, juce::jlimit (0.0f, 1.0f, s.damping)));
    {
        const float z1 = highCutF[0].z1, z2 = highCutF[0].z2;
        highCutF[0].set (Biquad::LowPass, sr, hc, 0.6f);
        highCutF[0].z1 = z1; highCutF[0].z2 = z2;
        highCutF[1] = Biquad { highCutF[0].b0, highCutF[0].b1, highCutF[0].b2, highCutF[0].a1, highCutF[0].a2, highCutF[1].z1, highCutF[1].z2 };
    }
    erTone[0].setCutoff (7000.0f, sr);
    erTone[1].setCutoff (7000.0f, sr);
    const float holdSamples = juce::jlimit (0.06f, 1.2f, 0.06f + s.decay * 0.08f) * (float) sr;
    const float gateAtt = 1.0f - std::exp (-1.0f / (0.001f * (float) sr)), gateRel = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
    const float envRel = std::exp (-1.0f / (0.03f * (float) sr));
    const float width = juce::jlimit (0.0f, 1.0f, s.width);

    for (int i = 0; i < n; ++i)
    {
        const float dryL = l[i], dryR = r[i];
        preL.push (dryL);
        preR.push (dryR);
        float xl = preL.read (preSamples), xr = preR.read (preSamples);
        float er[2] = { 0.0f, 0.0f };
        if (d.erLevel > 0.0f && ! tapped)
        {
            erL.push (xl);
            erR.push (xr);
            for (int t = 0; t < 14; ++t)
            {
                const int tl = std::max (1, (int) erTime[(size_t) t]);
                const float g = erGain[(size_t) t];
                if (t & 1) { er[1] += g * erR.readInt (tl); er[0] += 0.3f * g * erL.readInt (tl); }
                else { er[0] += g * erL.readInt (tl); er[1] += 0.3f * g * erR.readInt (tl); }
            }
            er[0] = erTone[0].process (er[0]);
            er[1] = erTone[1].process (er[1]);
        }
        // input diffusion
        for (int k = 0; k < 4; ++k)
        {
            xl = allpass (inDiff[(size_t) k], xl, inDiffLen[(size_t) k], kInDiffG[k] * diffScale);
            xr = allpass (inDiff[(size_t) k + 4], xr, inDiffLen[(size_t) k + 4], kInDiffG[k] * diffScale);
        }
        float outL = 0.0f, outR = 0.0f;
        if (tapped)
        {
            tapL.push (xl);
            tapR.push (xr);
            for (int t = 0; t < kTaps; ++t)
            {
                outL += tapGain[(size_t) t] * tapL.readInt ((int) tapTimeL[(size_t) t]);
                outR += tapGain[(size_t) t] * tapR.readInt ((int) tapTimeR[(size_t) t]);
            }
            const int a = (int) (0.0073 * sr), b = (int) (0.0119 * sr);
            outL = allpass (tapDiff[1], allpass (tapDiff[0], outL, a, 0.6f), b, 0.55f);
            outR = allpass (tapDiff[3], allpass (tapDiff[2], outR, b, 0.6f), a, 0.55f);
        }
        else
        {
            processFdn ((xl + 0.4f * er[0]) * 0.5f, (xr + 0.4f * er[1]) * 0.5f, outL, outR, d);
            outL += d.erLevel * er[0];
            outR += d.erLevel * er[1];
            if (s.program == Gated)
            {
                const float a = std::max (std::abs (dryL), std::abs (dryR));
                gateEnv = a > gateEnv ? a : a + (gateEnv - a) * envRel;
                if (gateEnv > 0.0158f) gateHold = holdSamples;
                else gateHold = std::max (0.0f, gateHold - 1.0f);
                const float target = gateHold > 0.0f ? 1.0f : 0.0f;
                gateGain += (target - gateGain) * (target > gateGain ? gateAtt : gateRel);
                outL *= gateGain;
                outR *= gateGain;
            }
        }
        outL = highCutF[0].process (lowCutF[0].process (outL)) * d.gain;
        outR = highCutF[1].process (lowCutF[1].process (outR)) * d.gain;
        const float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * width;
        l[i] = mid + side;
        r[i] = mid - side;
    }
}

} // namespace vsx
