#pragma once

#include "../Core/DspUtils.h"
#include "../Core/Params.h"
#include "../Core/PatchState.h"
#include <array>

namespace vsx
{
/** Block-level snapshot of all parameter values (real units) plus normalised values of modulation targets. */
struct ParamSnapshot
{
    std::array<float, pid::kNumParams> value {};
    std::array<float, pid::kNumParams> norm {};
    float get (int i) const noexcept { return value[(size_t) i]; }
    int geti (int i) const noexcept { return (int) std::lround (value[(size_t) i]); }
    bool getb (int i) const noexcept { return value[(size_t) i] >= 0.5f; }
};

/** Copy of the modulation-matrix routing taken once per block (avoids torn reads mid-block). */
struct MatrixSnapshot
{
    struct Slot { int source, dest, polarity, aux; float curve, amount; };
    std::array<Slot, kNumModSlots> slots {};
    int numSlots = 0;
    std::array<int, kNumModSlots> dests {};
    int numDests = 0;

    /** Captures active slots and fills params.norm for every modulated destination. */
    void capture (const PatchState& ps, ParamSnapshot& params);
};

/** DAHDSR envelope with per-segment curvature (negative curve = fast start, RC-like). */
class Envelope
{
public:
    enum class Stage { Idle, Delay, Attack, Hold, Decay, Sustain, Release, Kill };

    void prepare (double sampleRate) { sr = sampleRate; }
    void setParams (float delay, float attack, float hold, float decay, float sustain, float release,
                    float attackCurve, float decayCurve, float releaseCurve) noexcept;
    void noteOn (bool fromCurrentLevel) noexcept;
    void noteOff() noexcept;
    void kill (float seconds = 0.004f) noexcept;
    void reset() noexcept { stage = Stage::Idle; value = 0.0f; }

    float next() noexcept;
    float advance (int numSamples) noexcept; // control-rate stepping
    float getValue() const noexcept { return value; }
    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release || stage == Stage::Kill; }
    Stage getStage() const noexcept { return stage; }

private:
    void startSegment (Stage s, float target, float seconds, float curve) noexcept;
    void nextStage() noexcept;

    double sr = 44100.0;
    float pDelay = 0, pAttack = 0.003f, pHold = 0, pDecay = 1, pSustain = 1, pRelease = 0.15f;
    float cAttack = 0, cDecay = -0.6f, cRelease = -0.6f;

    Stage stage = Stage::Idle;
    float value = 0.0f, segStart = 0.0f, segEnd = 0.0f;
    float t = 0.0f, dt = 0.0f;
    float e = 1.0f, eMul = 1.0f, eDenom = 1.0f; // incremental exp shape
    bool linear = true;
    int holdSamples = 0;
};

struct LfoParams
{
    int shape = 0;
    float rateHz = 2.0f;
    bool sync = false;
    int syncDiv = 10;
    int mode = 0;      // 0 free, 1 retrigger, 2 envelope (one-shot)
    float phase = 0.0f, delay = 0.0f, fade = 0.0f, smooth = 0.0f;
};

/** Per-voice LFO evaluated at control rate. Free mode follows a shared global phase. */
class Lfo
{
public:
    void prepare (double controlRate) { cr = controlRate; }
    void noteOn (const LfoParams& p, uint32_t seed) noexcept;
    float tick (const LfoParams& p, double globalPhase, double globalCycle, double cyclesPerTick,
                const LfoTable* table, int lfoIndex) noexcept;
    float getValue() const noexcept { return out; }

    static float shapeValue (int shape, float phase, float rndA, float rndB, const LfoTable* table) noexcept;

private:
    double cr = 2756.25; // ticks per second
    double phase = 0.0;
    double elapsed = 0.0;
    bool finished = false;
    float out = 0.0f, smoothed = 0.0f;
    float rndA = 0.0f, rndB = 0.0f;
    long long lastCycle = -1;
    Rng rng;
};

/** Evaluates modulation for one voice (or the global context) into per-parameter normalised offsets. */
struct ModOffsets
{
    std::array<float, pid::kNumParams> offset {};
    std::array<int, kNumModSlots> touched {};
    int numTouched = 0;

    /** Clears the previous offsets and accumulates the matrix for the given source values. */
    void apply (const MatrixSnapshot& m, const float* sources) noexcept;
    void clearAll() noexcept
    {
        for (int i = 0; i < numTouched; ++i) offset[(size_t) touched[(size_t) i]] = 0.0f;
        numTouched = 0;
    }

    /** Modulated value in real units. */
    float get (const ParamSnapshot& p, int index) const noexcept
    {
        const float o = offset[(size_t) index];
        if (o == 0.0f) return p.value[(size_t) index];
        return ParamRegistry::get()[index].fromNormalised (p.norm[(size_t) index] + o);
    }
};

} // namespace vsx
