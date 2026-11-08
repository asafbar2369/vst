#include "JD800Engine.h"

namespace dc
{
namespace
{
    // Signed quadratic depth: -50..+50 -> -max..+max semitones (fine control near 0). Estimate.
    float signedSq (int centred, float maxSemis) noexcept
    {
        const float x = float (centred) / 50.0f;
        return (x >= 0 ? 1.0f : -1.0f) * maxSemis * x * x;
    }
    float levelScale (float sens, float curved) noexcept
    {
        // Velocity sensitivity -1..+1 (doc -50..+50): + louder/deeper with velocity, - the opposite.
        return sens >= 0.0f ? 1.0f - sens * (1.0f - curved) : 1.0f + sens * curved;
    }
    float timeVeloScale (float sens, float curved) noexcept
    {
        // + : harder playing -> faster T1 (up to 4x), - : slower (estimate)
        return std::exp2 (-sens * (2.0f * curved - 1.0f) * 2.0f);
    }
    float timeKfScale (float kf, float note) noexcept
    {
        // Time key follow -10..+10: + shorter for higher notes (halving per octave at +10, estimate)
        return std::exp2 (-(note - 60.0f) / 12.0f * kf / 10.0f);
    }
} // namespace

float jdVelocityCurve (int curve, float v) noexcept
{
    v = clampf (v, 1.0f / 127.0f, 1.0f);
    auto smooth = [] (float a, float b, float x)
    {
        const float t = clampf ((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    float y;
    switch (curve)
    {
        case 0:  y = smooth (64.0f / 127.0f, 105.0f / 127.0f, v); break; // 1: switch-like between 64 and 105
        case 1:  y = smooth (0.0f, 112.0f / 127.0f, v); break;           // 2: S-curve reaching max at 112
        case 2:  y = v * v; break;                                       // 3: gently curved
        default: y = v * v * v * v; break;                               // 4: steep at the top
    }
    return std::max (1.0f / 127.0f, y);
}

JdPatch JdPatch::decode (const uint8_t* raw) noexcept
{
    using namespace jd800;
    JdPatch p;
    auto clampTo = [] (int v, const ParamInfo& info) { return std::max<int> (info.minValue, std::min<int> (info.maxValue, v)); };
    auto cm = [&] (int o) { return clampTo (raw[kCommonOffset + o], commonParam (o)); };
    auto ef = [&] (int o) { return clampTo (raw[kEffectOffset + o], effectParam (o)); };

    auto& c = p.common;
    c.level = float (cm (common::Level)) / 100.0f;
    for (int t = 0; t < 4; ++t)
    {
        c.keyLow[t] = cm (common::KeyRangeLowA + 2 * t);
        c.keyHigh[t] = cm (common::KeyRangeHighA + 2 * t);
    }
    c.bendDown = cm (common::BenderDown);
    c.bendUp = cm (common::BenderUp);
    c.atBendSemis = aftertouchBendSemis (cm (common::AftertouchBend));
    c.solo = cm (common::SoloSw) != 0;
    c.legato = cm (common::SoloLegato) != 0;
    c.portamento = cm (common::PortamentoSw) != 0;
    c.portaMode = cm (common::PortamentoMode);
    c.portaSec = cm (common::PortamentoTime) == 0 ? 0.0f : timeFromValue (float (cm (common::PortamentoTime)));
    c.layerMask = cm (common::LayerTone);
    c.lowHz = cm (common::LowFreq) ? 400.0f : 200.0f;
    c.lowDb = float (cm (common::LowGain) - 15);
    c.midHz = kMidEqFreqHz[cm (common::MidFreq)];
    c.midQ = kMidEqQ[cm (common::MidQ)];
    c.midDb = float (cm (common::MidGain) - 15);
    c.highHz = cm (common::HighFreq) ? 8000.0f : 4000.0f;
    c.highDb = float (cm (common::HighGain) - 15);

    auto& e = p.effect;
    e.seqA = ef (effect::GroupASequence);
    e.seqB = ef (effect::GroupBSequence);
    for (int i = 0; i < 4; ++i) e.aOn[i] = ef (effect::GroupABlock1 + i) != 0;
    for (int i = 0; i < 3; ++i) e.bOn[i] = ef (effect::GroupBBlock1 + i) != 0;
    e.bBalance = float (ef (effect::GroupBBalance)) / 100.0f;
    e.distType = ef (effect::DistType);
    e.distDrive = float (ef (effect::DistDrive)) / 100.0f;
    e.distLevel = float (ef (effect::DistLevel)) / 100.0f;
    e.phManualHz = phaserManualHz (ef (effect::PhaserManual));
    e.phRateHz = rate01to10Hz (ef (effect::PhaserRate));
    e.phDepth = float (ef (effect::PhaserDepth)) / 100.0f;
    e.phReso = float (ef (effect::PhaserResonance)) / 100.0f;
    e.phMix = float (ef (effect::PhaserMix)) / 100.0f;
    for (int i = 0; i < 6; ++i) e.specDb[i] = float (ef (effect::SpectrumBand1 + i) - 15);
    e.specWidth = ef (effect::SpectrumWidth);
    e.enhSens = float (ef (effect::EnhancerSens)) / 100.0f;
    e.enhMix = float (ef (effect::EnhancerMix)) / 100.0f;
    e.dlyTapMs[0] = delayTapMs (ef (effect::DelayCenterTap));
    e.dlyLevel[0] = float (ef (effect::DelayCenterLevel)) / 100.0f;
    e.dlyTapMs[1] = delayTapMs (ef (effect::DelayLeftTap));
    e.dlyLevel[1] = float (ef (effect::DelayLeftLevel)) / 100.0f;
    e.dlyTapMs[2] = delayTapMs (ef (effect::DelayRightTap));
    e.dlyLevel[2] = float (ef (effect::DelayRightLevel)) / 100.0f;
    e.dlyFeedback = float (feedbackPercent (ef (effect::DelayFeedback))) / 100.0f;
    e.chRateHz = rate01to10Hz (ef (effect::ChorusRate));
    e.chDepth = float (ef (effect::ChorusDepth)) / 100.0f;
    e.chDelayMs = chorusDelayMs (ef (effect::ChorusDelay));
    e.chFeedback = float (feedbackPercent (ef (effect::ChorusFeedback))) / 100.0f;
    e.chLevel = float (ef (effect::ChorusLevel)) / 100.0f;
    e.rvType = ef (effect::ReverbType);
    e.rvPreMs = float (ef (effect::ReverbPreDelay));
    e.rvEarly = float (ef (effect::ReverbEarlyRef)) / 100.0f;
    e.rvDampHz = kHfDampHz[ef (effect::ReverbHfDamp)];
    e.rvTime = float (ef (effect::ReverbTime)) / 100.0f;
    e.rvLevel = float (ef (effect::ReverbLevel)) / 100.0f;

    for (int t = 0; t < 4; ++t)
    {
        auto tn = [&] (int o) { return clampTo (raw[kToneOffset[t] + o], toneParam (o)); };
        auto& tp = p.tone[t];
        tp.velocityCurve = tn (tone::VelocityCurve);
        tp.holdControl = tn (tone::HoldControl) != 0;
        for (int k = 0; k < 2; ++k)
        {
            const int b = tone::Lfo1Rate + 6 * k;
            auto& l = tp.lfo[k];
            l.rateHz = lfoRateHz (float (tn (b)));
            const int d = tn (b + 1);
            l.delayRelease = d == 101;                                 // "REL"
            l.delaySec = (d == 0 || d == 101) ? 0.0f : timeFromValue (float (d));
            const int f = tn (b + 2) - 50;
            l.fadeIn = f > 0;
            l.fadeSec = f == 0 ? 0.0f : timeFromValue (float (std::abs (f)) * 1.6f);
            l.wave = tn (b + 3);
            l.offset = tn (b + 4);
            l.keyTrigger = tn (b + 5) != 0;
        }
        const int wave = (tn (tone::WaveMsb) << 7) | tn (tone::WaveLsb);
        tp.waveform = (tn (tone::WaveSource) == 0 && wave < kNumWaveforms) ? wave : -1; // card / out of range: silent
        tp.coarse = float (tn (tone::PitchCoarse) - 48);
        tp.fineCents = float (tn (tone::PitchFine) - 50);
        tp.randomCents = float (tn (tone::PitchRandom)) * 0.5f;
        tp.keyFollow = float (kWgKeyFollowPercent[tn (tone::PitchKeyFollow)]) / 100.0f;
        tp.bender = tn (tone::BenderSw) != 0;
        tp.aftertouchBend = tn (tone::AftertouchBendSw) != 0;
        tp.lfoSemis[0] = signedSq (tn (tone::Lfo1Sens) - 50, 12.0f);
        tp.lfoSemis[1] = signedSq (tn (tone::Lfo2Sens) - 50, 12.0f);
        tp.leverSemis = signedSq (tn (tone::LeverSens) - 50, 12.0f);
        tp.atModSemis = signedSq (tn (tone::AftertouchModSens) - 50, 12.0f);
        tp.peVelo = float (tn (tone::PEnvVelo) - 50) / 50.0f;
        tp.peTimeVelo = float (tn (tone::PEnvTimeVelo) - 50) / 50.0f;
        tp.peTimeKf = float (tn (tone::PEnvTimeKf) - 10);
        tp.peL0 = signedSq (tn (tone::PEnvL0) - 50, 24.0f);
        tp.peT1 = timeFromValue (float (tn (tone::PEnvT1)));
        tp.peL1 = signedSq (tn (tone::PEnvL1) - 50, 24.0f);
        tp.peT2 = timeFromValue (float (tn (tone::PEnvT2)));
        tp.peT3 = timeFromValue (float (tn (tone::PEnvT3)));
        tp.peL2 = signedSq (tn (tone::PEnvL2) - 50, 24.0f);
        tp.filterMode = tn (tone::FilterMode);
        tp.cutoff = float (tn (tone::Cutoff));
        tp.resonance = float (tn (tone::Resonance));
        tp.cutoffKf = float (tvfKeyFollowPercent (tn (tone::CutoffKf))) / 100.0f;
        tp.tvfAt = float (tn (tone::TvfAftertouch) - 50) / 50.0f;
        tp.tvfLfo = tn (tone::TvfLfoSelect);
        tp.tvfLfoOct = float (tn (tone::TvfLfoDepth) - 50) / 50.0f * 3.0f;
        tp.tvfEnvOct = float (tn (tone::TvfEnvDepth) - 50) / 50.0f * 8.0f;
        tp.fVelo = float (tn (tone::TvfEnvVelo) - 50) / 50.0f;
        tp.fTimeVelo = float (tn (tone::TvfEnvTimeVelo) - 50) / 50.0f;
        tp.fTimeKf = float (tn (tone::TvfEnvTimeKf) - 10);
        tp.fT[0] = timeFromValue (float (tn (tone::TvfT1)));
        tp.fL1 = float (tn (tone::TvfL1)) / 100.0f;
        tp.fT[1] = timeFromValue (float (tn (tone::TvfT2)));
        tp.fL2 = float (tn (tone::TvfL2)) / 100.0f;
        tp.fT[2] = timeFromValue (float (tn (tone::TvfT3)));
        tp.fSus = float (tn (tone::TvfSustain)) / 100.0f;
        tp.fT[3] = timeFromValue (float (tn (tone::TvfT4)));
        tp.fL4 = float (tn (tone::TvfL4)) / 100.0f;
        tp.biasDir = tn (tone::BiasDirection);
        tp.biasPoint = tn (tone::BiasPoint);
        tp.biasDbPerOct = float (tn (tone::BiasLevel) - 10) * 1.2f;
        const float lv = float (tn (tone::Level)) / 100.0f;
        tp.level = lv * lv;
        tp.tvaAt = float (tn (tone::TvaAftertouch) - 50) / 50.0f;
        tp.tvaLfo = tn (tone::TvaLfoSelect);
        tp.tvaLfoDepth = float (tn (tone::TvaLfoDepth) - 50) / 50.0f;
        tp.aVelo = float (tn (tone::TvaEnvVelo) - 50) / 50.0f;
        tp.aTimeVelo = float (tn (tone::TvaEnvTimeVelo) - 50) / 50.0f;
        tp.aTimeKf = float (tn (tone::TvaEnvTimeKf) - 10);
        tp.aT[0] = timeFromValue (float (tn (tone::TvaT1)));
        tp.aL1 = float (tn (tone::TvaL1)) / 100.0f;
        tp.aT[1] = timeFromValue (float (tn (tone::TvaT2)));
        tp.aL2 = float (tn (tone::TvaL2)) / 100.0f;
        tp.aT[2] = timeFromValue (float (tn (tone::TvaT3)));
        tp.aSus = float (tn (tone::TvaSustain)) / 100.0f;
        tp.aT[3] = timeFromValue (float (tn (tone::TvaT4)));
    }
    return p;
}

// ---------------------------------------------------------------------------
void JD800Engine::prepare (double sr, int maxBlock)
{
    sampleRate = float (sr);
    const size_t n = (size_t) std::max (64, maxBlock);
    for (auto* b : { &bus, &wetL, &wetR })
        b->assign (n, 0.0f);
    effects.prepare (sampleRate, (int) n);
    (void) JdWaveData::get(); // build substitute waves outside the audio callback
    reset();
}

void JD800Engine::reset() noexcept
{
    allSoundOff();
    effects.reset();
    eqLow.reset(); eqMid.reset(); eqHigh.reset();
}

int JD800Engine::activeVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voices) c += v.isActive() ? 1 : 0;
    return c;
}

int JD800Engine::activeToneCount (int t) const noexcept
{
    int c = 0;
    for (auto& v : voices) c += v.tone[t & 3].active ? 1 : 0;
    return c;
}

void JD800Engine::startTone (Voice& v, int t, bool retrigger) noexcept
{
    const auto& tp = patch.tone[t];
    auto& ts = v.tone[t];
    const auto& c = patch.common;
    const bool inRange = v.note >= c.keyLow[t] && v.note <= c.keyHigh[t];
    if ((c.layerMask & (1 << t)) == 0 || ! inRange || tp.waveform < 0)
    {
        ts.active = false;
        ts.aenv.kill(); ts.fenv.kill(); ts.penv.kill();
        return;
    }
    if (! retrigger && ts.active)
        return;

    ts.active = true;
    ts.released = false;
    ts.releaseHandled = false;
    ts.elapsed = ts.sinceRelease = 0.0f;
    ts.lastAmp = 0.0f;
    const float cv = jdVelocityCurve (tp.velocityCurve, v.velocity);
    ts.velocity = cv;
    ts.osc.sampleRate = sampleRate;
    ts.osc.reset (tp.waveform, uint32_t (v.note * 977 + t * 131 + (int) counter));
    ts.filter.reset();
    ts.randomCents = tp.randomCents * noteRng.next();
    const float note = float (v.note);

    // Pitch envelope: L0 -T1-> L1 -T2-> 0 (sustain) ; key off -T3-> L2
    {
        const float s = levelScale (tp.peVelo, cv);
        const float k = timeKfScale (tp.peTimeKf, note);
        auto& e = ts.penv;
        e.sampleRate = sampleRate;
        e.numSegments = 2;
        e.startLevel = tp.peL0 * s;
        e.times[0] = tp.peT1 * k * timeVeloScale (tp.peTimeVelo, cv); e.levels[0] = tp.peL1 * s;
        e.times[1] = tp.peT2 * k;                                     e.levels[1] = 0.0f;
        e.releaseTime = tp.peT3 * k;
        e.releaseLevel = tp.peL2 * s;
        e.noteOn();
    }
    // TVF envelope: 0 -T1-> L1 -T2-> L2 -T3-> Sustain ; key off -T4-> L4
    {
        const float s = levelScale (tp.fVelo, cv);
        const float k = timeKfScale (tp.fTimeKf, note);
        auto& e = ts.fenv;
        e.sampleRate = sampleRate;
        e.numSegments = 3;
        e.startLevel = 0.0f;
        e.times[0] = tp.fT[0] * k * timeVeloScale (tp.fTimeVelo, cv); e.levels[0] = tp.fL1 * s;
        e.times[1] = tp.fT[1] * k;                                    e.levels[1] = tp.fL2 * s;
        e.times[2] = tp.fT[2] * k;                                    e.levels[2] = tp.fSus * s;
        e.releaseTime = tp.fT[3] * k;
        e.releaseLevel = tp.fL4 * s;
        e.noteOn();
    }
    // TVA envelope: 0 -T1-> L1 -T2-> L2 -T3-> Sustain ; key off -T4-> 0
    {
        const float s = levelScale (tp.aVelo, cv);
        const float k = timeKfScale (tp.aTimeKf, note);
        auto& e = ts.aenv;
        e.sampleRate = sampleRate;
        e.numSegments = 3;
        e.startLevel = 0.0f;
        e.times[0] = tp.aT[0] * k * timeVeloScale (tp.aTimeVelo, cv); e.levels[0] = tp.aL1 * s;
        e.times[1] = tp.aT[1] * k;                                    e.levels[1] = tp.aL2 * s;
        e.times[2] = tp.aT[2] * k;                                    e.levels[2] = tp.aSus * s;
        e.releaseTime = tp.aT[3] * k;
        e.releaseLevel = 0.0f;
        e.noteOn();
    }
    // TVA bias: UP = keys above the point, LOW = below, U&L = both sides; level in dB/octave (estimate)
    {
        const float d = note - float (tp.biasPoint);
        float oct = 0.0f;
        if (tp.biasDir == 0 && d > 0) oct = d / 12.0f;
        else if (tp.biasDir == 1 && d < 0) oct = -d / 12.0f;
        else if (tp.biasDir == 2) oct = std::abs (d) / 12.0f;
        ts.biasGain = dbToGain (tp.biasDbPerOct * oct);
    }
    for (int k = 0; k < 2; ++k)
    {
        auto& l = ts.lfo[k];
        l.phase = tp.lfo[k].keyTrigger ? 0.0f : freeLfoPhase[t][k];
        l.rng.state = 0x1234u + uint32_t (t * 17 + k * 5 + v.note);
        l.held = l.rng.next();
        l.from = l.held; l.to = l.rng.next();
    }
}

void JD800Engine::releaseTone (ToneState& ts) noexcept
{
    if (! ts.active || ts.released) return;
    ts.released = true;
    ts.sinceRelease = 0.0f;
    ts.aenv.noteOff(); ts.fenv.noteOff(); ts.penv.noteOff();
}

void JD800Engine::releaseVoice (Voice& v) noexcept
{
    v.held = false;
    v.sustained = false;
    for (auto& ts : v.tone) releaseTone (ts);
}

void JD800Engine::noteOn (int note, float velocity) noexcept
{
    const auto& c = patch.common;
    if (heldCount < 32) heldStack[heldCount++] = note;

    const bool anyHeldBefore = heldCount > 1;
    Voice* target = nullptr;
    bool retrigger = true;
    if (c.solo)
    {
        target = &voices[0];
        for (int i = 1; i < kMaxVoices; ++i) if (voices[i].isActive()) releaseVoice (voices[i]);
        if (c.legato && target->held && target->isActive())
            retrigger = false; // Solo Legato: change pitch without restarting the envelopes
    }
    else
    {
        for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr)
        {
            uint64_t best = UINT64_MAX;
            for (auto& v : voices)
                if (! v.held && ! v.sustained && v.age < best) { best = v.age; target = &v; }
            if (target == nullptr)
                for (auto& v : voices)
                    if (v.age < best) { best = v.age; target = &v; }
        }
    }
    auto& v = *target;
    const int previous = lastNote;
    v.note = note;
    v.held = true;
    v.sustained = false;
    v.age = ++counter;
    if (retrigger) v.velocity = clampf (velocity, 0.0f, 1.0f);
    for (int t = 0; t < 4; ++t)
        startTone (v, t, retrigger);

    // Portamento: NORMAL always glides, LEGATO only while another key is held
    v.glideOffset = v.glideRate = 0.0f;
    if (c.portamento && previous >= 0 && previous != note && c.portaSec > 0.0f && (c.portaMode == 0 || anyHeldBefore))
    {
        v.glideOffset = float (previous - note);
        v.glideRate = std::abs (v.glideOffset) / c.portaSec;
    }
    lastNote = note;
}

void JD800Engine::noteOff (int note) noexcept
{
    for (int i = 0; i < heldCount; ++i)
        if (heldStack[i] == note)
        {
            for (int j = i; j < heldCount - 1; ++j) heldStack[j] = heldStack[j + 1];
            --heldCount;
            break;
        }

    const auto& c = patch.common;
    if (c.solo)
    {
        auto& v = voices[0];
        if (v.note != note || ! v.held) return;
        if (heldCount > 0) // return to the most recent key still held
        {
            const int back = heldStack[heldCount - 1];
            const int from = v.note;
            v.note = back;
            for (int t = 0; t < 4; ++t) startTone (v, t, ! c.legato);
            v.glideOffset = v.glideRate = 0.0f;
            if (c.portamento && c.portaSec > 0.0f)
            {
                v.glideOffset = float (from - back);
                v.glideRate = std::abs (v.glideOffset) / c.portaSec;
            }
            lastNote = back;
            return;
        }
    }

    for (auto& v : voices)
        if (v.isActive() && v.held && v.note == note)
        {
            if (sustainDown)
            {
                v.held = false;
                v.sustained = true;
                for (int t = 0; t < 4; ++t)          // Hold control OFF: pedal does not hold this tone
                    if (! patch.tone[t].holdControl) releaseTone (v.tone[t]);
            }
            else releaseVoice (v);
        }
}

void JD800Engine::setSustain (bool down) noexcept
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.sustained) releaseVoice (v);
}

