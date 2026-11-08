#include "D50Engine.h"

namespace dc
{
namespace
{
    // Structure No. 1..7 (value 0..6): which partials are synth (S) or PCM (P) and whether
    // partial 2 is replaced by the ring-modulated product of both waveform generators.
    struct StructureInfo { bool p1Synth, p2Synth, ring; };
    constexpr StructureInfo kStructures[7] = {
        { true,  true,  false }, // 1: S + S
        { true,  true,  true  }, // 2: S + R(S x S)
        { false, true,  false }, // 3: P + S
        { false, true,  true  }, // 4: P + R(P x S)
        { true,  false, true  }, // 5: S + R(S x P)
        { false, false, false }, // 6: P + P
        { false, false, true  }, // 7: P + R(P x P)
    };

    // Chorus Type 1..8 (value 0..7): original settings, not measured from hardware.
    struct ChorusShape { float baseMs, depthMs, feedback, tremolo; };
    constexpr ChorusShape kChorusShapes[8] = {
        { 7.0f, 3.0f, 0.0f, 0.0f },  // chorus 1
        { 12.0f, 5.0f, 0.0f, 0.0f }, // chorus 2
        { 2.0f, 2.0f, 0.5f, 0.0f },  // flanger 1
        { 1.5f, 3.0f, 0.7f, 0.0f },  // flanger 2
        { 8.0f, 4.0f, 0.4f, 0.0f },  // feedback chorus
        { 5.0f, 0.0f, 0.0f, 1.0f },  // tremolo
        { 7.0f, 3.0f, 0.0f, 0.6f },  // chorus + tremolo
        { 10.0f, 1.0f, 0.0f, 0.0f }, // dimension-like
    };

    void reverbSettings (int type, float& size, float& decay, float& damp, float& pre) noexcept
    {
        // Reverb Type 1..32 (value 0..31). Our own spread of rooms/halls/plates; not Roland's programs.
        const int family = (type / 4) % 4;   // room, hall, plate, special
        const int variant = type % 4;
        const bool card = type >= 16;        // types 17..32 lived on a card on the original
        size  = 0.25f + 0.25f * float (family) * 0.7f + 0.12f * float (variant);
        decay = 0.5f + 0.7f * float (variant) + 0.8f * float (family) + (card ? 0.8f : 0.0f);
        damp  = family == 2 ? 9000.0f : 5000.0f - 600.0f * float (variant);
        pre   = family == 3 ? 40.0f : 5.0f + 6.0f * float (variant);
    }

    // Bias Point/Dir 0..127: 0-63 = "<A1 .. <C7" (keys below the point), 64-127 = ">A1 .. >C7" (above).
    inline float octavesBeyondBias (int biasPoint, float note) noexcept
    {
        const bool below = biasPoint < 64;
        const float point = 33.0f + float (biasPoint & 63); // A1 (MIDI 33) .. C7 (MIDI 96): 64 steps
        const float d = below ? point - note : note - point;
        return d > 0.0f ? d / 12.0f : 0.0f;
    }

    // Keyfollow 0..4 for envelope times / depth: 0 = none, 4 = halves per octave above C4 (estimate).
    inline float keyScale (int kf0to4, float note) noexcept
    {
        return std::exp2 (-(note - 60.0f) / 12.0f * float (kf0to4) * 0.25f);
    }

    inline float lfoSelect (int sel, const float* lv) noexcept
    {
        // (+1, -1, +2, -2, +3, -3)
        const int idx = std::min (2, sel / 2);
        return (sel % 2 == 0 ? 1.0f : -1.0f) * lv[idx];
    }

