#include "Oscillator.h"

namespace vsx
{
namespace
{
    struct SineTable
    {
        static constexpr int N = 4096;
        std::array<float, N + 1> t {};
        SineTable() { for (int i = 0; i <= N; ++i) t[(size_t) i] = (float) std::sin (2.0 * kPiD * i / N); }
        float operator() (float phase) const noexcept
        {
            const float idx = phase * (float) N;
            const int i = (int) idx;
            return t[(size_t) i] + (idx - (float) i) * (t[(size_t) i + 1] - t[(size_t) i]);
        }
    };

    const SineTable& sineTable()
    {
        static const SineTable s;
        return s;
    }

    /** Band-limited sine (frame 0) and triangle (frame 1) for the Analog engine. */
    const Wavetable* analogTables()
    {
        static const std::shared_ptr<Wavetable> t = []
        {
            std::vector<std::vector<float>> frames (2, std::vector<float> (Wavetable::kSize));
            for (int i = 0; i < Wavetable::kSize; ++i)
            {
                const float p = (float) i / (float) Wavetable::kSize;
                frames[0][(size_t) i] = std::sin (kTwoPi * p);
                const float q = p + 0.25f;
                const float qq = q - std::floor (q);
                frames[1][(size_t) i] = 1.0f - 4.0f * std::abs (qq - 0.5f);
            }
            return Wavetable::fromFrames (frames, "analog", "internal:analog", true);
        }();
        return t.get();
    }

    inline float wrap01 (float p) noexcept { return p - std::floor (p); }

    inline float foldTri (float x) noexcept
    {
        x = (x + 1.0f) * 0.25f;
        x = x - std::floor (x);
        return 1.0f - 4.0f * std::abs (x - 0.5f);
    }

