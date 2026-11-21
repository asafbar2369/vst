#include "Voice.h"

namespace vsx
{
using namespace pid;

void Voice::prepare (double sampleRateOs, double controlRate)
{
    sr = sampleRateOs;
    cr = controlRate;
    for (auto& o : osc) o.prepare (sr);
    sub.prepare (sr);
    noise.prepare (sr);
    for (auto& f : filt) f.prepare (sr);
    for (auto& e : env) e.prepare (sr);
    for (auto& l : lfo) l.prepare (cr);
    active = false;
}

static LfoParams lfoParams (const ParamSnapshot& p, const ModOffsets& m, int l)
{
    LfoParams lp;
    lp.shape = p.geti (L (l, lfo::Shape));
    lp.rateHz = m.get (p, L (l, lfo::Rate));
    lp.sync = p.getb (L (l, lfo::Sync));
    lp.syncDiv = p.geti (L (l, lfo::SyncDiv));
    lp.mode = p.geti (L (l, lfo::Mode));
    lp.phase = m.get (p, L (l, lfo::Phase));
    lp.delay = p.get (L (l, lfo::Delay));
    lp.fade = p.get (L (l, lfo::Fade));
    lp.smooth = m.get (p, L (l, lfo::Smooth));
    lp.depth = m.get (p, L (l, lfo::Depth));
    return lp;
}

OscTick Voice::makeOscTick (const ParamSnapshot& p, int o, float pitchBase) const noexcept
{
    OscTick t;
    t.on = p.getb (O (o, osc::On));
    if (! t.on) return t;
    t.mode = p.geti (O (o, osc::Mode));
    t.source = p.geti (O (o, osc::Source));
    t.level = mods.get (p, O (o, osc::Level));
    t.pan = mods.get (p, O (o, osc::Pan));
    t.pitchSemis = pitchBase + mods.get (p, O (o, osc::Octave)) * 12.0f + mods.get (p, O (o, osc::Semi))
                   + mods.get (p, O (o, osc::Fine)) * 0.01f + oscDrift[(size_t) o];
    t.position = mods.get (p, O (o, osc::Position));
    t.unison = p.geti (O (o, osc::Unison));
    t.detune = mods.get (p, O (o, osc::Detune));
    t.blend = mods.get (p, O (o, osc::Blend));
    t.spread = mods.get (p, O (o, osc::Spread));
    t.phase = mods.get (p, O (o, osc::Phase));
    t.phaseRand = p.get (O (o, osc::PhaseRand));
    t.warpMode = p.geti (O (o, osc::WarpMode));
    t.warpAmount = mods.get (p, O (o, osc::WarpAmount));
    t.xmodMode = p.geti (O (o, osc::XModMode));
    t.xmodSource = p.geti (O (o, osc::XModSource));
    t.xmodAmount = mods.get (p, O (o, osc::XModAmount));
    t.fold = mods.get (p, O (o, osc::Fold));
    t.pulseWidth = mods.get (p, O (o, osc::PulseWidth));
    t.loopMode = p.geti (O (o, osc::LoopMode));
    t.loopStart = mods.get (p, O (o, osc::LoopStart));
    t.loopEnd = std::max (t.loopStart + 0.001f, mods.get (p, O (o, osc::LoopEnd)));
    t.loopXfade = p.get (O (o, osc::LoopXfade));
    t.rootNote = p.geti (O (o, osc::RootNote));
    t.sampleEnd = mods.get (p, O (o, osc::SampleEnd));
    t.rootAuto = p.get (O (o, osc::RootAuto)) > 0.5f;
    t.fmAmount = mods.get (p, O (o, osc::FmAmount));
    t.grainSize = mods.get (p, O (o, osc::GrainSize));
    t.grainDensity = mods.get (p, O (o, osc::GrainDensity));
    t.grainSpray = mods.get (p, O (o, osc::GrainSpray));
    t.grainPitchRand = mods.get (p, O (o, osc::GrainPitchRand));
    t.grainWindow = p.geti (O (o, osc::GrainWindow));
    t.grainSpread = mods.get (p, O (o, osc::GrainSpread));
    t.specTilt = mods.get (p, O (o, osc::SpecTilt));
    t.specOddEven = mods.get (p, O (o, osc::SpecOddEven));
    t.specStretch = mods.get (p, O (o, osc::SpecStretch));
    t.specFormant = mods.get (p, O (o, osc::SpecFormant));
    t.specPartials = (int) std::round (mods.get (p, O (o, osc::SpecPartials)));
    t.specSpeed = mods.get (p, O (o, osc::SpecSpeed));
    t.physExcite = p.geti (O (o, osc::PhysExcite));
    t.physDecay = mods.get (p, O (o, osc::PhysDecay));
    t.physBright = mods.get (p, O (o, osc::PhysBright));
    t.physDamp = mods.get (p, O (o, osc::PhysDamp));
    t.physInharm = mods.get (p, O (o, osc::PhysInharm));
    t.physMaterial = p.geti (O (o, osc::PhysMaterial));
    t.pdShape = p.geti (O (o, osc::PdShape));
    t.pdAmount = mods.get (p, O (o, osc::PdAmount));
    t.addDecay = mods.get (p, O (o, osc::AddDecay));
    t.addMotion = mods.get (p, O (o, osc::AddMotion));
    return t;
}

void Voice::start (int n, int vel, uint32_t seed, float glideFromPitch, bool legato, const VoiceContext& ctx)
{
    const auto& p = *ctx.params;
    note = n;
    velocity = vel;
    active = true;
    released = false;
    killed = false;
    sustained = false;
    // A reused voice must not inherit expression or drift timing from an earlier note.
    polyPressure = 0.0f;
    driftCounter = 0;
    rng.seed (seed);
    randomValue = rng.bi();
    wseqStep = 0; wseqDir = 1; wseqPhase = 0.0;
    wseqNext = wseqNextStep (juce::jlimit (2, 16, ctx.params->geti (G (global::WseqSteps))), ctx.params->geti (G (global::WseqPattern)));
    targetPitch = (float) n;
    currentPitch = glideFromPitch >= 0.0f ? glideFromPitch : (float) n;

    for (int o = 0; o < kNumOscs; ++o)
    {
        oscDrift[(size_t) o] = 0.0f;
        oscDriftTarget[(size_t) o] = rng.bi();
    }
    for (int f = 0; f < kNumFilters; ++f)
    {
        fltDrift[(size_t) f] = rng.bi() * 0.5f;
        fltDriftTarget[(size_t) f] = rng.bi();
        filt[(size_t) f].setTolerance (p.get (F (f, flt::Tolerance)), hash32 (seed + (uint32_t) f * 101u));
        if (! legato) filt[(size_t) f].reset();
        filterActive[(size_t) f] = false;
    }

    mods.clearAll();
    for (int l = 0; l < kNumLfos; ++l)
        lfo[(size_t) l].noteOn (lfoParams (p, mods, l), hash32 (seed + 1000u + (uint32_t) l));

    for (int e = 0; e < kNumEnvs; ++e)
    {
        env[(size_t) e].setParams (p.get (E (e, env::Delay)), p.get (E (e, env::Attack)), p.get (E (e, env::Hold)),
                                   p.get (E (e, env::Decay)), p.get (E (e, env::Sustain)), p.get (E (e, env::Release)),
                                   p.get (E (e, env::AttackCurve)), p.get (E (e, env::DecayCurve)), p.get (E (e, env::ReleaseCurve)));
        env[(size_t) e].noteOn (legato);
    }

    computeSources (ctx);
    mods.apply (*ctx.matrix, sources.data());

    const float pitchBase = currentPitch;
    for (int o = 0; o < kNumOscs; ++o)
        osc[(size_t) o].noteOn (makeOscTick (p, o, pitchBase), n, vel, ctx.res[(size_t) o], hash32 (seed + 7u * (uint32_t) o + 3u));
    sub.noteOn (hash32 (seed + 55u));
    noise.noteOn (hash32 (seed + 99u));
    ampVelSmooth.setTime (0.002f, sr);
    ampVelSmooth.reset (0.0f);
}

void Voice::retarget (int n, int vel, bool retriggerEnvelopes, bool snapPitch, const VoiceContext& ctx)
{
    note = n;
    targetPitch = (float) n;
    if (snapPitch) currentPitch = (float) n;
    released = false;
    killed = false;
    sustained = false;
    if (retriggerEnvelopes)
    {
        velocity = vel;
        for (auto& e : env) e.noteOn (true);
        for (int l = 0; l < kNumLfos; ++l)
            if (ctx.params->geti (L (l, lfo::Mode)) != 0)
                lfo[(size_t) l].noteOn (lfoParams (*ctx.params, mods, l), rng.next());
    }
}

void Voice::release() noexcept
{
    if (! active || released) return;
    released = true;
    for (auto& e : env) e.noteOff();
    for (auto& oc : osc) oc.noteOff();
}

void Voice::kill() noexcept
{
    if (! active) return;
    killed = true;
    released = true;
    env[0].kill (0.004f);
}

void Voice::hardStop() noexcept
{
    active = false;
    killed = false;
    released = true;
    note = -1;
    for (auto& e : env) e.reset();
}

void Voice::computeSources (const VoiceContext& ctx) noexcept
{
    auto& s = sources;
    s[modsrc::None] = 0.0f;
    for (int e = 0; e < kNumEnvs; ++e) s[(size_t) (modsrc::Env1 + e)] = env[(size_t) e].getValue();
    for (int l = 0; l < kNumLfos; ++l) s[(size_t) (modsrc::Lfo1 + l)] = lfo[(size_t) l].getValue();
    for (int m = 0; m < kNumMacros; ++m) s[(size_t) (modsrc::Macro1 + m)] = ctx.macros[(size_t) m];
    s[modsrc::Velocity] = (float) velocity / 127.0f;
    s[modsrc::Note] = ((float) note - 60.0f) / 60.0f;
    s[modsrc::ModWheel] = ctx.modWheel;
    s[modsrc::Aftertouch] = std::max (ctx.channelPressure, polyPressure);
    s[modsrc::PitchBend] = ctx.pitchBend;
    s[modsrc::Random] = randomValue;
    s[modsrc::Gate] = released ? 0.0f : 1.0f;
    s[modsrc::Expression] = ctx.expression;
    s[modsrc::Breath] = ctx.breath;
    s[modsrc::Timbre] = ctx.timbre;
    s[modsrc::SeqMod] = ctx.seqMod;
}

void Voice::renderTick (const VoiceContext& ctx, float* outL, float* outR, int n) noexcept
{
    if (! active) return;
    const auto& p = *ctx.params;

    // ---- LFOs (rate modulation uses the previous tick's offsets) ----------------------
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto lp = lfoParams (p, mods, l);
        double cyclesPerTick;
        if (lp.sync)
            cyclesPerTick = (ctx.bpm / 60.0) / ParamRegistry::syncDivisionBeats (lp.syncDiv) / ctx.controlRate;
        else
            cyclesPerTick = lp.rateHz / ctx.controlRate;
        lfo[(size_t) l].tick (lp, ctx.lfoGlobalPhase[(size_t) l], ctx.lfoGlobalCycle[(size_t) l], cyclesPerTick,
                              ctx.lfoTables[(size_t) l], l);
    }

