#pragma once

#include "Filters.h"
#include "Modulation.h"
#include "Oscillator.h"

namespace vsx
{
/** Values shared by all voices during one control tick. */
struct VoiceContext
{
    const ParamSnapshot* params = nullptr;
    const MatrixSnapshot* matrix = nullptr;
    std::array<OscResourcesView, kNumOscs> res {};
    std::array<const LfoTable*, kNumLfos> lfoTables {};
    std::array<double, kNumLfos> lfoGlobalPhase {}, lfoGlobalCycle {};
    double bpm = 120.0;
    double sampleRateOs = 88200.0;
    double controlRate = 2756.25; // ticks per second
    int osFactor = 2;

    // global controller sources
    std::array<float, kNumMacros> macros {};
    float modWheel = 0.0f, channelPressure = 0.0f, pitchBend = 0.0f;
    float expression = 1.0f, breath = 0.0f, timbre = 0.0f, seqMod = 0.0f;
    const float* tuning = nullptr; // 128 pitches (MIDI semitones) per key; nullptr = 12-TET
    float mpeBendRange = 48.0f;     // semitones for a full per-note pitch bend (MPE)
    float pitchOf (int key) const noexcept { return tuning != nullptr && key >= 0 && key < 128 ? tuning[key] : (float) key; }
};

class Voice
{
public:
    void prepare (double sampleRateOs, double controlRate);

    void start (int note, int velocity, uint32_t seed, float glideFromPitch, bool legato, const VoiceContext& ctx);
    void retarget (int note, int velocity, bool retriggerEnvelopes, bool snapPitch, const VoiceContext& ctx); // mono/legato
    void release() noexcept;
    void kill() noexcept;
    void hardStop() noexcept;

    /** Renders one control tick (numSamples at the oversampled rate), adding into out. */
    void renderTick (const VoiceContext& ctx, float* outL, float* outR, int numSamples) noexcept;

    bool isActive() const noexcept { return active; }
    bool isReleased() const noexcept { return released; }
    bool isKilled() const noexcept { return killed; }
    float level() const noexcept { return env[0].getValue(); }

    int note = -1;
    int velocity = 0;
    uint64_t age = 0;
    bool sustained = false;
    float polyPressure = 0.0f;
    // MPE (member-channel notes): per-note pitch bend (-1..1), pressure and slide (CC 74)
    int channel = 1;
    bool mpe = false;
    float mpeBend = 0.0f, mpePressure = 0.0f, mpeTimbre = 0.0f;
    float currentPitch = 60.0f;

    const ModOffsets& getMods() const noexcept { return mods; }
    const float* getSources() const noexcept { return sources.data(); }

private:
    void computeSources (const VoiceContext& ctx) noexcept;
    OscTick makeOscTick (const ParamSnapshot& p, int o, float pitchBase) const noexcept;

    std::array<OscVoice, kNumOscs> osc;
    SubOsc sub;
    NoiseOsc noise;
    std::array<AnalogFilter, kNumFilters> filt;
    std::array<Envelope, kNumEnvs> env;
    std::array<Lfo, kNumLfos> lfo;
    ModOffsets mods;
    std::array<float, modsrc::Count> sources {};

    double sr = 88200.0, cr = 2756.25;
    bool active = false, released = false, killed = false;
    float targetPitch = 60.0f;
    float randomValue = 0.0f;
    Rng rng;
    // wave sequencing state (restarts on every note)
    int wseqStep = 0, wseqNext = 1, wseqDir = 1;
    double wseqPhase = 0.0;
    int wseqNextStep (int steps, int pattern) noexcept;
    std::array<float, kNumOscs> oscDrift {}, oscDriftTarget {};
    std::array<float, kNumFilters> fltDrift {}, fltDriftTarget {};
    int driftCounter = 0;
    std::array<bool, kNumFilters> filterActive {};
    Smoother ampVelSmooth;
};

} // namespace vsx