void JD800Engine::allNotesOff() noexcept
{
    sustainDown = false;
    heldCount = 0;
    for (auto& v : voices)
        if (v.isActive()) releaseVoice (v);
}

void JD800Engine::allSoundOff() noexcept
{
    sustainDown = false;
    heldCount = 0;
    for (auto& v : voices)
    {
        v.held = v.sustained = false;
        v.note = -1;
        v.glideOffset = v.glideRate = 0.0f;
        for (auto& ts : v.tone) { ts.active = false; ts.aenv.kill(); ts.fenv.kill(); ts.penv.kill(); }
    }
}

float JD800Engine::lfoValue (LfoState& s, const JdLfoParams& p, int, int, float dt, bool released) noexcept
{
    s.elapsed += dt;
    s.phase += p.rateHz * dt;
    if (s.phase >= 1.0f)
    {
        s.phase -= std::floor (s.phase);
        s.held = s.rng.next();
        s.from = s.to;
        s.to = s.rng.next();
    }
    float x;
    switch (p.wave)
    {
        case 1:  x = 2.0f * s.phase - 1.0f; break;                              // SAW
        case 2:  x = s.phase < 0.5f ? 1.0f : -1.0f; break;                      // SQU
        case 3:  x = s.held; break;                                             // S/H
        case 4:  x = s.from + (s.to - s.from) * s.phase; break;                 // RND (continuous)
        default: x = s.phase < 0.5f ? 4.0f * s.phase - 1.0f : 3.0f - 4.0f * s.phase; break; // TRI
    }
    if (p.offset == 0) x = 0.5f * (x + 1.0f);
    else if (p.offset == 2) x = 0.5f * (x - 1.0f);

    // Delay (from key-on, or from key-off for REL) and Fade in/out
    float te;
    if (p.delayRelease)
    {
        if (! released) return 0.0f;
        te = s.elapsed;   // elapsed is reset at release for REL (see renderTone)
    }
    else te = s.elapsed - p.delaySec;
    if (te < 0.0f) return 0.0f;
    float amp = 1.0f;
    if (p.fadeSec > 0.0f)
        amp = p.fadeIn ? std::min (1.0f, te / p.fadeSec) : std::max (0.0f, 1.0f - te / p.fadeSec);
    return x * amp;
}