    computeSources (ctx);
    mods.apply (*ctx.matrix, sources.data());

    // ---- envelopes -----------------------------------------------------------------
    for (int e = 0; e < kNumEnvs; ++e)
        env[(size_t) e].setParams (mods.get (p, E (e, env::Delay)), mods.get (p, E (e, env::Attack)), mods.get (p, E (e, env::Hold)),
                                   mods.get (p, E (e, env::Decay)), mods.get (p, E (e, env::Sustain)), mods.get (p, E (e, env::Release)),
                                   p.get (E (e, env::AttackCurve)), p.get (E (e, env::DecayCurve)), p.get (E (e, env::ReleaseCurve)));

    // ---- pitch: glide, bend, drift ----------------------------------------------------
    const float glide = p.get (G (global::GlideTime));
    const float tickSec = (float) n / (float) sr;
    if (glide > 1.0e-4f)
        currentPitch += (targetPitch - currentPitch) * (1.0f - std::exp (-tickSec / (glide * 0.25f)));
    else
        currentPitch = targetPitch;

    const float bend = ctx.pitchBend >= 0.0f ? ctx.pitchBend * p.get (G (global::BendUp)) : ctx.pitchBend * p.get (G (global::BendDown));
    const float pitchBase = currentPitch + p.get (G (global::Transpose)) + p.get (G (global::MasterTune)) * 0.01f + bend;

