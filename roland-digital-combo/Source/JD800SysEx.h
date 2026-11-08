#pragma once
// JD-800 exclusive-message layer and patch layout.
//
// Transcribed from Roland's "JD-800 Owner's Manual II (Reference)", MIDI Implementation
// (pages V-58 .. V-64) and Parameter List (V-37 .. V-42), downloaded from
// static.roland.com/assets/media/pdf/JD-800_OM.pdf.
//  - Model ID 3DH; Device ID = Unit number - 1 (Unit 17..32 -> 10H..1FH)
//  - Address map: 00-00-00 Patch Temporary (Single), 05-00-00 Patch Memory I-11 .. I-88
//  - Patch = Common (50 bytes, *4-5-1-1) + Effect (46, *4-5-1-2) + Tone A..D (72 each, *4-5-1-3)
// The same Roland message format as the D-50 is reused (D50SysEx.h ...ForModel functions).

#include "D50SysEx.h"

namespace jd800
{
constexpr uint8_t kModelId = 0x3D;
constexpr int kFirstUnit = 17, kLastUnit = 32;
constexpr uint8_t deviceForUnit (int unit) noexcept { return uint8_t (unit - 1); }

constexpr int kCommonSize = 50, kEffectSize = 46, kToneSize = 72;
constexpr int kPatchSize = kCommonSize + kEffectSize + 4 * kToneSize; // 384 = 03-00 (doc "Total Size 00 03 00")

// Offsets inside the Single Patch Temporary Area / one Patch Memory slot (*4-1-1, *4-5-1)
constexpr int kCommonOffset = 0x00;                       // 00-00-00
constexpr int kEffectOffset = 0x32;                       // 00-00-32
constexpr int kToneOffset[4] = { 0x60, 0xA8, 0xF0, 0x138 }; // 00-00-60, 00-01-28, 00-01-70, 00-02-38

constexpr uint32_t kPatchTemp = 0;                                // [00-00-00]
constexpr uint32_t kPatchMemoryBase = d50::toLinear (0x05, 0x00, 0x00);
constexpr int kNumPatches = 64;                                   // I-11 .. I-88
constexpr uint32_t patchMemoryAddress (int index0to63) noexcept { return kPatchMemoryBase + uint32_t (index0to63 * kPatchSize); }

enum class Block { Common, Effect, ToneA, ToneB, ToneC, ToneD };
Block blockForOffset (int patchOffset) noexcept;
int localOffset (int patchOffset) noexcept; // offset inside its block

struct ParamInfo
{
    const char* name;
    uint8_t minValue, maxValue;
};
const ParamInfo& commonParam (int local) noexcept;
const ParamInfo& effectParam (int local) noexcept;
const ParamInfo& toneParam (int local) noexcept;
const ParamInfo& paramForOffset (int patchOffset) noexcept;

namespace common
{
    enum : int
    {
        Name = 0x00, Level = 0x10, KeyRangeLowA = 0x11, KeyRangeHighA = 0x12, // +2 per tone
        BenderDown = 0x19, BenderUp = 0x1A, AftertouchBend = 0x1B, SoloSw = 0x1C, SoloLegato = 0x1D,
        PortamentoSw = 0x1E, PortamentoMode = 0x1F, PortamentoTime = 0x20, LayerTone = 0x21, ActiveTone = 0x22,
        LowFreq = 0x23, LowGain = 0x24, MidFreq = 0x25, MidQ = 0x26, MidGain = 0x27, HighFreq = 0x28, HighGain = 0x29,
        KeyMode = 0x2A, SplitPoint = 0x2B, LowerChannel = 0x2C, UpperChannel = 0x2D,
        LowerProgram = 0x2E, UpperProgram = 0x2F, HoldMode = 0x30
    };
}
namespace effect
{
    enum : int
    {
        GroupASequence = 0x00, GroupBSequence = 0x01, GroupABlock1 = 0x02, GroupBBlock1 = 0x06, GroupBBalance = 0x09,
        DistType = 0x0A, DistDrive = 0x0B, DistLevel = 0x0C,
        PhaserManual = 0x0D, PhaserRate = 0x0E, PhaserDepth = 0x0F, PhaserResonance = 0x10, PhaserMix = 0x11,
        SpectrumBand1 = 0x12, SpectrumWidth = 0x18, EnhancerSens = 0x19, EnhancerMix = 0x1A,
        DelayCenterTap = 0x1B, DelayCenterLevel = 0x1C, DelayLeftTap = 0x1D, DelayLeftLevel = 0x1E,
        DelayRightTap = 0x1F, DelayRightLevel = 0x20, DelayFeedback = 0x21,
        ChorusRate = 0x22, ChorusDepth = 0x23, ChorusDelay = 0x24, ChorusFeedback = 0x25, ChorusLevel = 0x26,
        ReverbType = 0x27, ReverbPreDelay = 0x28, ReverbEarlyRef = 0x29, ReverbHfDamp = 0x2A, ReverbTime = 0x2B,
        ReverbLevel = 0x2C
    };
}
namespace tone
{
    enum : int
    {
        VelocityCurve = 0x00, HoldControl = 0x01,
        Lfo1Rate = 0x02, Lfo1Delay = 0x03, Lfo1Fade = 0x04, Lfo1Wave = 0x05, Lfo1Offset = 0x06, Lfo1KeyTrigger = 0x07,
        Lfo2Rate = 0x08, // LFO2 = LFO1 + 6
        WaveSource = 0x0E, WaveMsb = 0x0F, WaveLsb = 0x10, PitchCoarse = 0x11, PitchFine = 0x12, PitchRandom = 0x13,
        PitchKeyFollow = 0x14, BenderSw = 0x15, AftertouchBendSw = 0x16, Lfo1Sens = 0x17, Lfo2Sens = 0x18,
        LeverSens = 0x19, AftertouchModSens = 0x1A,
        PEnvVelo = 0x1B, PEnvTimeVelo = 0x1C, PEnvTimeKf = 0x1D, PEnvL0 = 0x1E, PEnvT1 = 0x1F, PEnvL1 = 0x20,
        PEnvT2 = 0x21, PEnvT3 = 0x22, PEnvL2 = 0x23,
        FilterMode = 0x24, Cutoff = 0x25, Resonance = 0x26, CutoffKf = 0x27, TvfAftertouch = 0x28,
        TvfLfoSelect = 0x29, TvfLfoDepth = 0x2A, TvfEnvDepth = 0x2B,
        TvfEnvVelo = 0x2C, TvfEnvTimeVelo = 0x2D, TvfEnvTimeKf = 0x2E, TvfT1 = 0x2F, TvfL1 = 0x30, TvfT2 = 0x31,
        TvfL2 = 0x32, TvfT3 = 0x33, TvfSustain = 0x34, TvfT4 = 0x35, TvfL4 = 0x36,
        BiasDirection = 0x37, BiasPoint = 0x38, BiasLevel = 0x39, Level = 0x3A, TvaAftertouch = 0x3B,
        TvaLfoSelect = 0x3C, TvaLfoDepth = 0x3D,
        TvaEnvVelo = 0x3E, TvaEnvTimeVelo = 0x3F, TvaEnvTimeKf = 0x40, TvaT1 = 0x41, TvaL1 = 0x42, TvaT2 = 0x43,
        TvaL2 = 0x44, TvaT3 = 0x45, TvaSustain = 0x46, TvaT4 = 0x47
    };
}

constexpr int kNumWaveforms = 108; // internal waveforms 1..108 (values 0..107, doc *1)

// Value tables printed in the document
extern const float kMidEqFreqHz[17];        // 200 .. 8k
extern const float kMidEqQ[5];              // 0.5, 1.0, 2.0, 4.0, 9.0
extern const float kHfDampHz[17];           // 500 .. 16k, last = BYPASS (0)
extern const int   kWgKeyFollowPercent[17]; // -100 .. +200 %
extern const char* const kGroupASequences[24];
extern const char* const kGroupBSequences[6];
extern const char* const kReverbTypes[10];
extern const char* const kDistortionTypes[7];
extern const char* const kWaveNames[kNumWaveforms];
extern const char kWaveLoopType[kNumWaveforms];   // 'S' short loop, 'L' long loop, 'O' one shot
int tvfKeyFollowPercent (int value0to40) noexcept;  // -100..0 (10 % steps), 0..+150 (5 % steps)
int aftertouchBendSemis (int value0to26) noexcept;  // -36, -24, -12 .. +12
float phaserManualHz (int value0to99) noexcept;     // [*4]
float rate01to10Hz (int value0to99) noexcept;       // 0.1 .. 10 Hz, 0.1 Hz steps
float delayTapMs (int value0to125) noexcept;        // [*5]
float chorusDelayMs (int value0to99) noexcept;      // [*6]
int feedbackPercent (int value0to98) noexcept;      // -98 .. +98 (2 % steps)

// Roland message helpers bound to model 3DH
d50::ParseResult parse (const uint8_t* bytes, size_t length, d50::Message& out) noexcept;
size_t buildDT1 (uint8_t device, uint32_t address, const uint8_t* data, int dataLength, uint8_t* out) noexcept;
size_t buildRQ1 (uint8_t device, uint32_t address, uint32_t size, uint8_t* out) noexcept;
} // namespace jd800