    void decodePartial (const uint8_t* b, D50PartialParams& p) noexcept
    {
        using namespace d50::partial;
        auto v = [b] (int o) { return (int) std::min<uint8_t> (b[o], d50::partialParam (o).maxValue); };
        p.waveform = v (Waveform);
        p.pcmWave = v (PcmWave);
        p.coarseSemis = float (v (Coarse) - 36); // 36 = C4 (doc example 10.1)
        p.fineCents = float (v (Fine) - 50);
        p.keyfollow = d50::kPitchKeyfollow[v (Keyfollow)];
        p.lfoMode = v (LfoMode);
        p.penvMode = v (PEnvMode);
        p.bendMode = v (BendMode);
        p.pulseWidth = float (v (PulseWidth)) / 100.0f;
        p.pwLfoSelect = v (PwLfoSelect);
        p.pwLfoDepth = float (v (PwLfoDepth)) / 100.0f;
        p.cutoff = float (v (TvfCutoff));
        p.resonance = float (v (TvfResonance));
        p.tvfKeyfollow = d50::kTvfKeyfollow[v (TvfKeyfollow)];
        p.tvfEnvDepth = float (v (TvfEnvDepth)) / 100.0f;
        p.tvfVelRange = float (v (TvfEnvVelocity)) / 100.0f;
        for (int i = 0; i < 5; ++i) p.tvfTimes[i] = timeFromValue (float (v (TvfT1 + i)));
        for (int i = 0; i < 3; ++i) p.tvfLevels[i] = float (v (TvfL1 + i)) / 100.0f;
        p.tvfSustain = float (v (TvfSustain)) / 100.0f;
        p.tvfEnd = v (TvfEnd) ? 1.0f : 0.0f;
        p.tvfLfoSelect = v (TvfLfoSelect);
        p.tvfLfoDepth = float (v (TvfLfoDepth)) / 100.0f;
        p.tvaLevel = float (v (TvaLevel)) / 100.0f;
        p.tvaVelRange = float (v (TvaVelocity) - 50) / 50.0f;
        for (int i = 0; i < 5; ++i) p.tvaTimes[i] = timeFromValue (float (v (TvaT1 + i)));
        for (int i = 0; i < 3; ++i) p.tvaLevels[i] = float (v (TvaL1 + i)) / 100.0f;
        p.tvaSustain = float (v (TvaSustain)) / 100.0f;
        p.tvaEnd = v (TvaEnd) ? 1.0f : 0.0f;
        p.tvaLfoSelect = v (TvaLfoSelect);
        p.tvaLfoDepth = float (v (TvaLfoDepth)) / 100.0f;
        p.pwVelRange = float (v (PwVelocity) - 7) / 7.0f;
        p.pwAtRange = float (v (PwAftertouch) - 7) / 7.0f;
        p.tvfBiasPoint = v (TvfBiasPoint);
        p.tvfBiasLevel = float (v (TvfBiasLevel) - 7) / 7.0f;
        p.tvfDepthKf = v (TvfEnvDepthKf);
        p.tvfTimeKf = v (TvfEnvTimeKf);
        p.tvfAtRange = float (v (TvfAftertouch) - 7) / 7.0f;
        p.tvaBiasPoint = v (TvaBiasPoint);
        p.tvaBiasLevelDb = float (v (TvaBiasLevel) - 12);
        p.tvaVelFollow = v (TvaVelocityFollow);
        p.tvaTimeKf = v (TvaTimeKf);
        p.tvaAtRange = float (v (TvaAftertouch) - 7) / 7.0f;
    }

