#pragma once
// Algorithmic substitutes for the JD-800's 108 internal waveforms.
//
// The waveform NUMBERS, NAMES, loop types and one-line descriptions come from the
// "WAVEFORM LIST" in the JD-800 Owner's Manual II (V-34 .. V-36). The audio is NOT the
// original ROM: each entry is synthesised to fit its description (e.g. "Syn Pulse4 = pulse,
// duty cycle 14 %" is reproduced exactly; sampled instruments are approximated with
// harmonic tables, inharmonic partials, FM, noise or generated one-shot transients).

#include "DspCommon.h"
#include "JD800SysEx.h"

namespace dc
{
struct JdWaveRecipe
{
    enum Kind { Saw, SuperSaw, Square, Pulse, Pwm, Triangle, Sine, Table, Fifth, Organ, Inharmonic, Fm,
                SawNoise, SineNoise, VoxNoise, Noise, MetalNoise, OneShot };
    Kind kind;
    float a = 0, b = 0, c = 0, d = 0;   // kind-specific parameters
    int seed = 0;
};

const JdWaveRecipe& jdWaveRecipe (int index0to107) noexcept;

// Shared, read-only generated data (built once, outside the audio thread).
class JdWaveData
{
public:
    static const JdWaveData& get();
    static constexpr int kTableSize = 2048, kNumMips = 10; // mip m holds harmonics 1 .. 512 >> m
    const float* table (int wave, int mip) const noexcept;  // nullptr if the wave is not table-based
    struct OneShotData { std::vector<float> data; float rootHz = 261.6256f, rate = 32000.0f; };
    const OneShotData& oneShot (int wave) const noexcept { return oneShots[(size_t) wave]; }
    float sine (float phase01) const noexcept; // fast table sine
private:
    JdWaveData();
    std::vector<std::vector<float>> tables;  // [wave * kNumMips + mip]
    std::vector<OneShotData> oneShots;
    std::vector<float> sineTable;
};

// Per-tone oscillator state for any of the 108 waves.
struct JdOscillator
{
    float phase[8] {};
    float aux = 0, auxPhase = 0, fmEnv = 1;
    float pink[3] {};
    Svf band;
    Rng rng;
    float pos = 0;          // one-shot read position
    float sampleRate = 44100.0f;

    void reset (int wave, uint32_t seed) noexcept;
    // One sample. If `user` is set (a wave the user loaded from a file) it is played instead.
    float process (int wave, float hz, const SampleWave* user = nullptr) noexcept;
};
} // namespace dc