void JD800Engine::renderTone (Voice& v, int t, float* out, int n) noexcept
{
    auto& ts = v.tone[t];
    const auto& tp = patch.tone[t];
    const auto& c = patch.common;
    const float note = float (v.note);
    const float q = 0.5f * std::pow (40.0f, tp.resonance / 100.0f);
    float glide = v.glideOffset;

    for (int start = 0; start < n; start += kControlRate)
    {
        const int cb = std::min (kControlRate, n - start);
        const float dt = float (cb) / sampleRate;
        ts.elapsed += dt;
        if (ts.released)
        {
            if (! ts.releaseHandled) // LFO "REL" delay: start counting at key-off
            {
                ts.releaseHandled = true;
                for (int k = 0; k < 2; ++k)
                    if (tp.lfo[k].delayRelease) ts.lfo[k].elapsed = 0.0f;
            }
            ts.sinceRelease += dt;
        }
        if (v.glideRate > 0.0f)
        {
            const float step = v.glideRate * dt;
            glide = std::abs (glide) <= step ? 0.0f : glide - std::copysign (step, glide);
        }

        float lv[2];
        for (int k = 0; k < 2; ++k) lv[k] = lfoValue (ts.lfo[k], tp.lfo[k], k, t, dt, ts.released);

        float bendSemis = 0.0f;
        if (tp.bender) bendSemis += bend >= 0.0f ? bend * float (c.bendUp) : bend * float (c.bendDown);
        if (tp.aftertouchBend) bendSemis += aftertouch * float (c.atBendSemis);
        const float lever = tp.leverSemis >= 0 ? lv[0] * tp.leverSemis : lv[1] * -tp.leverSemis;
        const float atMod = tp.atModSemis >= 0 ? lv[0] * tp.atModSemis : lv[1] * -tp.atModSemis;
        const float penv = ts.penv.advance (cb);
        const float pitch = 60.0f + (note + glide - 60.0f) * tp.keyFollow + tp.coarse
                          + (tp.fineCents + ts.randomCents) * 0.01f + penv
                          + lv[0] * tp.lfoSemis[0] + lv[1] * tp.lfoSemis[1]
                          + modWheel * lever + aftertouch * atMod + bendSemis;
        const float hz = midiToHz (clampf (pitch, -24.0f, 150.0f));

        const float fenv = ts.fenv.advance (cb);
        const float oct = tp.cutoff * 0.1f + tp.cutoffKf * (note - 60.0f) / 12.0f + tp.tvfEnvOct * fenv
                        + tp.tvfLfoOct * lv[tp.tvfLfo] + tp.tvfAt * aftertouch * 3.0f;
        ts.filter.set (20.0f * std::exp2 (clampf (oct, 0.0f, 10.5f)), q, sampleRate);

        const float aenv = ts.aenv.advance (cb);
        const float trem = 1.0f + tp.tvaLfoDepth * lv[tp.tvaLfo] * 0.5f;
        const float target = aenv * tp.level * ts.biasGain * std::max (0.0f, trem)
                           * std::max (0.0f, 1.0f + tp.tvaAt * aftertouch * 0.5f) * 0.3f;
        const float inc = (target - ts.lastAmp) / float (cb);
        float a = ts.lastAmp;
        for (int s = 0; s < cb; ++s)
        {
            float lp, bp, hp;
            ts.filter.process (ts.osc.process (tp.waveform, hz), lp, bp, hp);
            const float y = tp.filterMode == 0 ? hp : (tp.filterMode == 1 ? bp : lp);
            a += inc;
            out[start + s] += y * a;
        }
        ts.lastAmp = target;
    }
    ts.active = ts.aenv.isActive();
}