    void decodeTone (const uint8_t* p1, const uint8_t* p2, const uint8_t* c, D50ToneParams& t) noexcept
    {
        using namespace d50::common;
        decodePartial (p1, t.partial[0]);
        decodePartial (p2, t.partial[1]);
        auto v = [c] (int o) { return (int) std::min<uint8_t> (c[o], d50::commonParam (o).maxValue); };
        t.structure = v (Structure);
        const auto& s = kStructures[t.structure];
        t.partial[0].synth = s.p1Synth;
        t.partial[1].synth = s.p2Synth;
        t.penvVelRange = float (v (PEnvVelocity)) * 0.5f;
        t.penvTimeKf = v (PEnvTimeKf);
        for (int i = 0; i < 4; ++i) t.penvTimes[i] = timeFromValue (float (v (PEnvT1 + i)) * 2.0f);
        for (int i = 0; i < 5; ++i) t.penvLevels[i] = float (v (PEnvL0 + i) - 50) / 50.0f * 12.0f;
        t.pModLfoDepth = float (v (PModLfoDepth)) / 100.0f;
        t.pModLever = float (v (PModLever)) / 100.0f;
        t.pModAftertouch = float (v (PModAftertouch)) / 100.0f;
        for (int k = 0; k < 3; ++k)
        {
            t.lfoWave[k] = v (Lfo1Wave + 4 * k);
            t.lfoRate[k] = lfoRateHz (float (v (Lfo1Rate + 4 * k)));
            t.lfoDelay[k] = v (Lfo1Delay + 4 * k) == 0 ? 0.0f : timeFromValue (float (v (Lfo1Delay + 4 * k)));
            t.lfoSync[k] = v (Lfo1Sync + 4 * k);
        }
        t.lowEqHz = d50::kLowEqFreqHz[v (LowEqFreq)];
        t.lowEqDb = float (v (LowEqGain) - 12);
        t.highEqHz = d50::kHighEqFreqHz[v (HighEqFreq)];
        t.highEqQ = d50::kHighEqQ[v (HighEqQ)];
        t.highEqDb = float (v (HighEqGain) - 12);
        t.chorusType = v (ChorusType);
        t.chorusRate = float (v (ChorusRate));
        t.chorusDepth = float (v (ChorusDepth)) / 100.0f;
        t.chorusBalance = float (v (ChorusBalance)) / 100.0f;
        // Partial Mute (00, 01, 10, 11): left digit = partial 1, right digit = partial 2.
        const int mute = v (PartialMute);
        t.partialOn[0] = (mute & 2) != 0;
        t.partialOn[1] = (mute & 1) != 0;
        t.partialBalance = float (v (PartialBalance));
    }
} // namespace

D50Params D50Params::decode (const uint8_t* raw) noexcept
{
    D50Params p;
    decodeTone (raw + d50::kUpperPartial1, raw + d50::kUpperPartial2, raw + d50::kUpperCommon, p.upper);
    decodeTone (raw + d50::kLowerPartial1, raw + d50::kLowerPartial2, raw + d50::kLowerCommon, p.lower);
    const uint8_t* b = raw + d50::kPatchFactors;
    using namespace d50::patch;
    auto v = [b] (int o) { return (int) std::min<uint8_t> (b[o], d50::patchParam (o).maxValue); };
    p.patch.keyMode = v (KeyMode);
    p.patch.splitNote = 36 + v (SplitPoint); // 0..60 = C2..C7 with C4 = 60
    p.patch.upperShift = v (UpperKeyShift) - 24;
    p.patch.lowerShift = v (LowerKeyShift) - 24;
    p.patch.upperFine = float (v (UpperFineTune) - 50);
    p.patch.lowerFine = float (v (LowerFineTune) - 50);
    p.patch.benderRange = v (BenderRange);
    p.patch.reverbType = v (ReverbType);
    p.patch.reverbBalance = float (v (ReverbBalance)) / 100.0f;
    p.patch.totalVolume = float (v (TotalVolume)) / 100.0f;
    p.patch.toneBalance = float (v (ToneBalance));
    p.patch.portamentoMode = v (PortamentoMode);
    p.patch.holdMode = v (HoldMode);
    p.patch.portamentoSeconds = v (PortamentoTime) == 0 ? 0.0f : timeFromValue (float (v (PortamentoTime)));
    p.patch.aftertouchBend = v (AftertouchBendRange) - 12;
    return p;
}

// ---------------------------------------------------------------------------
PcmBank::PcmBank()
{
    for (int n = 0; n < 100; ++n)
    {
        Rng rng;
        rng.state = 0x9E3779B9u * uint32_t (n + 1);
        auto uni = [&rng] { return 0.5f * (rng.next() + 1.0f); };
        Wave& w = waves[n];

        if (n < 50)
        {
            // One-shot attack transient: resonant noise burst + decaying, slightly inharmonic partials.
            const int len = int (32000.0f * (0.06f + 0.34f * uni()));
            w.data.assign ((size_t) len, 0.0f);
            w.loop = false;
            const float centre = 300.0f * std::pow (2.0f, float (n % 10) * 0.55f);
            Svf bp;
            bp.set (centre, 1.5f + 4.0f * uni(), 32000.0f);
            const float noiseDecay = 20.0f + 120.0f * uni();
            const int partials = 3 + int (uni() * 8.0f);
            float amps[12], ratios[12], decays[12];
            const float inharm = (n % 3 == 0) ? 0.02f * uni() : 0.0f;
            for (int h = 0; h < partials; ++h)
            {
                const float hn = float (h + 1);
                ratios[h] = hn * (1.0f + inharm * hn * hn);
                amps[h] = uni() / hn;
                decays[h] = 3.0f + 25.0f * uni() * hn;
            }
            const float tone = uni();
            double phase[12] {};
            float peak = 1.0e-6f;
            for (int i = 0; i < len; ++i)
            {
                const float t = float (i) / 32000.0f;
                float l, b, h;
                bp.process (rng.next(), l, b, h);
                float s = b * std::exp (-noiseDecay * t) * (1.0f - tone);
                for (int k = 0; k < partials; ++k)
                {
                    phase[k] += double (w.rootHz * ratios[k]) / 32000.0;
                    s += tone * amps[k] * std::exp (-decays[k] * t) * float (std::sin (6.283185307179586 * phase[k]));
                }
                const float fadeOut = std::min (1.0f, float (len - i) / 256.0f);
                w.data[(size_t) i] = s * fadeOut;
                peak = std::max (peak, std::abs (s));
            }
            for (auto& x : w.data) x *= 0.9f / peak;
        }
        else
        {
            // Single-cycle loop with a seeded harmonic spectrum.
            const int len = 256;
            w.data.assign ((size_t) len, 0.0f);
            w.loop = true;
            w.rootHz = 32000.0f / float (len);
            const float slope = 0.5f + 1.5f * uni();
            const int harmonics = 6 + int (uni() * 40.0f);
            float peak = 1.0e-6f;
            std::vector<float> amp ((size_t) harmonics), ph ((size_t) harmonics);
            for (int h = 0; h < harmonics; ++h)
            {
                const float u = uni();
                amp[(size_t) h] = u * u / std::pow (float (h + 1), slope);
                ph[(size_t) h] = kTwoPi * uni();
            }
            for (int i = 0; i < len; ++i)
            {
                float s = 0;
                for (int h = 0; h < harmonics; ++h)
                    s += amp[(size_t) h] * std::sin (kTwoPi * float ((h + 1) * i) / float (len) + ph[(size_t) h]);
                w.data[(size_t) i] = s;
                peak = std::max (peak, std::abs (s));
            }
            for (auto& x : w.data) x *= 0.9f / peak;
        }
    }
}

const PcmBank& PcmBank::get()
{
    static const PcmBank bank;
    return bank;
}

// ---------------------------------------------------------------------------
void D50Engine::prepare (double sr, int maxBlock)
{
    sampleRate = float (sr);
    const size_t n = (size_t) std::max (64, maxBlock);
    for (auto* b : { &upperBuf, &lowerBuf, &tmpL, &tmpR, &revInL, &revInR, &revOutL, &revOutR })
        b->assign (n, 0.0f);
    for (auto& c : chorus) c.prepare (sampleRate);
    reverb.prepare (sampleRate);
    dcL.prepare (sampleRate);
    dcR.prepare (sampleRate);
    (void) PcmBank::get(); // build substitute waves outside the audio callback
    reset();
}

void D50Engine::reset() noexcept
{
    allSoundOff();
    for (auto& c : chorus) c.reset();
    for (auto& e : eqLow) e.reset();
    for (auto& e : eqHigh) e.reset();
    reverb.reset();
    dcL.reset(); dcR.reset();
}

int D50Engine::activeVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voices) c += v.isActive() ? 1 : 0;
    return c;
}