    // slow random drift (analog instability)
    if (++driftCounter >= (int) (cr * 0.7))
    {
        driftCounter = 0;
        for (auto& t : oscDriftTarget) t = rng.bi();
        for (auto& t : fltDriftTarget) t = rng.bi();
    }
    const float driftCoef = 1.0f - std::exp (-tickSec / 0.4f);
    const float oscDriftAmt = mods.get (p, G (global::OscDrift)) * 0.12f; // up to +/-12 cents
    for (int o = 0; o < kNumOscs; ++o)
        oscDrift[(size_t) o] += (oscDriftTarget[(size_t) o] * oscDriftAmt - oscDrift[(size_t) o]) * driftCoef;

    // ---- vector synthesis: equal-power bilinear mix of A (top left), B (top right), C (bottom left), D (bottom right)
    const bool vectorOn = p.getb (G (global::VecOn));
    std::array<float, 4> vecW { 1.0f, 1.0f, 1.0f, 1.0f };
    if (vectorOn)
    {
        const float u = 0.5f * (juce::jlimit (-1.0f, 1.0f, mods.get (p, G (global::VecX))) + 1.0f);
        const float v = 0.5f * (juce::jlimit (-1.0f, 1.0f, mods.get (p, G (global::VecY))) + 1.0f);
        vecW = { std::sqrt ((1.0f - u) * v), std::sqrt (u * v), std::sqrt ((1.0f - u) * (1.0f - v)), std::sqrt (u * (1.0f - v)) };
    }
    const bool vecDIsNoise = p.geti (G (global::VecD)) == 1;

