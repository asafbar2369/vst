#include "Modulation.h"

namespace vsx
{
void MatrixSnapshot::capture (const PatchState& ps, ParamSnapshot& params)
{
    numSlots = 0;
    numDests = 0;
    const auto& reg = ParamRegistry::get();
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const auto& s = ps.slots[(size_t) i];
        const int src = s.source.load (std::memory_order_relaxed);
        const int dest = s.dest.load (std::memory_order_relaxed);
        if (src <= modsrc::None || src >= modsrc::Count || dest < 0 || dest >= pid::kNumParams)
            continue;
        const float amount = params.value[(size_t) pid::MODAMT (i)];
        if (std::abs (amount) < 1.0e-6f)
            continue;
        int aux = s.aux.load (std::memory_order_relaxed);
        if (aux < 0 || aux >= modsrc::Count) aux = modsrc::None;
        slots[(size_t) numSlots++] = { src, dest, s.polarity.load (std::memory_order_relaxed), aux,
                                       s.curve.load (std::memory_order_relaxed), amount };
        bool found = false;
        for (int d = 0; d < numDests; ++d)
            if (dests[(size_t) d] == dest) { found = true; break; }
        if (! found)
        {
            dests[(size_t) numDests++] = dest;
            params.norm[(size_t) dest] = reg[dest].toNormalised (params.value[(size_t) dest]);
        }
    }
}

void ModOffsets::apply (const MatrixSnapshot& m, const float* sources) noexcept
{
    clearAll();
    for (int i = 0; i < m.numSlots; ++i)
    {
        const auto& s = m.slots[(size_t) i];
        float v = sources[s.source];
        const bool bip = modsrc::isBipolar (s.source);
        if (s.polarity == ModSlotState::Bipolar && ! bip) v = 2.0f * v - 1.0f;
        else if (s.polarity == ModSlotState::Unipolar && bip) v = 0.5f + 0.5f * v;
        if (std::abs (s.curve) > 1.0e-3f)
        {
            const float ex = std::exp2 (s.curve * 2.0f);
            v = (v < 0.0f ? -1.0f : 1.0f) * std::pow (std::abs (v), ex);
        }
        if (s.aux != modsrc::None)
            v *= sources[s.aux];

        float& o = offset[(size_t) s.dest];
        bool already = false;
        for (int t = 0; t < numTouched; ++t)
            if (touched[(size_t) t] == s.dest) { already = true; break; }
        if (! already && numTouched < kNumModSlots)
            touched[(size_t) numTouched++] = s.dest;
        o += v * s.amount;
    }
}

// =================================================================================
void Envelope::setParams (float delay, float attack, float hold, float decay, float sustain, float release,
                          float ac, float dc, float rc) noexcept
{
    pDelay = delay; pAttack = attack; pHold = hold; pDecay = decay; pSustain = juce::jlimit (0.0f, 1.0f, sustain);
    pRelease = release; cAttack = ac; cDecay = dc; cRelease = rc;
}

void Envelope::startSegment (Stage s, float target, float seconds, float curve) noexcept
{
    stage = s;
    segStart = value;
    segEnd = target;
    const float samples = seconds * (float) sr;
    if (samples < 1.0f)
    {
        value = target;
        t = 1.0f;
        dt = 1.0f;
        linear = true;
        return;
    }
    t = 0.0f;
    dt = 1.0f / samples;
    const float kc = -curve * 6.0f;
    linear = std::abs (kc) < 1.0e-3f;
    if (! linear)
    {
        e = 1.0f;
        eMul = std::exp (-kc * dt);
        eDenom = 1.0f / (1.0f - std::exp (-kc));
    }
}

void Envelope::noteOn (bool fromCurrentLevel) noexcept
{
    if (! fromCurrentLevel)
        value = 0.0f;
    if (pDelay > 0.0f)
    {
        stage = Stage::Delay;
        holdSamples = (int) (pDelay * sr);
    }
    else
        startSegment (Stage::Attack, 1.0f, pAttack, cAttack);
}

void Envelope::noteOff() noexcept
{
    if (stage == Stage::Idle || stage == Stage::Release || stage == Stage::Kill)
        return;
    startSegment (Stage::Release, 0.0f, pRelease, cRelease);
}

void Envelope::kill (float seconds) noexcept
{
    if (stage == Stage::Idle) return;
    startSegment (Stage::Kill, 0.0f, seconds, 0.0f);
}