void D50Engine::configureEnvelopes (PartialVoice& pv, const D50PartialParams& pp, float note, float velocity) noexcept
{
    const float tvaScale = keyScale (pp.tvaTimeKf, note);
    const float tvfScale = keyScale (pp.tvfTimeKf, note);
    auto& a = pv.tva;
    a.sampleRate = sampleRate;
    a.numSegments = 4;
    for (int i = 0; i < 4; ++i) a.times[i] = pp.tvaTimes[i] * tvaScale;
    // Velocity Follow 0..4: harder playing shortens Time 1 (estimate: up to 75 % at 4)
    a.times[0] *= 1.0f - 0.75f * float (pp.tvaVelFollow) / 4.0f * velocity;
    a.levels[0] = pp.tvaLevels[0]; a.levels[1] = pp.tvaLevels[1]; a.levels[2] = pp.tvaLevels[2]; a.levels[3] = pp.tvaSustain;
    a.releaseTime = pp.tvaTimes[4] * tvaScale;
    a.releaseLevel = 0.0f; // End Level 100 is approximated as 0 so released notes always finish
    a.startLevel = 0.0f;

    auto& f = pv.tvf;
    f.sampleRate = sampleRate;
    f.numSegments = 4;
    for (int i = 0; i < 4; ++i) f.times[i] = pp.tvfTimes[i] * tvfScale;
    f.levels[0] = pp.tvfLevels[0]; f.levels[1] = pp.tvfLevels[1]; f.levels[2] = pp.tvfLevels[2]; f.levels[3] = pp.tvfSustain;
    f.releaseTime = pp.tvfTimes[4] * tvfScale;
    f.releaseLevel = pp.tvfEnd;
    f.startLevel = 0.0f;

    pv.tvaBiasGain = dbToGain (pp.tvaBiasLevelDb * octavesBeyondBias (pp.tvaBiasPoint, note));
    pv.tvfBiasOct = pp.tvfBiasLevel * octavesBeyondBias (pp.tvfBiasPoint, note);
    pv.tvfDepthScale = std::min (2.0f, keyScale (pp.tvfDepthKf, note));
}