    // ---- wave sequencing: OSC A position steps through Start..End with crossfades ------------
    float seqPosition = -1.0f;
    if (p.getb (G (global::WseqOn)))
    {
        const int steps = juce::jlimit (2, 16, p.geti (G (global::WseqSteps)));
        const int pattern = p.geti (G (global::WseqPattern));
        const double stepsPerSec = p.getb (G (global::WseqSync))
                                       ? (ctx.bpm / 60.0) / ParamRegistry::syncDivisionBeats (p.geti (G (global::WseqDiv)))
                                       : (double) mods.get (p, G (global::WseqRate));
        wseqPhase += (double) tickSec * std::max (0.0, stepsPerSec);
        while (wseqPhase >= 1.0)
        {
            wseqPhase -= 1.0;
            wseqStep = juce::jlimit (0, steps - 1, wseqNext);
            wseqNext = wseqNextStep (steps, pattern);
        }
        const float a = mods.get (p, G (global::WseqStart)), b = mods.get (p, G (global::WseqEnd));
        auto posOf = [&] (int k) { return a + (b - a) * (float) k / (float) (steps - 1); };
        const float xf = juce::jlimit (0.0f, 1.0f, mods.get (p, G (global::WseqXfade)));
        float f = xf > 1.0e-4f ? juce::jlimit (0.0f, 1.0f, (float) (wseqPhase - (1.0 - xf)) / xf) : 0.0f;
        f = f * f * (3.0f - 2.0f * f);
        seqPosition = juce::jlimit (0.0f, 1.0f, posOf (wseqStep) + (posOf (juce::jlimit (0, steps - 1, wseqNext)) - posOf (wseqStep)) * f);
    }