void JD800Engine::render (float* L, float* R, int n) noexcept
{
    const auto& c = patch.common;
    const int maxChunk = (int) bus.size();
    for (int offset = 0; offset < n; offset += maxChunk)
    {
        const int len = std::min (maxChunk, n - offset);
        std::fill (bus.begin(), bus.begin() + len, 0.0f);

        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            for (int t = 0; t < 4; ++t)
                if (v.tone[t].active)
                    renderTone (v, t, bus.data(), len);
            if (v.glideRate > 0.0f)
            {
                const float step = v.glideRate * float (len) / sampleRate;
                v.glideOffset = std::abs (v.glideOffset) <= step ? 0.0f : v.glideOffset - std::copysign (step, v.glideOffset);
                if (std::abs (v.glideOffset) < 1.0e-6f) v.glideRate = 0.0f;
            }
            if (! v.isActive()) { v.note = -1; v.held = v.sustained = false; }
        }
        for (int t = 0; t < 4; ++t)
            for (int k = 0; k < 2; ++k)
            {
                freeLfoPhase[t][k] += patch.tone[t].lfo[k].rateHz * float (len) / sampleRate;
                freeLfoPhase[t][k] -= std::floor (freeLfoPhase[t][k]);
            }

        // Patch EQ (Low shelf 200/400 Hz, Mid peak, High shelf 4k/8k), then the effects chain
        eqLow.setLowShelf (c.lowHz, c.lowDb, sampleRate);
        eqMid.setPeak (c.midHz, c.midQ, c.midDb, sampleRate);
        eqHigh.setHighShelf (std::min (c.highHz, sampleRate * 0.4f), c.highDb, sampleRate);
        for (int i = 0; i < len; ++i)
            bus[(size_t) i] = eqHigh.process (eqMid.process (eqLow.process (bus[(size_t) i])));
        effects.process (bus.data(), wetL.data(), wetR.data(), len, patch.effect);
        for (int i = 0; i < len; ++i)
        {
            L[offset + i] += wetL[(size_t) i] * c.level;
            R[offset + i] += wetR[(size_t) i] * c.level;
        }
    }
}
} // namespace dc