void D50Engine::startTone (ToneVoice& tv, const D50ToneParams& tp, float toneNote, float velocity) noexcept
{
    tv.active = true;
    tv.elapsed = 0.0f;
    tv.glideOffset = tv.glideRate = 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        auto& pv = tv.p[i];
        pv.phase = 0.0f;
        pv.pcmPos = 0.0f;
        pv.f1.reset();
        pv.f2.reset();
        pv.lastOsc = 0.0f;
        configureEnvelopes (pv, tp.partial[i], toneNote, velocity);
        pv.tva.noteOn();
        pv.tvf.noteOn();
    }
    auto& pe = tv.penv;
    pe.sampleRate = sampleRate;
    pe.numSegments = 3;
    const float penvScale = keyScale (tp.penvTimeKf, toneNote);
    pe.startLevel = tp.penvLevels[0];
    pe.times[0] = tp.penvTimes[0] * penvScale; pe.levels[0] = tp.penvLevels[1];
    pe.times[1] = tp.penvTimes[1] * penvScale; pe.levels[1] = tp.penvLevels[2];
    pe.times[2] = tp.penvTimes[2] * penvScale; pe.levels[2] = tp.penvLevels[3];
    pe.releaseTime = tp.penvTimes[3] * penvScale;
    pe.releaseLevel = tp.penvLevels[4];
    pe.noteOn();
    const int t = (&tp == &params.upper) ? 0 : 1;
    for (int k = 0; k < 3; ++k)
    {
        tv.lfo[k].wave = tp.lfoWave[k];
        tv.lfo[k].phase = tp.lfoSync[k] == 0 ? freeLfoPhase[t][k] : 0.0f;
    }
}

void D50Engine::noteOn (int note, float velocity) noexcept
{
    const auto& pt = params.patch;
    bool useUpper = true, useLower = false;
    switch (pt.keyMode)
    {
        case 0: case 4: useUpper = true; useLower = false; break;             // Whole, Whole-S
        case 1: case 5: useUpper = true; useLower = true; break;              // Dual, Dual-S
        default: useUpper = note >= pt.splitNote; useLower = !useUpper; break; // Split / Separate variants
    }
    // Solo ("-S") key modes: one note at a time for the solo part(s).
    const bool soloUpper = pt.keyMode == 4 || pt.keyMode == 5 || pt.keyMode == 6 || pt.keyMode == 8;
    const bool soloLower = pt.keyMode == 5 || pt.keyMode == 7 || pt.keyMode == 8;
    for (auto& v : voices)
    {
        if (! v.isActive() || ! v.held) continue;
        if ((useUpper && soloUpper && v.upper.active) || (useLower && soloLower && v.lower.active))
            releaseVoice (v);
    }

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
    if (target == nullptr)
    {
        uint64_t best = UINT64_MAX;
        for (auto& v : voices) // prefer the oldest released voice
            if (! v.held && ! v.sustained && v.age < best) { best = v.age; target = &v; }
        if (target == nullptr)
            for (auto& v : voices)
                if (v.age < best) { best = v.age; target = &v; }
    }
    auto& v = *target;
    v.note = note;
    v.velocity = clampf (velocity, 0.0f, 1.0f);
    v.held = true;
    v.sustained = false;
    v.age = ++counter;
    v.upper.active = v.lower.active = false;
    if (useUpper) startTone (v.upper, params.upper, float (note + pt.upperShift), v.velocity);
    if (useLower) startTone (v.lower, params.lower, float (note + pt.lowerShift), v.velocity);

    // Portamento (CC65 switch, Patch "Portamento Mode" U / L / UL, "Portamento Time")
    if (portamentoOn && lastNote >= 0 && pt.portamentoSeconds > 0.0f && lastNote != note)
    {
        const float offset = float (lastNote - note);
        const float rate = std::abs (offset) / pt.portamentoSeconds;
        if (useUpper && pt.portamentoMode != 1) { v.upper.glideOffset = offset; v.upper.glideRate = rate; }
        if (useLower && pt.portamentoMode != 0) { v.lower.glideOffset = offset; v.lower.glideRate = rate; }
    }
    lastNote = note;
}

void D50Engine::releaseTone (ToneVoice& tv) noexcept
{
    if (! tv.active) return;
    for (auto& pv : tv.p) { pv.tva.noteOff(); pv.tvf.noteOff(); }
    tv.penv.noteOff();
}