    inline float hermite (const float* d, int i, float f) noexcept
    {
        const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    inline float grainWindow (int type, float x) noexcept
    {
        switch (type)
        {
            case 1: return 1.0f - std::abs (2.0f * x - 1.0f);
            case 2:
                if (x < 0.25f) return 0.5f - 0.5f * std::cos (kPi * x / 0.25f);
                if (x > 0.75f) return 0.5f - 0.5f * std::cos (kPi * (1.0f - x) / 0.25f);
                return 1.0f;
            case 3: return x < 0.05f ? x / 0.05f : std::exp (-(x - 0.05f) * 5.0f);
            default: return 0.5f - 0.5f * std::cos (kTwoPi * x);
        }
    }
}

// =================================================================================
void OscVoice::prepare (double s)
{
    sr = s;
    fmRenderer.prepare (s);
    vaTables = analogTables();
    (void) sineTable();
}

void OscVoice::computeUnisonLayout (int n, float detune, float blend, float spread) noexcept
{
    n = juce::jlimit (1, kMaxUnison, n);
    numUnison = n;
    if (n == 1)
    {
        detuneRatio[0] = 1.0f;
        gainL[0] = gainR[0] = 1.0f;
        return;
    }

    float sumSq = 0.0f;
    const float detuneSemis = std::pow (detune, 1.5f) * 1.0f;
    const float innerLimit = (n % 2 == 1) ? 1.0e-3f : 1.0f / (float) (n - 1) + 1.0e-3f;
    for (int i = 0; i < n; ++i)
    {
        const float u = 2.0f * (float) i / (float) (n - 1) - 1.0f;
        detuneRatio[(size_t) i] = std::exp2 (u * detuneSemis / 12.0f);
        const float g = std::abs (u) <= innerLimit ? 1.0f : blend;
        const float pan = juce::jlimit (-1.0f, 1.0f, u * spread);
        const float ang = (pan + 1.0f) * kPi * 0.25f;
        gainL[(size_t) i] = g * std::cos (ang) * 1.41421356f;
        gainR[(size_t) i] = g * std::sin (ang) * 1.41421356f;
        sumSq += g * g;
    }
    const float norm = 1.0f / std::sqrt (std::max (1.0f, sumSq));
    for (int i = 0; i < n; ++i) { gainL[(size_t) i] *= norm; gainR[(size_t) i] *= norm; }
}

void OscVoice::resolveZone() noexcept
{
    zone = nullptr;
    zoneOwner = res.multi;
    if (res.multi != nullptr)
        zone = res.multi->find (note, velocity);
    if (zone != nullptr)
    {
        zoneGain = dbToGain (zone->gainDb);
        zonePan = zone->pan;
    }
}

void OscVoice::noteOn (const OscTick& t, int n, int vel, const OscResourcesView& r, uint32_t seed)
{
    tick = t;
    res = r;
    note = n;
    velocity = vel;
    rng.seed (seed);
    active = t.on;
    computeUnisonLayout (t.unison, t.detune, t.blend, t.spread);
    fmSeed = seed;
    fmStarted = false;
    if (t.mode == (int) OscMode::FM)
    {
        fmRenderer.setPatch (r.fm);
        fmRenderer.setPitch (t.pitchSemis, t.fmAmount);
        fmRenderer.noteOn (r.fm != nullptr ? *r.fm : fm::Patch::init(), n, vel, t.pitchSemis, seed);
        fmStarted = true;
    }

    for (int u = 0; u < kMaxUnison; ++u)
        phase[(size_t) u] = wrap01 (t.phase / 360.0f + rng.uni() * t.phaseRand);

    baseIncCur = midiToHz (t.pitchSemis) / (float) sr;
    baseIncStep = 0.0f;

    // sample start
    resolveZone();
    const SampleData* s = (t.mode == (int) OscMode::Multisample) ? (zone ? zone->sample.get() : nullptr) : res.sample;
    double start = 0.0;
    if (s != nullptr)
    {
        const double endFrac = juce::jlimit (0.0, 1.0, (double) t.sampleEnd);
        start = (double) t.position * (double) s->length;
        if (t.mode == (int) OscMode::Multisample && zone != nullptr) start += zone->offset;
        start = juce::jlimit (0.0, std::max (0.0, endFrac * (double) s->length - 2.0), start);
    }
    for (int u = 0; u < kMaxUnison; ++u)
    {
        spos[(size_t) u] = start;
        sdir[(size_t) u] = 1;
        sended[(size_t) u] = false;
    }

    for (auto& g : grains) g.active = false;
    grainTimer = 0.0;
    grainCounter = 0;

    specTime = 0.0;
    std::fill (ampCur.begin(), ampCur.end(), 0.0f);
    std::fill (ampStep.begin(), ampStep.end(), 0.0f);
    for (int u = 0; u < kMaxUnison; ++u)
        for (int k = 0; k < kPartials; ++k)
        {
            float ph = rng.uni();
            if (t.source == 0 && res.wavetable != nullptr)
                ph = wrap01 ((res.wavetable->partialPhases (0)[k] + kPi * 0.5f) / kTwoPi + (u > 0 ? rng.uni() * t.phaseRand : 0.0f));
            pphase[(size_t) u][(size_t) k] = ph;
        }

    levelRamp.jump (t.level);
    panRamp.jump (t.pan);
    wrapFrac = -1.0f;
    lastMono = 0.0f;
}

void OscVoice::beginTick (const OscTick& t, int numSamples, const OscResourcesView& r)
{
    const bool layoutChanged = t.unison != numUnison || std::abs (t.detune - tick.detune) > 1.0e-5f
                               || std::abs (t.blend - tick.blend) > 1.0e-5f || std::abs (t.spread - tick.spread) > 1.0e-5f;
    const int oldUnison = numUnison;
    tick = t;
    active = t.on;
    if (t.mode == (int) OscMode::FM)
    {
        fmRenderer.setPatch (r.fm); // re-read every tick: the editor may publish a new voice at any time
        fmRenderer.setPitch (t.pitchSemis, t.fmAmount);
        if (! fmStarted)
        {
            // switched to FM while the note was held
            fmRenderer.noteOn (r.fm != nullptr ? *r.fm : fm::Patch::init(), note, velocity, t.pitchSemis, fmSeed);
            fmStarted = true;
        }
    }
    if (layoutChanged)
    {
        computeUnisonLayout (t.unison, t.detune, t.blend, t.spread);
        for (int u = oldUnison; u < numUnison; ++u)
        {
            phase[(size_t) u] = wrap01 (phase[0] + rng.uni());
            spos[(size_t) u] = spos[0]; sdir[(size_t) u] = sdir[0]; sended[(size_t) u] = sended[0];
        }
    }

    if (r.multi != zoneOwner || r.sample != res.sample)
    {
        res = r;
        resolveZone();
    }
    res = r;

    const float targetInc = midiToHz (t.pitchSemis) / (float) sr;
    baseIncStep = numSamples > 0 ? (targetInc - baseIncCur) / (float) numSamples : 0.0f;
    if (baseIncCur <= 0.0f) { baseIncCur = targetInc; baseIncStep = 0.0f; }

    levelRamp.set (t.level, numSamples);
    panRamp.set (t.pan, numSamples);

    const float maxDetune = numUnison > 1 ? detuneRatio[(size_t) numUnison - 1] : 1.0f;
    float harmonicMult = 1.0f;
    switch ((WarpMode) t.warpMode)
    {
        case WarpMode::Sync: harmonicMult = 1.0f + t.warpAmount * 15.0f; break;
        case WarpMode::Pwm: harmonicMult = 1.0f / std::max (0.05f, 1.0f - 0.95f * t.warpAmount); break;
        case WarpMode::BendPlus:
        case WarpMode::BendMinus: harmonicMult = 1.0f + t.warpAmount * 4.0f; break;
        case WarpMode::Asym: harmonicMult = 1.0f / std::max (0.02f, 2.0f * (0.5f - 0.49f * t.warpAmount)); break;
        default: break;
    }
    if (t.xmodMode == (int) XModMode::FM) harmonicMult += t.xmodAmount * 6.0f;
    if (t.xmodMode == (int) XModMode::PM) harmonicMult += t.xmodAmount * 4.0f;

    const auto mode = (OscMode) t.mode;
    const Wavetable* wt = res.wavetable;
    if ((mode == OscMode::Wavetable || mode == OscMode::Spectral || mode == OscMode::Granular) && wt != nullptr)
    {
        const float fp = juce::jlimit (0.0f, 1.0f, t.position) * (float) (wt->numFrames - 1);
        frameA = juce::jlimit (0, wt->numFrames - 1, (int) fp);
        frameB = std::min (frameA + 1, wt->numFrames - 1);
        frameFrac = fp - (float) frameA;
    }
    if (mode == OscMode::Wavetable || mode == OscMode::Analog)
        level = Wavetable::levelFor (std::abs (targetInc) * maxDetune, harmonicMult);

    if (mode == OscMode::Sample || mode == OscMode::Multisample)
    {
        const SampleData* s = mode == OscMode::Multisample ? (zone ? zone->sample.get() : nullptr) : res.sample;
        if (s != nullptr)
        {
            int lm = t.loopMode;
            double ls = (double) t.loopStart * s->length, le = (double) t.loopEnd * s->length;
            if (mode == OscMode::Multisample && zone != nullptr && zone->loopMode >= 0)
            {
                lm = zone->loopMode;
                ls = zone->loopStart >= 0 ? zone->loopStart : (s->hasLoop ? s->loopStart : 0);
                le = zone->loopEnd >= 0 ? zone->loopEnd : (s->hasLoop ? s->loopEnd : s->length);
            }
            else if (s->hasLoop && t.loopStart <= 1.0e-4f && t.loopEnd >= 0.9999f)
            {
                ls = s->loopStart; le = s->loopEnd;
            }
            // the playable region ends at Sample End; loops live inside it
            const double hardEnd = juce::jlimit (4.0, (double) s->length, (double) juce::jlimit (0.0f, 1.0f, t.sampleEnd) * s->length);
            ls = juce::jlimit (0.0, hardEnd - 2.0, ls);
            le = juce::jlimit (ls + 2.0, hardEnd, le);
            activeLoopMode = lm;
            loopS = ls; loopE = le;
            playEnd = hardEnd;
            // crossfade blends the loop end with the material just before the loop start, so it can be no
            // longer than that material (a loop starting at 0 cannot crossfade)
            xfadeLen = lm == 1 ? std::min ((double) t.loopXfade * (le - ls), ls) : 0.0;
            sampleRateRatio = s->sampleRate / sr;
        }
    }

    if (mode == OscMode::Spectral)
    {
        std::array<float, kPartials> src {};
        const SampleData* s = res.sample;
        if (t.source == 1 && s != nullptr && s->numSpecFrames > 0)
        {
            double fpos = (double) t.position * s->numSpecFrames + specTime / std::max (1.0e-4, s->specHopSeconds) * t.specSpeed;
            if (t.loopMode != 0) fpos = std::fmod (fpos, (double) s->numSpecFrames);
            fpos = juce::jlimit (0.0, (double) s->numSpecFrames - 1.0, fpos);
            const int f0 = (int) fpos;
            const int f1 = std::min (f0 + 1, s->numSpecFrames - 1);
            const float fr = (float) (fpos - f0);
            const float* a = s->specAmps.data() + (size_t) f0 * SampleData::kSpecPartials;
            const float* b = s->specAmps.data() + (size_t) f1 * SampleData::kSpecPartials;
            for (int k = 0; k < kPartials; ++k) src[(size_t) k] = a[k] + fr * (b[k] - a[k]);
        }
        else if (wt != nullptr)
        {
            const float* a = wt->partialAmps (frameA);
            const float* b = wt->partialAmps (frameB);
            for (int k = 0; k < kPartials; ++k) src[(size_t) k] = a[k] + frameFrac * (b[k] - a[k]);
        }
        specTime += (double) numSamples / sr;

        const float formantShift = std::exp2 (-t.specFormant / 12.0f);
        const float exponent = 1.0f + 0.15f * t.specStretch;
        float srcEnergy = 0.0f, dstEnergy = 0.0f;
        std::array<float, kPartials> target {};
        activePartials = 0;
        for (int k = 1; k <= kPartials; ++k)
        {
            srcEnergy += src[(size_t) k - 1] * src[(size_t) k - 1];
            const float hk = (float) k * formantShift;
            float amp = 0.0f;
            if (hk >= 1.0f && hk <= (float) kPartials)
            {
                const int i0 = (int) hk - 1;
                const int i1 = std::min (i0 + 1, kPartials - 1);
                const float fr = hk - std::floor (hk);
                amp = src[(size_t) i0] + fr * (src[(size_t) i1] - src[(size_t) i0]);
            }
            else if (hk < 1.0f)
                amp = src[0] * hk;
            amp *= std::pow ((float) k, t.specTilt * 1.5f);
            if (k > 1)
            {
                if (k % 2 == 0) amp *= juce::jlimit (0.0f, 2.0f, 1.0f + t.specOddEven);
                else amp *= juce::jlimit (0.0f, 2.0f, 1.0f - t.specOddEven);
            }
            if (k > t.specPartials) amp = 0.0f;
            ratio[(size_t) k - 1] = std::pow ((float) k, exponent);
            if (targetInc * ratio[(size_t) k - 1] * maxDetune > 0.45f) amp = 0.0f;
            target[(size_t) k - 1] = amp;
            dstEnergy += amp * amp;
        }
        const float norm = dstEnergy > 1.0e-12f ? juce::jlimit (0.25f, 4.0f, std::sqrt (srcEnergy / dstEnergy)) : 1.0f;
        for (int k = 0; k < kPartials; ++k)
        {
            target[(size_t) k] *= norm;
            ampStep[(size_t) k] = numSamples > 0 ? (target[(size_t) k] - ampCur[(size_t) k]) / (float) numSamples : 0.0f;
            if (target[(size_t) k] > 1.0e-6f || ampCur[(size_t) k] > 1.0e-6f) activePartials = k + 1;
        }
    }
}

float OscVoice::warpPhase (float p, float& hm) const noexcept
{
    const float a = tick.warpAmount;
    hm = 1.0f;
    switch ((WarpMode) tick.warpMode)
    {
        case WarpMode::Sync:
        {
            const float q = p * (1.0f + a * 15.0f);
            return q - std::floor (q);
        }
        case WarpMode::BendPlus:
        {
            const float k = 1.0f + a * 8.0f;
            return p / (p + (1.0f - p) * k);
        }
        case WarpMode::BendMinus:
        {
            const float k = 1.0f / (1.0f + a * 8.0f);
            return p / (p + (1.0f - p) * k);
        }
        case WarpMode::Pwm:
        {
            const float w = 1.0f - 0.95f * a;
            return p < w ? p / w : 0.0f;
        }
        case WarpMode::Mirror:
        {
            const float q = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
            return wrap01 (p + (q * 0.5f - p) * a);
        }
        case WarpMode::Asym:
        {
            const float m = 0.5f - 0.49f * a;
            return p < m ? p / m * 0.5f : 0.5f + (p - m) / (1.0f - m) * 0.5f;
        }
        case WarpMode::Quantize:
        {
            const float steps = 2.0f + (1.0f - a) * (1.0f - a) * 1022.0f;
            return std::floor (p * steps) / steps;
        }
        default: return p;
    }
}

float OscVoice::renderWavetable (int u, float incU, float pm) noexcept
{
    const Wavetable* wt = res.wavetable;
    float& p = phase[(size_t) u];
    float y = 0.0f;
    if (wt != nullptr)
    {
        float hm;
        const float wp = warpPhase (pm != 0.0f ? wrap01 (p + pm) : p, hm);
        const float* ta = wt->table (frameA, level);
        const float a = Wavetable::read (ta, wp);
        if (frameFrac > 1.0e-4f)
        {
            const float b = Wavetable::read (wt->table (frameB, level), wp);
            y = a + frameFrac * (b - a);
        }
        else
            y = a;
    }
    const float np = p + incU;
    if (u == numUnison / 2 && (np >= 1.0f || np < 0.0f))
        wrapFrac = incU != 0.0f ? juce::jlimit (0.0f, 1.0f, wrap01 (np) / std::abs (incU)) : 0.0f;
    p = wrap01 (np);
    return y;
}

OscVoice::AnalogSetup OscVoice::analogSetup() const noexcept
{
    AnalogSetup a;
    a.dtMul = tick.warpMode == (int) WarpMode::Sync ? 1.0f + tick.warpAmount * 15.0f : 1.0f;
    a.pw = tick.pulseWidth;
    a.pwm = tick.warpMode == (int) WarpMode::Pwm;
    if (a.pwm) a.pw = juce::jlimit (0.02f, 0.98f, a.pw * (1.0f - 0.9f * tick.warpAmount));
    a.warp = ! a.pwm && tick.warpMode != 0;
    const float s = juce::jlimit (0.0f, 1.0f, tick.position) * 3.0f;
    a.seg = std::min (2, (int) s);
    a.t = s - (float) a.seg;
    // positions within 1.5 % of a pure shape render that shape only (a <= 1.5 % blend, about -36 dB, is
    // inaudible but doubled the per-sample cost, e.g. the common "saw" setting 0.67 vs. the exact 2/3)
    if (a.t < 0.015f) a.t = 0.0f;
    else if (a.t > 0.985f && a.seg < 2) { ++a.seg; a.t = 0.0f; }
    return a;
}

// inlined into the unison loop of render(): the per-sample setup above is shared by every unison voice
// (callgrind: the per-call setup and call overhead were ~8 % of a six-voice render)
forcedinline float OscVoice::renderAnalog (int u, float incU, float pm, const AnalogSetup& a) noexcept
{
    float& p = phase[(size_t) u];
    const float dt = std::min (0.5f, std::abs (incU) * a.dtMul);
    float hm;
    float wp = p;
    const float pw = a.pw;
    if (a.warp) wp = warpPhase (p, hm);
    if (pm != 0.0f) wp = wrap01 (wp + pm);
    const int seg = a.seg;
    const float t = a.t;
    float y = 0.0f;

    auto sine = [&] { return Wavetable::read (vaTables->table (0, level), wp); };
    auto tri = [&] { return Wavetable::read (vaTables->table (1, level), wp); };
    auto saw = [&] { return 2.0f * wp - 1.0f - polyBlep4 (wp, dt); };
    auto sq = [&]
    {
        float v = wp < pw ? 1.0f : -1.0f;
        v += polyBlep4 (wp, dt);
        v -= polyBlep4 (wrap01 (wp - pw + 1.0f), dt);
        return v;
    };

    // each waveform is evaluated once (morphing previously evaluated the first shape twice)
    if (seg == 0) { const float a = sine(); y = t < 1.0e-4f ? a : a + t * (tri() - a); }
    else if (seg == 1) { const float a = tri(); y = t < 1.0e-4f ? a : a + t * (saw() - a); }
    else { const float a = saw(); y = t < 1.0e-4f ? a : a + t * (sq() - a); }

    const float np = p + incU;
    if (u == numUnison / 2 && (np >= 1.0f || np < 0.0f))
        wrapFrac = incU != 0.0f ? juce::jlimit (0.0f, 1.0f, wrap01 (np) / std::abs (incU)) : 0.0f;
    p = wrap01 (np);
    return y;
}

float OscVoice::readSample (const SampleData& s, int lv, int ch, double pos) const noexcept
{
    const double scale = 1.0 / (double) (1 << lv);
    const double lp = pos * scale;
    const int len = s.levelLength (lv);
    const int i = (int) lp;
    if (i < 0 || i >= len) return 0.0f;
    return hermite (s.channel (lv, ch), i, (float) (lp - i));
}

void OscVoice::renderSampleVoices (float fmMul, float& l, float& r) noexcept
{
    const bool multi = tick.mode == (int) OscMode::Multisample;
    const SampleData* s = multi ? (zone ? zone->sample.get() : nullptr) : res.sample;
    if (s == nullptr) return;

    // Single samples: the Root Key control (default C4) unless Root Auto uses the key and tuning stored in or
    // detected from the file. fineTuneCents is the measured pitch offset of the recording, so its actual pitch is
    // root + cents / 100 (the previous code subtracted it, doubling the error). SFZ "tune" raises the pitch.
    const float rootPitch = multi && zone != nullptr ? (float) zone->rootNote - zone->tuneCents / 100.0f
                          : tick.rootAuto ? (float) s->rootNote + s->fineTuneCents / 100.0f
                                          : (float) tick.rootNote;
    const float pitchRatio = baseIncCur * (float) sr / midiToHz (rootPitch);
    const double rate0 = (double) pitchRatio * sampleRateRatio * fmMul;
    const int maxLevel = (int) s->mips.size() - 1;
    const double absRate = std::abs (rate0);
    const int lv = absRate > 1.6 ? std::min (maxLevel, (int) std::floor (std::log2 (absRate * 0.8))) : 0;
    const bool stereo = s->numChannels > 1 && numUnison == 1;
    const float zg = multi ? zoneGain : 1.0f;

    for (int u = 0; u < numUnison; ++u)
    {
        if (sended[(size_t) u]) continue;
        double& pos = spos[(size_t) u];
        int& dir = sdir[(size_t) u];
        const double rate = rate0 * detuneRatio[(size_t) u];

        float yl = readSample (*s, lv, 0, pos);
        float yr = s->numChannels > 1 ? readSample (*s, lv, 1, pos) : yl;
        if (activeLoopMode == 1 && xfadeLen > 1.0 && pos > loopE - xfadeLen)
        {
            const float t = (float) ((pos - (loopE - xfadeLen)) / xfadeLen);
            const double alt = pos - (loopE - loopS);
            yl = yl * (1.0f - t) + readSample (*s, lv, 0, alt) * t;
            yr = yr * (1.0f - t) + (s->numChannels > 1 ? readSample (*s, lv, 1, alt) : readSample (*s, lv, 0, alt)) * t;
        }

        if (stereo) { l += yl * zg; r += yr * zg; }
        else
        {
            const float m = 0.5f * (yl + yr) * zg;
            l += m * gainL[(size_t) u];
            r += m * gainR[(size_t) u];
        }

        pos += rate * dir;
        const double span = loopE - loopS;
        if (activeLoopMode == 1)
        {
            // wrap by any number of loop lengths (a high note on a short loop can step past several)
            if (pos >= loopE) pos = loopS + std::fmod (pos - loopS, span);
        }
        else if (activeLoopMode == 2 && (pos >= loopE || dir < 0))
        {
            // ping-pong as a triangle fold of the unfolded travel distance, valid for any step size
            const double travelled = dir > 0 ? pos - loopS : 2.0 * span - (pos - loopS);
            double u2 = std::fmod (travelled, 2.0 * span);
            if (u2 < 0.0) u2 += 2.0 * span;
            if (u2 < span) { pos = loopS + u2; dir = 1; }
            else { pos = loopS + (2.0 * span - u2); dir = -1; }
        }
        // a looping voice stays inside [loopS, loopE) for ever; only one-shot playback ends at Sample End
        if (activeLoopMode == 0 ? (pos >= std::min (playEnd, (double) s->length - 1.0) || pos < 0.0) : (pos < 0.0 || pos >= (double) s->length))
            sended[(size_t) u] = true;
    }
    if (multi && std::abs (zonePan) > 1.0e-3f)
    {
        l *= zonePan > 0.0f ? 1.0f - zonePan : 1.0f;
        r *= zonePan < 0.0f ? 1.0f + zonePan : 1.0f;
    }
}

void OscVoice::spawnGrain() noexcept
{
    Grain* g = nullptr;
    for (auto& gr : grains)
        if (! gr.active) { g = &gr; break; }
    if (g == nullptr) return;

    const int u = grainCounter++ % numUnison;
    g->active = true;
    g->age = 0;
    g->length = std::max (8, (int) (tick.grainSize * 0.001 * sr));
    const float pitchRand = tick.grainPitchRand > 0.0f ? std::exp2 (rng.bi() * tick.grainPitchRand / 12.0f) : 1.0f;
    const float pan = juce::jlimit (-1.0f, 1.0f, rng.bi() * tick.grainSpread);
    const float ang = (pan + 1.0f) * kPi * 0.25f;
    const float ul = gainL[(size_t) u], ur = gainR[(size_t) u];
    g->panL = std::cos (ang) * 1.41421356f * (numUnison > 1 ? ul : 1.0f);
    g->panR = std::sin (ang) * 1.41421356f * (numUnison > 1 ? ur : 1.0f);

    const SampleData* s = res.sample;
    const float pos = juce::jlimit (0.0f, 1.0f, tick.position + tick.grainSpray * rng.bi() * 0.5f);
    if (tick.source == 1 && s != nullptr)
    {
        const float rootPitch = (float) tick.rootNote - s->fineTuneCents / 100.0f;
        const float pitchRatio = baseIncCur * (float) sr / midiToHz (rootPitch);
        g->rate = (double) pitchRatio * sampleRateRatio * detuneRatio[(size_t) u] * pitchRand;
        if (sampleRateRatio <= 0.0) g->rate = pitchRatio;
        g->rate = (double) pitchRatio * (s->sampleRate / sr) * detuneRatio[(size_t) u] * pitchRand;
        g->pos = (double) pos * (s->length - 1);
        g->frame = -1;
    }
    else if (res.wavetable != nullptr)
    {
        g->frame = juce::jlimit (0, res.wavetable->numFrames - 1, (int) std::round (pos * (float) (res.wavetable->numFrames - 1)));
        g->phase = rng.uni();
        g->phaseInc = baseIncCur * detuneRatio[(size_t) u] * pitchRand;
    }
    else
        g->active = false;
}

void OscVoice::renderGranular (float& l, float& r) noexcept
{
    grainTimer -= 1.0;
    if (grainTimer <= 0.0)
    {
        spawnGrain();
        const double interval = sr / std::max (0.5f, tick.grainDensity);
        grainTimer += interval * (1.0 + 0.25 * tick.grainSpray * rng.bi());
    }

    const SampleData* s = res.sample;
    const Wavetable* wt = res.wavetable;
    const float overlap = tick.grainDensity * tick.grainSize * 0.001f;
    const float norm = 1.0f / std::sqrt (std::max (1.0f, overlap));
    for (auto& g : grains)
    {
        if (! g.active) continue;
        const float w = grainWindow (tick.grainWindow, (float) g.age / (float) g.length) * norm;
        float v = 0.0f;
        if (g.frame < 0 && s != nullptr)
        {
            const int lv = g.rate > 1.6 ? std::min ((int) s->mips.size() - 1, (int) std::floor (std::log2 (g.rate * 0.8))) : 0;
            v = s->numChannels > 1 ? 0.5f * (readSample (*s, lv, 0, g.pos) + readSample (*s, lv, 1, g.pos)) : readSample (*s, lv, 0, g.pos);
            g.pos += g.rate;
            if (g.pos >= s->length - 2) g.age = g.length;
        }
        else if (g.frame >= 0 && wt != nullptr && g.frame < wt->numFrames)
        {
            const int lv = Wavetable::levelFor (g.phaseInc);
            v = Wavetable::read (wt->table (g.frame, lv), g.phase);
            g.phase = wrap01 (g.phase + g.phaseInc);
        }
        l += v * w * g.panL;
        r += v * w * g.panR;
        if (++g.age >= g.length) g.active = false;
    }
}

void OscVoice::renderSpectral (float fmMul, float& l, float& r) noexcept
{
    const auto& sine = sineTable();
    const int np = activePartials;
    for (int u = 0; u < numUnison; ++u)
    {
        const float incU = baseIncCur * detuneRatio[(size_t) u] * fmMul;
        auto& ph = pphase[(size_t) u];
        float acc = 0.0f;
        for (int k = 0; k < np; ++k)
        {
            float p = ph[(size_t) k] + incU * ratio[(size_t) k];
            p -= std::floor (p);
            ph[(size_t) k] = p;
            acc += ampCur[(size_t) k] * sine (p);
        }
        l += acc * gainL[(size_t) u];
        r += acc * gainR[(size_t) u];
    }
    for (int k = 0; k < np; ++k)
        ampCur[(size_t) k] = std::max (0.0f, ampCur[(size_t) k] + ampStep[(size_t) k]);
}

void OscVoice::render (float xmod, float syncFrac, float& outL, float& outR) noexcept
{
    wrapFrac = -1.0f;
    if (! active)
    {
        outL = outR = 0.0f;
        lastMono = 0.0f;
        return;
    }

    baseIncCur += baseIncStep;
    float fmMul = 1.0f, pm = 0.0f;
    const auto xm = (XModMode) tick.xmodMode;
    if (xm == XModMode::FM) fmMul = 1.0f + xmod * tick.xmodAmount * 6.0f;
    else if (xm == XModMode::PM) pm = xmod * tick.xmodAmount * 0.95f;
    else if (xm == XModMode::HardSync && syncFrac >= 0.0f)
    {
        for (int u = 0; u < numUnison; ++u)
            phase[(size_t) u] = wrap01 (baseIncCur * detuneRatio[(size_t) u] * syncFrac);
    }

    float l = 0.0f, r = 0.0f;
    switch ((OscMode) tick.mode)
    {
        case OscMode::Wavetable:
            for (int u = 0; u < numUnison; ++u)
            {
                const float y = renderWavetable (u, baseIncCur * detuneRatio[(size_t) u] * fmMul, pm);
                l += y * gainL[(size_t) u];
                r += y * gainR[(size_t) u];
            }
            break;
        case OscMode::Analog:
        {
            const auto setup = analogSetup();
            for (int u = 0; u < numUnison; ++u)
            {
                const float y = renderAnalog (u, baseIncCur * detuneRatio[(size_t) u] * fmMul, pm, setup);
                l += y * gainL[(size_t) u];
                r += y * gainR[(size_t) u];
            }
            break;
        }
        case OscMode::Sample:
        case OscMode::Multisample: renderSampleVoices (fmMul, l, r); break;
        case OscMode::Granular: renderGranular (l, r); break;
        case OscMode::Spectral: renderSpectral (fmMul, l, r); break;
        case OscMode::FM:
        {
            // MSFA output is Dexed-scaled (a single full-level carrier peaks at -18 dBFS, headroom for 16 voices);
            // +12 dB brings FM to the level of the other oscillator modes. Level and pan are applied below.
            const float y = fmStarted ? fmRenderer.render() * 4.0f : 0.0f;
            l = r = y * 0.70710678f;
            break;
        }
    }

    if (tick.fold > 1.0e-3f)
    {
        const float g = 1.0f + 7.0f * tick.fold;
        l = foldTri (l * g);
        r = foldTri (r * g);
    }

    if (xm == XModMode::RM)
    {
        const float a = tick.xmodAmount;
        l = l * (1.0f - a) + l * xmod * a;
        r = r * (1.0f - a) + r * xmod * a;
    }
    else if (xm == XModMode::AM)
    {
        const float g = 1.0f - tick.xmodAmount + tick.xmodAmount * (0.5f + 0.5f * xmod);
        l *= g; r *= g;
    }

    lastMono = 0.5f * (l + r);
    const float lv = levelRamp.next();
    const float pn = panRamp.next();
    outL = l * lv * (pn > 0.0f ? 1.0f - pn : 1.0f);
    outR = r * lv * (pn < 0.0f ? 1.0f + pn : 1.0f);
}

// =================================================================================
float SubOsc::render (int shape, float freq) noexcept
{
    const float dt = std::min (0.45f, freq / (float) sr);
    const float y = [&]
    {
        switch (shape)
        {
            // triangle: polyBLAMP at both corners (slope change 8 per cycle; measured 3.9 dB less aliasing at G6)
            case 1: return 1.0f - 4.0f * std::abs (phase - 0.5f) + 4.0f * dt * (polyBlamp (phase, dt) - polyBlamp (wrap01 (phase + 0.5f), dt));
            case 2: return 2.0f * phase - 1.0f - polyBlep4 (phase, dt);
            case 3:
            {
                float v = phase < 0.5f ? 1.0f : -1.0f;
                v += polyBlep4 (phase, dt);
                v -= polyBlep4 (wrap01 (phase + 0.5f), dt);
                return v;
            }
            default: return sineTable() (phase);
        }
    }();
    phase += dt;
    wrapFrac = -1.0f;
    if (phase >= 1.0f)
    {
        phase -= 1.0f;
        wrapFrac = dt > 0.0f ? phase / dt : 0.0f;
    }
    return y;
}

// =================================================================================
void NoiseOsc::prepare (double s)
{
    sr = s;
    tiltLpL.setCutoff (1000.0f, sr);
    tiltLpR.setCutoff (1000.0f, sr);
}

void NoiseOsc::noteOn (uint32_t seed)
{
    rngL.seed (hash32 (seed));
    rngR.seed (hash32 (seed ^ 0xA5A5A5A5u));
}

float NoiseOsc::pink (float* b, float white) noexcept
{
    // Paul Kellet's refined pink-noise filter
    b[0] = 0.99886f * b[0] + white * 0.0555179f;
    b[1] = 0.99332f * b[1] + white * 0.0750759f;
    b[2] = 0.96900f * b[2] + white * 0.1538520f;
    b[3] = 0.86650f * b[3] + white * 0.3104856f;
    b[4] = 0.55000f * b[4] + white * 0.5329522f;
    b[5] = -0.7616f * b[5] - white * 0.0168980f;
    const float pinkOut = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * 0.5362f;
    b[6] = white * 0.115926f;
    return pinkOut * 0.11f;
}

void NoiseOsc::render (int type, float color, float width, float rateHz, float& l, float& r) noexcept
{
    const float wl = rngL.bi();
    const float wr = lerp (wl, rngR.bi(), width);
    float yl = 0.0f, yr = 0.0f;
    switch (type)
    {
        case 1: yl = pink (pinkL, wl); yr = pink (pinkR, wr); break;
        case 2:
            brownL = (brownL + 0.02f * wl) / 1.02f; brownR = (brownR + 0.02f * wr) / 1.02f;
            yl = brownL * 3.5f; yr = brownR * 3.5f;
            break;
        case 3:
            holdPhase += rateHz / (float) sr;
            if (holdPhase >= 1.0f)
            {
                holdPhase -= std::floor (holdPhase);
                holdL = std::round (wl * 4.0f) / 4.0f;
                holdR = lerp (holdL, std::round (wr * 4.0f) / 4.0f, width);
            }
            yl = holdL; yr = holdR;
            break;
        case 4:
        {
            const float prob = juce::jlimit (0.00005f, 0.05f, rateHz / (float) sr * 0.05f);
            if (rngL.uni() < prob) crackle = rngL.bi();
            crackle *= 0.85f;
            yl = crackle + wl * 0.02f;
            yr = crackle * (1.0f - 0.3f * width) + wr * 0.02f;
            break;
        }
        default: yl = wl * 0.6f; yr = wr * 0.6f; break;
    }

    if (std::abs (color) > 1.0e-3f)
    {
        if (color != lastColor)
        {
            const float cutoff = color < 0.0f ? 150.0f * std::exp2 ((1.0f + color) * 7.0f) : 800.0f;
            tiltLpL.setCutoff (cutoff, sr);
            tiltLpR.setCutoff (cutoff, sr);
            lastColor = color;
        }
        const float ll = tiltLpL.process (yl), lr = tiltLpR.process (yr);
        if (color < 0.0f) { yl = ll; yr = lr; }
        else { yl = yl - ll * color; yr = yr - lr * color; }
    }
    l = yl;
    r = yr;
}

} // namespace vsx
