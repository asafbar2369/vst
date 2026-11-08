#include "JdEffects.h"
#include <cstring>

namespace dc
{
namespace
{
    int blockIndex (const char* code) noexcept
    {
        if (std::strncmp (code, "DS", 2) == 0 || std::strncmp (code, "CHO", 3) == 0) return 0;
        if (std::strncmp (code, "PH", 2) == 0 || std::strncmp (code, "DLY", 3) == 0) return 1;
        if (std::strncmp (code, "SP", 2) == 0 || std::strncmp (code, "REV", 3) == 0) return 2;
        return 3; // EN
    }
    // Spectrum band centres are not printed in the manual: estimated, one per octave.
    constexpr float kSpectrumHz[6] = { 250, 500, 1000, 2000, 4000, 8000 };
    constexpr float kSpectrumQ[5] = { 4.0f, 2.8f, 2.0f, 1.4f, 1.0f }; // Band width 1 (narrow) .. 5 (wide)
} // namespace

void JdEffects::groupAOrder (int seq, int order[4]) noexcept
{
    const char* s = jd800::kGroupASequences[(seq >= 0 && seq < 24) ? seq : 0];
    for (int i = 0; i < 4; ++i) order[i] = blockIndex (s + i * 3);
}

void JdEffects::groupBOrder (int seq, int order[3]) noexcept
{
    const char* s = jd800::kGroupBSequences[(seq >= 0 && seq < 6) ? seq : 0];
    for (int i = 0; i < 3; ++i) order[i] = blockIndex (s + i * 4);
}

void JdEffects::prepare (float sr, int maxBlock)
{
    sampleRate = sr;
    const size_t n = (size_t) std::max (64, maxBlock);
    for (auto* b : { &mono, &l, &r, &dryL, &dryR, &tmpL, &tmpR, &tmpM })
        b->assign (n, 0.0f);
    chorusUnit.prepare (sr);
    dly.prepare (int (sr * 1.3f));
    rev.prepare (sr);
    early.prepare (int (sr * 0.25f));
    reset();
}

void JdEffects::reset() noexcept
{
    distTone.reset(); cryBand.reset(); enhHp.reset();
    for (auto& s : spec) s.reset();
    std::fill (std::begin (apState), std::end (apState), 0.0f);
    phLfo = phFb = 0;
    chorusUnit.reset();
    dly.reset();
    rev.reset();
    early.reset();
    gateEnv = gateOpen = revRamp = flyPhase = inEnv = 0;
    wasQuiet = true;
}

void JdEffects::distortion (float* x, int n, const JdEffectParams& p) noexcept
{
    const float g = dbToGain (p.distDrive * 40.0f);
    const float out = p.distLevel * 1.2f;
    const int t = p.distType;
    distTone.setPeak (t == 5 ? 150.0f : 3000.0f, 0.7f, t == 5 ? 6.0f : (t == 0 || t == 3 ? -6.0f : 0.0f), sampleRate);
    cryBand.setPeak (1200.0f, 2.5f, 12.0f, sampleRate);
    for (int i = 0; i < n; ++i)
    {
        float v = x[i];
        if (t == 2) v = cryBand.process (v);              // CRY DRIVE: resonant mid emphasis
        v *= g;
        float y;
        switch (t)
        {
            case 0:  y = std::tanh (0.5f * v); break;                         // MELLOW DRIVE
            case 1:  y = std::tanh (v); break;                                // OVERDRIVE
            case 2:  y = std::tanh (v); break;                                // CRY DRIVE
            case 3:  y = 0.63662f * std::atan (2.0f * v); break;              // MELLOW DIST
            case 4:  y = v / (1.0f + std::abs (v)); break;                    // LIGHT DIST
            case 5:  y = std::tanh (v + 0.3f) - 0.2913f; break;               // FAT DIST (asymmetric)
            default: y = (v >= 0 ? 1.0f : -1.0f) * (1.0f - std::exp (-4.0f * std::abs (v))); break; // FUZZ
        }
        x[i] = distTone.process (y) * out;
    }
}

void JdEffects::phaser (float* x, int n, const JdEffectParams& p) noexcept
{
    const float inc = p.phRateHz / sampleRate;
    for (int i = 0; i < n; ++i)
    {
        phLfo += inc;
        if (phLfo >= 1.0f) phLfo -= 1.0f;
        const float sweep = std::sin (kTwoPi * phLfo);
        const float f = clampf (p.phManualHz * std::exp2 (2.0f * p.phDepth * sweep), 30.0f, sampleRate * 0.45f);
        const float tn = std::tan (kPi * f / sampleRate);
        const float a = (tn - 1.0f) / (tn + 1.0f);
        float y = x[i] + phFb * p.phReso * 0.9f;
        for (auto& s : apState) // first-order all-pass stages
        {
            const float o = a * y + s;
            s = y - a * o;
            y = o;
        }
        phFb = y;
        x[i] = x[i] + p.phMix * 0.5f * (y - x[i]);
    }
}

void JdEffects::spectrum (float* x, int n, const JdEffectParams& p) noexcept
{
    const float q = kSpectrumQ[std::max (0, std::min (4, p.specWidth))];
    for (int b = 0; b < 6; ++b)
        spec[b].setPeak (std::min (kSpectrumHz[b], sampleRate * 0.4f), q, p.specDb[b], sampleRate);
    for (int i = 0; i < n; ++i)
    {
        float v = x[i];
        for (auto& s : spec) v = s.process (v);
        x[i] = v;
    }
}

void JdEffects::enhancer (float* x, int n, const JdEffectParams& p) noexcept
{
    enhHp.setHighPass (2500.0f, 0.7f, sampleRate);
    const float k = 1.0f + p.enhSens * 9.0f;
    for (int i = 0; i < n; ++i)
    {
        const float hp = enhHp.process (x[i]);
        x[i] += p.enhMix * 0.5f * std::tanh (hp * k);
    }
}

void JdEffects::chorus (float* L, float* R, int n, const JdEffectParams& p) noexcept
{
    for (int i = 0; i < n; ++i) tmpM[(size_t) i] = 0.5f * (L[i] + R[i]);
    chorusUnit.process (tmpM.data(), tmpL.data(), tmpR.data(), n, p.chRateHz, p.chDelayMs,
                        p.chDepth * std::min (5.0f, p.chDelayMs), p.chFeedback, 0.0f, 1.0f);
    for (int i = 0; i < n; ++i)
    {
        L[i] += tmpL[(size_t) i] * p.chLevel;
        R[i] += tmpR[(size_t) i] * p.chLevel;
    }
}

void JdEffects::delay (float* L, float* R, int n, const JdEffectParams& p) noexcept
{
    const float c = std::max (1.0f, p.dlyTapMs[0] * 0.001f * sampleRate);
    const float lt = std::max (1.0f, p.dlyTapMs[1] * 0.001f * sampleRate);
    const float rt = std::max (1.0f, p.dlyTapMs[2] * 0.001f * sampleRate);
    for (int i = 0; i < n; ++i)
    {
        const float centre = dly.read (c), left = dly.read (lt), right = dly.read (rt);
        dly.push (0.5f * (L[i] + R[i]) + centre * p.dlyFeedback);   // feedback from the centre tap
        L[i] += left * p.dlyLevel[1] + centre * p.dlyLevel[0] * 0.7f;
        R[i] += right * p.dlyLevel[2] + centre * p.dlyLevel[0] * 0.7f;
    }
}

void JdEffects::reverb (float* L, float* R, int n, const JdEffectParams& p) noexcept
{
    const int type = p.rvType;
    const bool special = type >= 6;  // GATE, REVERSE, FLYING1/2
    // Time: 0.1 .. 20 s (ROOM/HALL) or 5 .. 500 ms (GATE/REVERSE/FLYING); the exact step table in
    // the manual does not fit the 0-100 data range, so a smooth exponential mapping is used.
    const float t = clampf (p.rvTime, 0.0f, 1.0f);
    const float timeSec = special ? 0.005f * std::pow (100.0f, t) : 0.1f * std::pow (200.0f, t);
    static constexpr float sizes[10] = { 0.25f, 0.4f, 0.6f, 0.7f, 0.8f, 0.95f, 0.6f, 0.6f, 0.7f, 0.8f };
    const float decay = special ? 2.5f : timeSec;
    const float damp = p.rvDampHz <= 0.0f ? sampleRate * 0.45f : p.rvDampHz;

    for (int i = 0; i < n; ++i) tmpM[(size_t) i] = 0.5f * (L[i] + R[i]);
    rev.process (tmpM.data(), tmpM.data(), tmpL.data(), tmpR.data(), n, sizes[type], decay, damp, p.rvPreMs);

    const float pre = std::max (1.0f, p.rvPreMs * 0.001f * sampleRate);
    static constexpr float erMs[4] = { 7.0f, 11.0f, 17.0f, 23.0f };
    const float envCoef = std::exp (-1.0f / (0.01f * sampleRate));
    const float rampInc = 1.0f / std::max (1.0f, timeSec * sampleRate);
    for (int i = 0; i < n; ++i)
    {
        const float in = tmpM[(size_t) i];
        early.push (in);
        float er = 0;
        for (int k = 0; k < 4; ++k) er += early.read (pre + erMs[k] * 0.001f * sampleRate) * (k % 2 ? -0.25f : 0.25f);
        float wl = tmpL[(size_t) i] + er * p.rvEarly, wr = tmpR[(size_t) i] + er * p.rvEarly;

        inEnv = std::max (std::abs (in), inEnv * envCoef);
        const bool quiet = inEnv < 0.02f;
        if (special && type <= 7 && wasQuiet && ! quiet) { gateOpen = timeSec; revRamp = 0.0f; } // onset
        wasQuiet = quiet;
        if (type == 6) // GATE: open for "time" after an onset, then close quickly
        {
            gateOpen -= 1.0f / sampleRate;
            gateEnv += ((gateOpen > 0.0f ? 1.0f : 0.0f) - gateEnv) * 0.01f;
            wl *= gateEnv; wr *= gateEnv;
        }
        else if (type == 7) // REVERSE: wet level rises over "time", then cuts
        {
            gateOpen -= 1.0f / sampleRate;
            revRamp = gateOpen > 0.0f ? std::min (1.0f, revRamp + rampInc) : revRamp * 0.995f;
            wl *= revRamp * revRamp; wr *= revRamp * revRamp;
        }
        else if (type >= 8) // FLYING 1/2: wet signal circles between the speakers
        {
            flyPhase += (type == 8 ? 0.4f : 1.1f) / sampleRate;
            if (flyPhase >= 1.0f) flyPhase -= 1.0f;
            const float pan = 0.5f + 0.5f * std::sin (kTwoPi * flyPhase);
            const float m = 0.5f * (wl + wr);
            wl = m * std::cos (pan * kPi * 0.5f) * 1.4f;
            wr = m * std::sin (pan * kPi * 0.5f) * 1.4f;
        }
        L[i] += wl * p.rvLevel;
        R[i] += wr * p.rvLevel;
    }
}

void JdEffects::process (const float* in, float* L, float* R, int n, const JdEffectParams& p) noexcept
{
    const int maxChunk = (int) mono.size();
    for (int off = 0; off < n; off += maxChunk)
    {
        const int len = std::min (maxChunk, n - off);
        std::copy (in + off, in + off + len, mono.begin());

        // ---- Group A (mono) ----
        int orderA[4];
        groupAOrder (p.seqA, orderA);
        for (int pos = 0; pos < 4; ++pos)
        {
            if (! p.aOn[pos]) continue; // block switches follow the sequence positions
            switch (orderA[pos])
            {
                case 0: distortion (mono.data(), len, p); break;
                case 1: phaser (mono.data(), len, p); break;
                case 2: spectrum (mono.data(), len, p); break;
                default: enhancer (mono.data(), len, p); break;
            }
        }

        // ---- Group B (stereo) ----
        for (int i = 0; i < len; ++i)
            l[(size_t) i] = r[(size_t) i] = dryL[(size_t) i] = dryR[(size_t) i] = mono[(size_t) i];
        int orderB[3];
        groupBOrder (p.seqB, orderB);
        for (int pos = 0; pos < 3; ++pos)
        {
            if (! p.bOn[pos]) continue;
            switch (orderB[pos])
            {
                case 0: chorus (l.data(), r.data(), len, p); break;
                case 1: delay (l.data(), r.data(), len, p); break;
                default: reverb (l.data(), r.data(), len, p); break;
            }
        }
        const float b = clampf (p.bBalance, 0.0f, 1.0f);
        for (int i = 0; i < len; ++i)
        {
            L[off + i] = dryL[(size_t) i] * (1.0f - b) + l[(size_t) i] * b;
            R[off + i] = dryR[(size_t) i] * (1.0f - b) + r[(size_t) i] * b;
        }
    }
}
} // namespace dc