int D50Engine::activeToneCount (bool upper) const noexcept
{
    int c = 0;
    for (auto& v : voices) c += (upper ? v.upper.active : v.lower.active) ? 1 : 0;
    return c;
}

void D50Engine::releaseVoice (Voice& v) noexcept
{
    v.held = false;
    v.sustained = false;
    releaseTone (v.upper);
    releaseTone (v.lower);
}

void D50Engine::noteOff (int note) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.held && v.note == note)
        {
            if (sustainDown)
            {
                // Hold Mode (U, L, UL): the pedal only sustains the selected tone(s).
                v.held = false;
                v.sustained = true;
                const int hold = params.patch.holdMode;
                if (hold == 0) releaseTone (v.lower);
                else if (hold == 1) releaseTone (v.upper);
            }
            else releaseVoice (v);
        }
}

void D50Engine::setSustain (bool down) noexcept
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.sustained) releaseVoice (v);
}

void D50Engine::allNotesOff() noexcept
{
    sustainDown = false;
    for (auto& v : voices)
        if (v.isActive()) releaseVoice (v);
}

void D50Engine::allSoundOff() noexcept
{
    sustainDown = false;
    for (auto& v : voices)
    {
        v.upper.active = v.lower.active = false;
        v.held = v.sustained = false;
        v.note = -1;
        for (auto* tv : { &v.upper, &v.lower })
            for (auto& pv : tv->p) { pv.tva.kill(); pv.tvf.kill(); }
    }
}