void Envelope::nextStage() noexcept
{
    switch (stage)
    {
        case Stage::Attack:
            if (pHold > 0.0f) { stage = Stage::Hold; holdSamples = (int) (pHold * sr); }
            else startSegment (Stage::Decay, pSustain, pDecay, cDecay);
            break;
        case Stage::Decay: stage = Stage::Sustain; value = pSustain; break;
        case Stage::Release:
        case Stage::Kill: stage = Stage::Idle; value = 0.0f; break;
        default: break;
    }
}

float Envelope::next() noexcept
{
    switch (stage)
    {
        case Stage::Idle: return 0.0f;
        case Stage::Delay:
            if (--holdSamples <= 0) startSegment (Stage::Attack, 1.0f, pAttack, cAttack);
            return value;
        case Stage::Hold:
            if (--holdSamples <= 0) startSegment (Stage::Decay, pSustain, pDecay, cDecay);
            return value;
        case Stage::Sustain:
            value += (pSustain - value) * 0.002f; // follows sustain changes without zipper noise
            return value;
        case Stage::Attack:
        case Stage::Decay:
        case Stage::Release:
        case Stage::Kill:
        {
            if (t >= 1.0f) { nextStage(); return value; }
            t += dt;
            float shape;
            if (linear) shape = t;
            else { e *= eMul; shape = (1.0f - e) * eDenom; }
            if (stage == Stage::Decay) segEnd = pSustain;
            value = segStart + (segEnd - segStart) * std::min (1.0f, shape);
            if (t >= 1.0f) { value = segEnd; nextStage(); }
            return value;
        }
    }
    return value;
}

float Envelope::advance (int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
        next();
    return value;
}

// =================================================================================
void Lfo::noteOn (const LfoParams& p, uint32_t seed) noexcept
{
    rng.seed (seed);
    phase = p.phase;
    elapsed = 0.0;
    finished = false;
    lastCycle = -1;
    rndA = rng.bi();
    rndB = rng.bi();
    out = smoothed = 0.0f;
}

float Lfo::shapeValue (int shape, float ph, float a, float b, const LfoTable* table) noexcept
{
    switch (shape)
    {
        case 0: return std::sin (kTwoPi * ph);
        case 1: return ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f);
        case 2: return 2.0f * ph - 1.0f;
        case 3: return 1.0f - 2.0f * ph;
        case 4: return ph < 0.5f ? 1.0f : -1.0f;
        case 5: return a;
        case 6: return a + (b - a) * (0.5f - 0.5f * std::cos (kPi * ph));
        case 7: return table != nullptr ? table->read (ph) : 0.0f;
        default: return 0.0f;
    }
}

float Lfo::tick (const LfoParams& p, double globalPhase, double globalCycle, double cyclesPerTick, const LfoTable* table,
                 int lfoIndex) noexcept
{
    elapsed += 1.0 / cr;
    float ph;
    if (p.mode == 0)
    {
        const double gp = globalPhase + p.phase;
        ph = (float) (gp - std::floor (gp));
        const auto cyc = (long long) std::floor (globalCycle + p.phase);
        if (cyc != lastCycle)
        {
            lastCycle = cyc;
            rndA = hashToBi ((uint32_t) (cyc * 131 + lfoIndex * 7919));
            rndB = hashToBi ((uint32_t) ((cyc + 1) * 131 + lfoIndex * 7919));
        }
    }
    else
    {
        if (elapsed >= p.delay && ! finished)
            phase += cyclesPerTick;
        if (p.mode == 2 && phase - p.phase >= 1.0)
        {
            phase = p.phase + 0.999999;
            finished = true;
        }
        const auto cyc = (long long) std::floor (phase);
        if (cyc != lastCycle)
        {
            if (lastCycle >= 0) { rndA = rndB; rndB = rng.bi(); }
            lastCycle = cyc;
        }
        ph = (float) (phase - std::floor (phase));
    }

    float v = shapeValue (p.shape, ph, rndA, rndB, table);
    if (elapsed < p.delay)
        v = 0.0f;
    else if (p.fade > 0.0f)
        v *= (float) std::min (1.0, (elapsed - p.delay) / p.fade);

    if (p.smooth > 1.0e-3f)
    {
        const float coef = (float) std::exp (-1.0 / (p.smooth * 0.25 * cr));
        smoothed = v + (smoothed - v) * coef;
    }
    else
        smoothed = v;
    out = smoothed * p.depth;
    return out;
}

} // namespace vsx