    // ---- oscillators ---------------------------------------------------------------------
    std::array<int, kNumOscs> route {};
    std::array<int, kNumOscs> xsrc {};
    std::array<bool, kNumOscs> oscOn {};
    for (int o = 0; o < kNumOscs; ++o)
    {
        auto t = makeOscTick (p, o, pitchBase);
        if (vectorOn) t.level *= vecW[(size_t) o];
        if (o == 0 && seqPosition >= 0.0f) t.position = seqPosition;
        oscOn[(size_t) o] = t.on;
        route[(size_t) o] = p.geti (O (o, osc::Route));
        xsrc[(size_t) o] = t.xmodSource;
        osc[(size_t) o].beginTick (t, n, ctx.res[(size_t) o]);
    }

    const bool subOn = p.getb (SUB (sub::On));
    const int subShape = p.geti (SUB (sub::Shape));
    const float subFreq = midiToHz (pitchBase + 12.0f * (float) p.geti (SUB (sub::Octave)));
    const float subLevel = mods.get (p, SUB (sub::Level)) * (vectorOn && ! vecDIsNoise ? vecW[3] : 1.0f);
    const float subPan = mods.get (p, SUB (sub::Pan));
    const int subRoute = p.geti (SUB (sub::Route));

    const bool noiseOn = p.getb (NOISE (noise::On));
    const int noiseType = p.geti (NOISE (noise::Type));
    const float noiseLevel = mods.get (p, NOISE (noise::Level)) * (vectorOn && vecDIsNoise ? vecW[3] : 1.0f);
    const float noiseColor = mods.get (p, NOISE (noise::Color));
    const float noisePan = mods.get (p, NOISE (noise::Pan));
    const int noiseRoute = p.geti (NOISE (noise::Route));
    const float noiseKt = mods.get (p, NOISE (noise::KeyTrack));
    const float noiseWidth = mods.get (p, NOISE (noise::Width));
    const float noiseRate = lerp (4000.0f, midiToHz (pitchBase) * 8.0f, noiseKt);

    // ---- filters ------------------------------------------------------------------------
    const bool serial = p.geti (G (global::FilterRouting)) == 0;
    std::array<float, kNumFilters> fMix {}, fGain {};
    for (int f = 0; f < kNumFilters; ++f)
    {
        filterActive[(size_t) f] = p.getb (F (f, flt::On));
        if (! filterActive[(size_t) f]) continue;
        auto& fl = filt[(size_t) f];
        fl.setModel (p.geti (F (f, flt::Model)), p.geti (F (f, flt::Mode)));
        fltDrift[(size_t) f] += (fltDriftTarget[(size_t) f] - fltDrift[(size_t) f]) * driftCoef;
        const float envAmt = mods.get (p, F (f, flt::EnvAmount));
        const float kt = mods.get (p, F (f, flt::KeyTrack));
        const float driftSemis = mods.get (p, F (f, flt::Drift)) * 0.6f * fltDrift[(size_t) f];
        const float octaves = envAmt * 8.0f * env[1].getValue() + kt * (currentPitch - 60.0f) / 12.0f + driftSemis / 12.0f;
        const float cutoff = mods.get (p, F (f, flt::Cutoff)) * std::exp2 (octaves);
        fl.update (juce::jlimit (10.0f, 40000.0f, cutoff), mods.get (p, F (f, flt::Resonance)), mods.get (p, F (f, flt::Drive)),
                   mods.get (p, F (f, flt::Character)), n);
        fMix[(size_t) f] = mods.get (p, F (f, flt::Mix));
        fGain[(size_t) f] = dbToGain (mods.get (p, F (f, flt::Gain)));
    }

    const float ampVel = mods.get (p, G (global::AmpVelocity));
    const float velGain = 1.0f - ampVel + ampVel * std::pow ((float) velocity / 127.0f, 1.5f);
    ampVelSmooth.setTarget (velGain * 0.7f);

    auto pan = [] (float& l, float& r, float pn) noexcept
    {
        l *= pn > 0.0f ? 1.0f - pn : 1.0f;
        r *= pn < 0.0f ? 1.0f + pn : 1.0f;
    };

    std::array<float, 5> lastSrc {}; // A, B, C, Sub, Noise (mono) for cross-modulation
    for (int o = 0; o < kNumOscs; ++o) lastSrc[(size_t) o] = osc[(size_t) o].lastMono;