void D50Engine::renderTone (ToneVoice& tv, const D50ToneParams& tp, float note, float velocity, float* out, int n) noexcept
{
    const auto& bank = PcmBank::get();
    const bool ring = kStructures[tp.structure].ring;
    const float balance[2] = { std::min (1.0f, (100.0f - tp.partialBalance) / 50.0f),
                               std::min (1.0f, tp.partialBalance / 50.0f) };
    const float bendSemis = bend * float (params.patch.benderRange) + aftertouch * float (params.patch.aftertouchBend);

    for (int start = 0; start < n; start += kControlRate)
    {
        const int cb = std::min (kControlRate, n - start);
        const float dt = float (cb) / sampleRate;
        tv.elapsed += dt;

        float lv[3];
        for (int k = 0; k < 3; ++k)
        {
            const float fade = tp.lfoDelay[k] > 0.0f ? std::min (1.0f, tv.elapsed / tp.lfoDelay[k]) : 1.0f;
            lv[k] = tv.lfo[k].advance (tp.lfoRate[k], dt) * fade;
        }
        const float penv = tv.penv.advance (cb) * (1.0f - tp.penvVelRange * (1.0f - velocity));
        const float leverMod = lv[0] * (modWheel * tp.pModLever + aftertouch * tp.pModAftertouch);
        const float lfoMod = lv[0] * tp.pModLfoDepth;
        if (tv.glideRate > 0.0f)
        {
            const float step = tv.glideRate * dt;
            tv.glideOffset = std::abs (tv.glideOffset) <= step ? 0.0f : tv.glideOffset - std::copysign (step, tv.glideOffset);
            if (std::abs (tv.glideOffset) < 1.0e-6f) tv.glideRate = 0.0f;
        }

        float wg[2][kControlRate] {};
        float amp0[2] {}, amp1[2] {};
        bool partialLive[2] {};

        for (int i = 0; i < 2; ++i)
        {
            auto& pv = tv.p[i];
            const auto& pp = tp.partial[i];
            const float tvfEnv = pv.tvf.advance (cb);
            const float prevAmp = pv.tva.value;
            const float tvaEnv = pv.tva.advance (cb);
            partialLive[i] = tp.partialOn[i] && (pv.tva.isActive() || prevAmp > 0.0f);
            if (! partialLive[i])
                continue;

            float pitch = 60.0f + (note + tv.glideOffset - 60.0f) * pp.keyfollow + pp.coarseSemis + pp.fineCents * 0.01f;
            if (pp.penvMode == 1) pitch += penv; else if (pp.penvMode == 2) pitch -= penv;
            if (pp.lfoMode == 1) pitch += lfoMod + leverMod;
            else if (pp.lfoMode == 2) pitch -= lfoMod + leverMod;
            else if (pp.lfoMode == 3) pitch += leverMod;
            if (pp.bendMode == 2) pitch += bendSemis;
            else if (pp.bendMode == 1) pitch += bendSemis * pp.keyfollow;
            const float hz = midiToHz (clampf (pitch, -24.0f, 150.0f));

            // Oscillator for this control block
            if (pp.synth)
            {
                const float dtp = std::min (0.45f, hz / sampleRate);
                if (pp.waveform == 0)
                {
                    const float pw = clampf (pp.pulseWidth + lfoSelect (pp.pwLfoSelect, lv) * pp.pwLfoDepth * 0.5f
                                                 + pp.pwVelRange * velocity * 0.5f + pp.pwAtRange * aftertouch * 0.5f, 0.0f, 1.0f);
                    const float duty = 0.5f - 0.47f * pw;
                    for (int s = 0; s < cb; ++s)
                    {
                        float x = pv.phase < duty ? 1.0f : -1.0f;
                        x += polyBlep (pv.phase, dtp);
                        float p2 = pv.phase + 1.0f - duty;
                        p2 -= std::floor (p2);
                        x -= polyBlep (p2, dtp);
                        x -= 2.0f * duty - 1.0f; // remove the duty-cycle DC component
                        wg[i][s] = x;
                        pv.phase += dtp;
                        if (pv.phase >= 1.0f) pv.phase -= 1.0f;
                    }
                }
                else
                {
                    for (int s = 0; s < cb; ++s)
                    {
                        wg[i][s] = 2.0f * pv.phase - 1.0f - polyBlep (pv.phase, dtp);
                        pv.phase += dtp;
                        if (pv.phase >= 1.0f) pv.phase -= 1.0f;
                    }
                }
            }
            else
            {
                const auto& w = bank.waves[pp.pcmWave];
                const float step = (hz / w.rootHz) * (w.rate / sampleRate);
                const int len = (int) w.data.size();
                for (int s = 0; s < cb; ++s)
                {
                    float x = 0.0f;
                    if (w.loop)
                    {
                        const int i0 = int (pv.pcmPos);
                        const float f = pv.pcmPos - float (i0);
                        x = w.data[(size_t) i0] + f * (w.data[(size_t) ((i0 + 1) % len)] - w.data[(size_t) i0]);
                        pv.pcmPos += step;
                        while (pv.pcmPos >= float (len)) pv.pcmPos -= float (len);
                    }
                    else if (pv.pcmPos < float (len - 1))
                    {
                        const int i0 = int (pv.pcmPos);
                        const float f = pv.pcmPos - float (i0);
                        x = w.data[(size_t) i0] + f * (w.data[(size_t) i0 + 1] - w.data[(size_t) i0]);
                        pv.pcmPos += step;
                    }
                    wg[i][s] = x;
                }
            }

            // TVF (synth partials only; PCM partials bypass the filter as on the original)
            if (pp.synth)
            {
                const float depth = pp.tvfEnvDepth * (1.0f - pp.tvfVelRange * (1.0f - velocity)) * pv.tvfDepthScale;
                const float oct = pp.cutoff * 0.1f + pp.tvfKeyfollow * (note - 60.0f) / 12.0f
                                + depth * 7.0f * tvfEnv + lfoSelect (pp.tvfLfoSelect, lv) * pp.tvfLfoDepth * 2.0f
                                + pv.tvfBiasOct + pp.tvfAtRange * aftertouch * 2.0f;
                const float cutoffHz = 20.0f * std::exp2 (clampf (oct, 0.0f, 10.5f));
                const float q = 0.5f * std::pow (16.0f, pp.resonance / 30.0f);
                pv.f1.set (cutoffHz, 0.54f, sampleRate);
                pv.f2.set (cutoffHz, q, sampleRate);
            }

            const float velGain = pp.tvaVelRange >= 0.0f ? 1.0f - pp.tvaVelRange * (1.0f - velocity)
                                                         : 1.0f + pp.tvaVelRange * velocity;
            const float trem = 1.0f + lfoSelect (pp.tvaLfoSelect, lv) * pp.tvaLfoDepth * 0.5f;
            const float atGain = std::max (0.0f, 1.0f + pp.tvaAtRange * aftertouch * 0.5f);
            const float g = pp.tvaLevel * pp.tvaLevel * std::max (0.0f, velGain) * trem * balance[i] * pv.tvaBiasGain * atGain;
            amp0[i] = prevAmp * g;
            amp1[i] = tvaEnv * g;
        }

        // Ring modulator replaces partial 2's waveform with the product of both generators.
        if (ring && partialLive[1])
            for (int s = 0; s < cb; ++s)
                wg[1][s] = wg[0][s] * wg[1][s];

        for (int i = 0; i < 2; ++i)
        {
            if (! partialLive[i]) continue;
            auto& pv = tv.p[i];
            const bool filt = tp.partial[i].synth;
            const float inc = (amp1[i] - amp0[i]) / float (cb);
            float a = amp0[i];
            for (int s = 0; s < cb; ++s)
            {
                float x = wg[i][s];
                if (filt) x = pv.f2.lowpass (pv.f1.lowpass (x));
                a += inc;
                out[start + s] += x * a * 0.3f;
            }
        }
    }

    bool any = false;
    for (int i = 0; i < 2; ++i)
        any = any || (tp.partialOn[i] && tv.p[i].tva.isActive());
    tv.active = any;
}

