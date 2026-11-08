#pragma once
// JD-800 style patch effects: Group A (Distortion, Phaser, Spectrum, Enhancer; mono, 24 orders)
// and Group B (Chorus, Delay, Reverb; stereo, 6 orders, Dry:Effect balance), with the block
// structure, switches, orders and parameter ranges of the JD-800 Owner's Manual II
// (*4-5-1-2). The DSP inside each block is original and only approximates the hardware.

#include "DspCommon.h"
#include "JD800SysEx.h"

namespace dc
{
struct JdEffectParams
{
    int seqA = 0, seqB = 0;
    bool aOn[4] { false, false, false, false }, bOn[3] { true, true, true };
    float bBalance = 0.5f;                       // 0 = dry only, 1 = effects only
    int distType = 0; float distDrive = 0.3f, distLevel = 0.5f;
    float phManualHz = 600, phRateHz = 0.5f, phDepth = 0.5f, phReso = 0.3f, phMix = 0.5f;
    float specDb[6] {}; int specWidth = 3;
    float enhSens = 0.5f, enhMix = 0.5f;
    float dlyTapMs[3] { 300, 200, 400 }, dlyLevel[3] { 0.3f, 0.3f, 0.3f }; float dlyFeedback = 0.2f;
    float chRateHz = 0.5f, chDepth = 0.5f, chDelayMs = 7, chFeedback = 0, chLevel = 0.5f;
    int rvType = 2; float rvPreMs = 10, rvEarly = 0.3f, rvDampHz = 6300, rvTime = 0.5f, rvLevel = 0.3f;
};

class JdEffects
{
public:
    void prepare (float sampleRate, int maxBlock);
    void reset() noexcept;
    // mono in -> stereo out (in may alias neither L nor R)
    void process (const float* in, float* L, float* R, int n, const JdEffectParams& p) noexcept;

    // For tests: the block order a sequence value selects ("DS-PH-SP-EN" -> {0,1,2,3}).
    static void groupAOrder (int seq, int order[4]) noexcept;
    static void groupBOrder (int seq, int order[3]) noexcept;

private:
    void distortion (float* x, int n, const JdEffectParams& p) noexcept;
    void phaser (float* x, int n, const JdEffectParams& p) noexcept;
    void spectrum (float* x, int n, const JdEffectParams& p) noexcept;
    void enhancer (float* x, int n, const JdEffectParams& p) noexcept;
    void chorus (float* L, float* R, int n, const JdEffectParams& p) noexcept;
    void delay (float* L, float* R, int n, const JdEffectParams& p) noexcept;
    void reverb (float* L, float* R, int n, const JdEffectParams& p) noexcept;

    float sampleRate = 44100.0f;
    std::vector<float> mono, l, r, dryL, dryR, tmpL, tmpR, tmpM;
    Biquad distTone, cryBand, spec[6], enhHp;
    float apState[6] {}, phLfo = 0, phFb = 0;
    Chorus chorusUnit;
    DelayLine dly;
    Reverb rev;
    DelayLine early;
    float gateEnv = 0, gateOpen = 0, revRamp = 0, flyPhase = 0, inEnv = 0;
    bool wasQuiet = true;
};
} // namespace dc