    for (int i = 0; i < n; ++i)
    {
        const float amp = env[0].next() * ampVelSmooth.next();

        float nl = 0.0f, nr = 0.0f;
        if (noiseOn)
        {
            noise.render (noiseType, noiseColor, noiseWidth, noiseRate, nl, nr);
            lastSrc[4] = 0.5f * (nl + nr);
            nl *= noiseLevel; nr *= noiseLevel;
            pan (nl, nr, noisePan);
        }
        float subOut = 0.0f, subWrap = -1.0f;
        if (subOn)
        {
            subOut = sub.render (subShape, subFreq);
            subWrap = sub.wrapFrac;
            lastSrc[3] = subOut;
        }

        float f1L = 0.0f, f1R = 0.0f, f2L = 0.0f, f2R = 0.0f, dL = 0.0f, dR = 0.0f;
        auto routeTo = [&] (int r, float l, float rr) noexcept
        {
            switch (r)
            {
                case 0: f1L += l; f1R += rr; break;
                case 1: f2L += l; f2R += rr; break;
                case 2:
                    if (serial) { f1L += l; f1R += rr; }
                    else { f1L += l * 0.7071f; f1R += rr * 0.7071f; f2L += l * 0.7071f; f2R += rr * 0.7071f; }
                    break;
                default: dL += l; dR += rr; break;
            }
        };

        for (int o = 0; o < kNumOscs; ++o)
        {
            if (! oscOn[(size_t) o]) continue;
            auto& ov = osc[(size_t) o];
            const int src = xsrc[(size_t) o];
            const float xm = lastSrc[(size_t) src];
            float syncFrac = -1.0f;
            if (src < kNumOscs) syncFrac = osc[(size_t) src].wrapFrac;
            else if (src == 3) syncFrac = subWrap;
            float l, r;
            ov.render (xm, syncFrac, l, r);
            lastSrc[(size_t) o] = ov.lastMono;
            routeTo (route[(size_t) o], l, r);
        }
        if (subOn)
        {
            float l = subOut * subLevel, r = l;
            pan (l, r, subPan);
            routeTo (subRoute, l, r);
        }
        if (noiseOn)
            routeTo (noiseRoute, nl, nr);

        float outLs = dL, outRs = dR;
        auto runFilter = [&] (int f, float& l, float& r) noexcept
        {
            if (! filterActive[(size_t) f]) return;
            float wl = l, wr = r;
            filt[(size_t) f].process (wl, wr);
            const float m = fMix[(size_t) f], g = fGain[(size_t) f];
            l = (l + (wl - l) * m) * g;
            r = (r + (wr - r) * m) * g;
        };

        if (serial)
        {
            runFilter (0, f1L, f1R);
            f2L += f1L; f2R += f1R;
            runFilter (1, f2L, f2R);
            outLs += f2L; outRs += f2R;
        }
        else
        {
            runFilter (0, f1L, f1R);
            runFilter (1, f2L, f2R);
            outLs += f1L + f2L;
            outRs += f1R + f2R;
        }

        outL[i] += outLs * amp;
        outR[i] += outRs * amp;
    }

    for (int e = 1; e < kNumEnvs; ++e)
        env[(size_t) e].advance (n);

    if (! env[0].isActive())
    {
        active = false;
        note = -1;
    }
}

int Voice::wseqNextStep (int steps, int pattern) noexcept
{
    switch (pattern)
    {
        case 1: return (wseqStep - 1 + steps) % steps;                          // down
        case 2:                                                                  // ping-pong
        {
            if (wseqStep + wseqDir >= steps || wseqStep + wseqDir < 0) wseqDir = -wseqDir;
            return wseqStep + wseqDir;
        }
        case 3:                                                                  // random (never the same step twice)
        {
            const int r = (int) (rng.uni() * (float) (steps - 1)) % (steps - 1);
            return r >= wseqStep ? r + 1 : r;
        }
        default: return (wseqStep + 1) % steps;                                  // up
    }
}

} // namespace vsx