void D50Engine::render (float* L, float* R, int n) noexcept
{
    const int maxChunk = (int) upperBuf.size();
    for (int offset = 0; offset < n; offset += maxChunk)
    {
        const int len = std::min (maxChunk, n - offset);
        std::fill (upperBuf.begin(), upperBuf.begin() + len, 0.0f);
        std::fill (lowerBuf.begin(), lowerBuf.begin() + len, 0.0f);

        bool anyVoice = false;
        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            anyVoice = true;
            if (v.upper.active)
                renderTone (v.upper, params.upper,
                            float (v.note + params.patch.upperShift) + params.patch.upperFine * 0.01f,
                            v.velocity, upperBuf.data(), len);
            if (v.lower.active)
                renderTone (v.lower, params.lower,
                            float (v.note + params.patch.lowerShift) + params.patch.lowerFine * 0.01f,
                            v.velocity, lowerBuf.data(), len);
            if (! v.isActive()) { v.note = -1; v.held = v.sustained = false; }
        }
        (void) anyVoice;

        for (int t = 0; t < 2; ++t)
            for (int k = 0; k < 3; ++k)
            {
                const auto& tp = t == 0 ? params.upper : params.lower;
                freeLfoPhase[t][k] += tp.lfoRate[k] * float (len) / sampleRate;
                freeLfoPhase[t][k] -= std::floor (freeLfoPhase[t][k]);
            }

        const float toneGain[2] = { std::min (1.0f, params.patch.toneBalance / 50.0f),
                                    std::min (1.0f, (100.0f - params.patch.toneBalance) / 50.0f) };
        std::fill (revInL.begin(), revInL.begin() + len, 0.0f);
        std::fill (revInR.begin(), revInR.begin() + len, 0.0f);

        for (int t = 0; t < 2; ++t)
        {
            const auto& tp = t == 0 ? params.upper : params.lower;
            float* buf = t == 0 ? upperBuf.data() : lowerBuf.data();
            eqLow[t].setLowShelf (tp.lowEqHz, tp.lowEqDb, sampleRate);
            eqHigh[t].setPeak (tp.highEqHz, tp.highEqQ, tp.highEqDb, sampleRate);
            for (int i = 0; i < len; ++i)
                buf[i] = eqHigh[t].process (eqLow[t].process (buf[i]));
            const auto& cs = kChorusShapes[tp.chorusType];
            chorus[t].process (buf, tmpL.data(), tmpR.data(), len, lfoRateHz (tp.chorusRate) * 0.5f,
                               cs.baseMs, cs.depthMs * tp.chorusDepth, cs.feedback, cs.tremolo * tp.chorusDepth,
                               tp.chorusBalance);
            for (int i = 0; i < len; ++i)
            {
                revInL[(size_t) i] += tmpL[(size_t) i] * toneGain[t];
                revInR[(size_t) i] += tmpR[(size_t) i] * toneGain[t];
            }
        }

        float size, decay, damp, pre;
        reverbSettings (params.patch.reverbType, size, decay, damp, pre);
        reverb.process (revInL.data(), revInR.data(), revOutL.data(), revOutR.data(), len, size, decay, damp, pre);
        const float bal = params.patch.reverbBalance;
        const float vol = params.patch.totalVolume;
        for (int i = 0; i < len; ++i)
        {
            L[offset + i] += dcL.process ((revInL[(size_t) i] * (1.0f - 0.5f * bal) + revOutL[(size_t) i] * bal * 0.6f) * vol);
            R[offset + i] += dcR.process ((revInR[(size_t) i] * (1.0f - 0.5f * bal) + revOutR[(size_t) i] * bal * 0.6f) * vol);
        }
    }
}
} // namespace dc
