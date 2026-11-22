#include "Filters.h"
#include <juce_dsp/juce_dsp.h>

namespace vsx
{
namespace
{
    constexpr float kNl = 0.42f; // level scaling of the transistor/OTA nonlinearities (headroom ~ +/-2.4: three summed oscillators stay clean at drive 0)

    inline float sat (float x) noexcept { return fastTanh (x * kNl) * (1.0f / kNl); }

    /** Secant conductance tanh(a v) / (a v) used to linearise diode pairs around the last sample. */
    inline float secant (float v) noexcept
    {
        const float a = v * kNl;
        if (std::abs (a) < 1.0e-3f) return 1.0f - a * a * (1.0f / 3.0f);
        return fastTanh (a) / a;
    }

    // Diode-ladder calibration (normalised RC-ladder with diode conductances):
    //   generic diode ladder: capacitors (1,1,1,0.5) -> phase -180 deg at w = 1.4142, k_osc = 17.0, -3 dB (k=0) at 0.1494
    //   acid / 303 style:     capacitors (0.55,1,1,1) -> w180 = 1.4180, k_osc = 21.51, -3 dB at 0.1217
    constexpr float kDiodeCaps[4] = { 1.0f, 1.0f, 1.0f, 0.5f };
    constexpr float kAcidCaps[4] = { 0.55f, 1.0f, 1.0f, 1.0f };
    constexpr float kDiodeW180 = 1.4142f, kDiodeW3 = 0.1494f, kDiodeKosc = 17.0f;
    constexpr float kAcidW180 = 1.4180f, kAcidW3 = 0.1217f, kAcidKosc = 21.51f;
}

void AnalogFilter::prepare (double s)
{
    sr = s;
    dcCoef = (float) (2.0 * kPiD * 5.0 / sr);
    noise.seed (0x1234567u);
    reset();
}

void AnalogFilter::reset()
{
    for (auto& c : chans) c = Channel();
    // a new note starts at its own cutoff instead of gliding from the previous note's value
    gCur = 0.0f;
    gStep = 0.0f;
    gEff = 0.0f;
    fmFactor = 1.0f;
}

void AnalogFilter::setModel (int m, int md)
{
    const auto newModel = (FilterModel) juce::jlimit (0, (int) FilterModel::Count - 1, m);
    if (newModel != model)
        reset();
    model = newModel;
    mode = (FilterMode) juce::jlimit (0, 5, md);
}

void AnalogFilter::setTolerance (float amount, uint32_t seed)
{
    Rng r (seed);
    for (auto& t : stageTol) t = 1.0f + amount * 0.06f * r.bi();
    kTol = 1.0f + amount * 0.04f * r.bi();
    offsetTol = amount * 0.025f * r.bi();
    noise.seed (hash32 (seed + 77));
}

void AnalogFilter::update (float cutoffHz, float resonance, float drive, float charIn, int numSamples)
{
    cutoff = juce::jlimit (8.0f, 30000.0f, cutoffHz);
    res = juce::jlimit (0.0f, 1.0f, resonance);
    character = juce::jlimit (0.0f, 1.0f, charIn);

    float fm = cutoff;
    // small-signal brightness at low resonance, the resonant peak at the cutoff setting at high resonance
    const float rc = std::pow (juce::jlimit (0.0f, 1.0f, resonance), 0.7f);
    if (model == FilterModel::DiodeLadder) fm = cutoff / (std::pow (kDiodeW180, 0.5f + 0.5f * rc) * std::pow (kDiodeW3, 0.5f - 0.5f * rc));
    else if (model == FilterModel::Acid303) fm = cutoff / (std::pow (kAcidW180, 0.5f + 0.5f * rc) * std::pow (kAcidW3, 0.5f - 0.5f * rc));
    fm = std::min (fm, (float) (sr * 0.45));
    const float gTarget = (float) std::tan (kPiD * fm / sr);
    if (numSamples > 0 && gCur > 0.0f)
        gStep = (gTarget - gCur) / (float) numSamples;
    else
    {
        gCur = gTarget;
        gStep = 0.0f;
    }

    switch (model)
    {
        case FilterModel::MgLadder: k = 4.25f * res; break;
        case FilterModel::DiodeLadder: k = kDiodeKosc * 1.05f * res; break;
        case FilterModel::Acid303: k = kAcidKosc * (0.97f + 0.06f * character) * res; break;
        case FilterModel::MsSallenKey: k = 0.01f + 2.05f * res; break;
        case FilterModel::JpOta: k = 4.15f * res; break;
        case FilterModel::JnOta: k = 4.1f * res; break;
        case FilterModel::P5Cascade: k = 4.3f * res; break;
        case FilterModel::SemSvf: k = 1.0f - 0.96f * res; break;    // damping R
        case FilterModel::AnalogSvf: k = 1.0f - 1.02f * res; break; // damping R (slightly negative at the top: self-oscillation)
        default: break;
    }
    if (model != FilterModel::SemSvf && model != FilterModel::AnalogSvf)
        k *= kTol;

    // Drive: 0 -> 0 dB, 0.3 -> 3.9 dB, 0.6 -> 11.2 dB, 1 -> 24 dB into the nonlinear stages (was linear 0-30 dB, which put
    // typical preset settings 15 dB into saturation)
    float driveDb = 24.0f * std::pow (juce::jlimit (0.0f, 1.0f, drive), 1.5f);
    if (model == FilterModel::Acid303) driveDb += character * 8.0f;
    inGain = dbToGain (driveDb);
    outGain = 1.0f / std::pow (inGain, 0.6f);

    if (model == FilterModel::JnOta)
    {
        // 0 bass boost, 1 flat (the default range around 0.5), 2 / 3 high-pass at 240 / 720 Hz
        junoHpPos = character < 0.2f ? 0 : character < 0.6f ? 1 : character < 0.85f ? 2 : 3;
        const float hpHz = junoHpPos == 2 ? 240.0f : (junoHpPos == 3 ? 720.0f : 110.0f);
        junoHpG = (float) std::exp (-2.0 * kPiD * hpHz / sr);
    }
}

void AnalogFilter::process (float& left, float& right) noexcept
{
    gCur += gStep;
    gEff = fmFactor == 1.0f ? gCur : juce::jlimit (1.0e-5f, 6.3f, gCur * fmFactor); // tan (0.45 pi) = 6.31
    // shared one-pole stage coefficients for the ladder and OTA cascades (both channels use them)
    if (model == FilterModel::MgLadder || model == FilterModel::JpOta || model == FilterModel::JnOta || model == FilterModel::P5Cascade)
        for (size_t i = 0; i < 4; ++i)
        {
            const float gi = gEff * stageTol[i];
            const float inv = 1.0f / (1.0f + gi);
            otaG[i] = gi * inv;
            otaInv[i] = inv;
        }
    left = processChannel (chans[0], left, 0);
    right = processChannel (chans[1], right, 1);
}

float AnalogFilter::processChannel (Channel& c, float x, int) noexcept
{
    x = x * inGain + offsetTol + noise.bi() * 1.0e-6f;
    float y = 0.0f;
    switch (model)
    {
        case FilterModel::MgLadder: y = ladder (c, x); break;
        case FilterModel::DiodeLadder: y = diode (c, x, false); break;
        case FilterModel::Acid303: y = diode (c, x, true); break;
        case FilterModel::MsSallenKey: y = korg35 (c, x); break;
        case FilterModel::JpOta: y = otaCascade (c, x, 0); break;
        case FilterModel::JnOta: y = otaCascade (c, x, 1); break;
        case FilterModel::P5Cascade: y = otaCascade (c, x, 2); break;
        case FilterModel::SemSvf: y = svf (c, x, true); break;
        case FilterModel::AnalogSvf: y = svf (c, x, false); break;
        default: y = x; break;
    }
    y *= outGain;

    // remove the DC introduced by component offsets (~5 Hz)
    c.dc += (y - c.dc) * dcCoef;
    y -= c.dc;

    for (auto& s : c.s) s = flushDenormal (s);
    if (! std::isfinite (y))
    {
        c = Channel();
        y = 0.0f;
    }
    return y;
}

float AnalogFilter::tapMix (float y0, float y1, float y2, float y3, float y4) const noexcept
{
    const float lp4 = y4;
    switch (mode)
    {
        case FilterMode::LP24: return lp4;
        case FilterMode::LP12: return y2;
        case FilterMode::BP: return 4.0f * (y2 - 2.0f * y3 + y4);
        case FilterMode::HP: return y0 - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
        case FilterMode::Notch: return y0 - 2.0f * y1 + 2.0f * y2;
        case FilterMode::Morph:
        {
            const float bp = 4.0f * (y2 - 2.0f * y3 + y4);
            const float hp = y0 - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
            const float m = character * 2.0f;
            return m < 1.0f ? lp4 + (bp - lp4) * m : bp + (hp - bp) * (m - 1.0f);
        }
    }
    return lp4;
}

// ---------------------------------------------------------------------------------
// Transistor ladder: four nonlinear TPT stages v = G (tanh(in) - tanh(s)), global feedback.
float AnalogFilter::ladder (Channel& c, float x) noexcept
{
    // stage coefficients are shared by both channels and computed once per sample in process()
    float G[4], beta[4];
    for (int i = 0; i < 4; ++i)
    {
        G[i] = otaG[(size_t) i];
        beta[i] = c.s[(size_t) i] * otaInv[(size_t) i];
    }
    const float comp = (mode == FilterMode::Morph) ? 0.5f : character; // bass compensation
    const float xin = x * (1.0f + k * comp * 0.9f);
    const float Gp = G[0] * G[1] * G[2] * G[3];
    const float S = G[1] * G[2] * G[3] * beta[0] + G[2] * G[3] * beta[1] + G[3] * beta[2] + beta[3];
    const float y4est = (Gp * xin + S) / (1.0f + k * Gp);
    const float u = sat (xin - k * sat (y4est));

    float y[5];
    y[0] = u;
    float in = u;
    for (int i = 0; i < 4; ++i)
    {
        float& s = c.s[(size_t) i];
        const float v = (sat (in) - sat (s)) * G[i];
        y[i + 1] = v + s;
        s = y[i + 1] + v;
        in = y[i + 1];
    }
    return tapMix (y[0], y[1], y[2], y[3], y[4]);
}

// ---------------------------------------------------------------------------------
// Diode ladder as a nonlinear coupled RC network, solved implicitly (trapezoidal) each sample.
float AnalogFilter::diode (Channel& c, float x, bool acid) noexcept
{
    const float* caps = acid ? kAcidCaps : kDiodeCaps;
    const float g = gEff;
    const float y1p = c.s[0], y2p = c.s[1], y3p = c.s[2], y4p = c.s[3];
    const float z = acid ? c.fbHp : 0.0f; // feedback high-pass state (303 style)

    const float uPrev = c.xPrev - k * (y4p - z);
    const float c1 = secant (uPrev - y1p), c2 = secant (y1p - y2p), c3 = secant (y2p - y3p), c4 = secant (y3p - y4p);
    const float C1 = caps[0] / stageTol[0], C2 = caps[1] / stageTol[1], C3 = caps[2] / stageTol[2], C4 = caps[3] / stageTol[3];

    // A y + b x + d   (row-wise, divided by capacitances)
    const float a11 = -(c1 + c2) / C1, a12 = c2 / C1, a14 = -k * c1 / C1, b1 = c1 / C1, d1 = k * c1 * z / C1;
    const float a21 = c2 / C2, a22 = -(c2 + c3) / C2, a23 = c3 / C2;
    const float a32 = c3 / C3, a33 = -(c3 + c4) / C3, a34 = c4 / C3;
    const float a43 = c4 / C4, a44 = -c4 / C4;

    // RHS = y + g (A y + b (x + xPrev) + 2 d)
    const float r1 = y1p + g * (a11 * y1p + a12 * y2p + a14 * y4p + b1 * (x + c.xPrev) + 2.0f * d1);
    const float r2 = y2p + g * (a21 * y1p + a22 * y2p + a23 * y3p);
    const float r3 = y3p + g * (a32 * y2p + a33 * y3p + a34 * y4p);
    const float r4 = y4p + g * (a43 * y3p + a44 * y4p);

    // M = I - g A (tridiagonal + corner f at (1,4))
    const float d1m = 1.0f - g * a11, e1 = -g * a12, f = -g * a14;
    const float e2 = -g * a21, d2m = 1.0f - g * a22, e3 = -g * a23;
    const float e4 = -g * a32, d3m = 1.0f - g * a33, e5 = -g * a34;
    const float e6 = -g * a43, d4m = 1.0f - g * a44;

    const float d3p = d3m - e5 * e6 / d4m;
    const float r3p = r3 - e5 * r4 / d4m;
    const float d2p = d2m - e3 * e4 / d3p;
    const float r2p = r2 - e3 * r3p / d3p;
    const float alpha = (r4 - e6 * r3p / d3p) / d4m;
    const float betaC = e6 * e4 / (d3p * d4m);
    const float ef = e1 + f * betaC;
    const float y1 = (r1 - f * alpha - ef * r2p / d2p) / (d1m - ef * e2 / d2p);
    const float y2 = (r2p - e2 * y1) / d2p;
    const float y3 = (r3p - e4 * y2) / d3p;
    const float y4 = (r4 - e6 * y3) / d4m;

    c.s[0] = y1; c.s[1] = y2; c.s[2] = y3; c.s[3] = y4;
    c.xPrev = x;
    if (acid)
        c.fbHp += (y4 - c.fbHp) * (float) (2.0 * kPiD * 90.0 / sr);

    float comp = acid ? 0.35f : character;
    if (mode == FilterMode::Morph) comp = 0.5f;
    const float u = x - k * (y4 - z);
    const float out = tapMix (u, y1, y2, y3, y4) * (1.0f + k * comp * 0.9f);
    return sat (out);
}

// ---------------------------------------------------------------------------------
// Korg35-style Sallen-Key (after W. Pirkle's TPT derivation) with clipping in the loop.
float AnalogFilter::korg35 (Channel& c, float x) noexcept
{
    const float g = gEff;
    const float G = g / (1.0f + g);
    const float K = k;
    const float alpha0 = 1.0f / (1.0f - K * G + K * G * G);
    const float hard = 0.5f + 1.2f * (mode == FilterMode::Morph ? 0.5f : character);
    auto clip = [hard] (float v) { return fastTanh (v * hard) / hard; };
    auto onePoleLp = [G] (float& s, float in) { const float v = (in - s) * G; const float y = v + s; s = y + v; return y; };

    auto lowpass = [&] (float in)
    {
        // states: s0 LPF1, s1 LPF2, s2 HPF1(lp state)
        const float lpf2Beta = (K - K * G) / (1.0f + g);
        const float hpf1Beta = -1.0f / (1.0f + g);
        const float y1 = onePoleLp (c.s[0], in);
        const float S35 = hpf1Beta * c.s[2] + lpf2Beta * c.s[1];
        float u = alpha0 * (y1 + S35);
        u = clip (u);
        const float y = K * onePoleLp (c.s[1], u);
        const float hpIn = y;
        onePoleLp (c.s[2], hpIn); // HPF1 shares the LP state update (hp = in - lp)
        return K > 0.0f ? y / K : y;
    };

    auto highpass = [&] (float in)
    {
        // states: s3 HPF1, s4 HPF2, s5 LPF1
        const float hpf2Beta = -G / (1.0f + g);
        const float lpf1Beta = 1.0f / (1.0f + g);
        const float y1 = in - onePoleLp (c.s[3], in);
        const float S35 = hpf2Beta * c.s[4] + lpf1Beta * c.s[5];
        const float u = alpha0 * (y1 + S35);
        float y = K * u;
        y = clip (y);
        const float hp2 = y - onePoleLp (c.s[4], y);
        onePoleLp (c.s[5], hp2);
        return K > 0.0f ? y / K : y;
    };

    switch (mode)
    {
        case FilterMode::LP24: return onePoleLp (c.s[6], lowpass (x));
        case FilterMode::LP12: return lowpass (x);
        case FilterMode::BP:
        {
            const float pre = x - onePoleLp (c.s[7], x);
            return lowpass (pre) * 2.0f;
        }
        case FilterMode::HP:
        {
            // the Sallen-Key high-pass collapses to one pole at low resonance: an input pole keeps it at >= 12 dB/oct
            const float pre = x - onePoleLp (c.hpz2, x);
            return highpass (pre);
        }
        case FilterMode::Notch: return 0.5f * (lowpass (x) + highpass (x));
        case FilterMode::Morph:
        {
            const float l = lowpass (x), h = highpass (x);
            return l + (h - l) * character;
        }
    }
    return lowpass (x);
}

// ---------------------------------------------------------------------------------
// OTA cascades: variant 0 = JP (IR3109 style, 12/24 taps), 1 = JN (80017A style + HPF), 2 = P5 (CEM style)
float AnalogFilter::otaCascade (Channel& c, float x, int variant) noexcept
{
    // stage coefficients are shared by both channels and computed once per sample in process()
    float G[4], beta[4];
    for (int i = 0; i < 4; ++i)
    {
        G[i] = otaG[(size_t) i];
        beta[i] = c.s[(size_t) i] * otaInv[(size_t) i];
    }
    const float Gp = G[0] * G[1] * G[2] * G[3];
    const float S = G[1] * G[2] * G[3] * beta[0] + G[2] * G[3] * beta[1] + G[3] * beta[2] + beta[3];
    const float comp = variant == 0 ? 0.25f : (variant == 1 ? 0.65f : 0.4f); // bass retention as resonance rises
    const float xin = x * (1.0f + k * comp);
    const float y4est = (Gp * xin + S) / (1.0f + k * Gp);

    float fb;
    if (variant == 2)
    {
        // CEM-style feedback: cubic soft clip
        const float v = juce::jlimit (-1.5f, 1.5f, y4est * 0.66f);
        fb = (v - v * v * v * (4.0f / 27.0f)) * 1.5f;
    }
    else if (variant == 1)
        fb = fastTanh (y4est * 0.45f) / 0.45f;
    else
        fb = sat (y4est);

    float u = xin - k * fb;
    const float stageDrive = variant == 0 ? (0.45f + (mode == FilterMode::Morph ? 0.5f : character) * 0.9f)
                                          : (variant == 1 ? 0.4f : 0.55f);
    const float asym = variant == 2 ? 0.08f + (mode == FilterMode::Morph ? 0.0f : character) * 0.16f : 0.0f;
    const float asymOffset = asym != 0.0f ? fastTanh (asym) : 0.0f;
    const float invDrive = 1.0f / stageDrive;
    auto ota = [stageDrive, asym, asymOffset, invDrive] (float d)
    {
        if (asym != 0.0f)
            return (fastTanh (d * stageDrive + asym) - asymOffset) * invDrive;
        return fastTanh (d * stageDrive) * invDrive;
    };
    u = ota (u);

    float y[5];
    y[0] = u;
    float in = u;
    for (int i = 0; i < 4; ++i)
    {
        float& s = c.s[(size_t) i];
        const float v = G[i] * ota (in - s);
        y[i + 1] = v + s;
        s = y[i + 1] + v;
        in = y[i + 1];
    }

    float out = tapMix (y[0], y[1], y[2], y[3], y[4]);
    if (variant == 1)
    {
        // Juno-style non-resonant HPF / bass boost (Character selects position 0..3)
        c.hpz = out + (c.hpz - out) * junoHpG;
        if (junoHpPos == 0) out = out + 0.6f * c.hpz;     // low-shelf boost
        else if (junoHpPos >= 2) out = out - c.hpz;       // one-pole HPF
    }
    return out;
}

// ---------------------------------------------------------------------------------
float AnalogFilter::svf (Channel& c, float x, bool sem) noexcept
{
    // input stage: soft saturation driven by the Drive control (inGain), so Drive shapes the tone
    // of the state-variable models too and not only the resonance path
    {
        const float a = sem ? 0.32f : 0.26f;
        x = fastTanh (x * a) / a;
    }
    const float g = gEff;
    const float R = std::max (sem ? 0.04f : -0.025f, k);
    float& s1 = c.s[0];
    float& s2 = c.s[1];
    const float hp = (x - (2.0f * R + g) * s1 - s2) / (1.0f + 2.0f * R * g + g * g);
    const float bp = g * hp + s1;
    s1 = g * hp + bp;
    const float lp = g * bp + s2;
    s2 = g * bp + lp;

    // saturating integrator: limits self-oscillation (Analog SVF) / adds mild warmth (SEM)
    const float sc = sem ? 0.35f : 0.55f;
    s1 = fastTanh (s1 * sc) / sc;

    const float bpN = bp * 2.0f * std::max (R, 0.05f);
    const float notch = x - 2.0f * R * bp;

    switch (mode)
    {
        case FilterMode::LP24:
        {
            // second, gently damped stage for 24 dB/oct
            float& t1 = c.s[2];
            float& t2 = c.s[3];
            const float R2 = 0.75f;
            const float hp2 = (lp - (2.0f * R2 + g) * t1 - t2) / (1.0f + 2.0f * R2 * g + g * g);
            const float bp2 = g * hp2 + t1;
            t1 = g * hp2 + bp2;
            const float lp2 = g * bp2 + t2;
            t2 = g * bp2 + lp2;
            return lp2;
        }
        case FilterMode::LP12: return lp;
        case FilterMode::BP: return bpN;
        case FilterMode::HP: return hp;
        case FilterMode::Notch: return notch;
        case FilterMode::Morph:
        {
            const float m = character;
            if (sem)
                return m < 0.5f ? lp + (notch - lp) * (m * 2.0f) : notch + (hp - notch) * (m * 2.0f - 1.0f);
            return m < 0.5f ? lp + (bpN - lp) * (m * 2.0f) : bpN + (hp - bpN) * (m * 2.0f - 1.0f);
        }
    }
    return lp;
}

// ---------------------------------------------------------------------------------
void measureFilterResponse (int model, int mode, float cutoff, float resonance, float drive, float character,
                            double sampleRate, const float* freqs, float* magsDb, int numFreqs)
{
    AnalogFilter f;
    f.prepare (sampleRate);
    f.setModel (model, mode);
    f.setTolerance (0.0f, 1);
    f.update (cutoff, resonance, drive, character, 0);

    constexpr int order = 13;
    constexpr int n = 1 << order;
    std::vector<float> l ((size_t) n * 2, 0.0f), r ((size_t) n * 2, 0.0f);
    const float amp = 0.01f;
    for (int i = 0; i < n; ++i)
    {
        float a = i == 0 ? amp : 0.0f, b = a;
        f.process (a, b);
        // fade the tail to limit leakage from self-oscillation
        const float w = i > n * 3 / 4 ? 0.5f + 0.5f * std::cos (kPi * (float) (i - n * 3 / 4) / (float) (n / 4)) : 1.0f;
        l[(size_t) i] = a * w / amp;
    }
    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (l.data(), true);
    for (int i = 0; i < numFreqs; ++i)
    {
        const float bin = freqs[i] / (float) sampleRate * (float) n;
        const int b0 = juce::jlimit (0, n / 2 - 1, (int) bin);
        const float fr = bin - (float) b0;
        const float m = l[(size_t) b0] + fr * (l[(size_t) b0 + 1] - l[(size_t) b0]);
        magsDb[i] = 20.0f * std::log10 (std::max (m, 1.0e-6f));
    }
}

} // namespace vsx
