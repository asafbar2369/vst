#pragma once
// D-50 exclusive-message layer.
//
// Every constant in this file was transcribed from Roland's own document
// "D-50 MIDI Implementation, Version 2.00" (static.roland.com/assets/media/pdf/D-50_MI.pdf),
// sections 4.1 (message structure), 4.2 (address mapping) and notes *4-3 .. *4-7
// (patch / partial / common / patch-factor / reverb block layouts).
// Test vectors from section 10 of the same document live in Tests/TestMain.cpp.
//
// This file is plain C++ with no JUCE dependency so it can be tested in isolation.

#include <cstddef>
#include <cstdint>

namespace d50
{
// ---- 4.1 message structure ------------------------------------------------
constexpr uint8_t kExclusiveStatus = 0xF0;
constexpr uint8_t kEndOfExclusive  = 0xF7;
constexpr uint8_t kRolandId        = 0x41;
constexpr uint8_t kModelId         = 0x14; // "Model-ID # (D-50)" = 0001 0100

// Command IDs (sections 3 and 4 of the generic Roland part of the document)
constexpr uint8_t kRQ1 = 0x11, kDT1 = 0x12;
constexpr uint8_t kWSD = 0x40, kRQD = 0x41, kDAT = 0x42, kACK = 0x43,
                  kEOD = 0x45, kERR = 0x4E, kRJC = 0x4F;

// DT1 / DAT carry at most 256 data bytes (notes *6-2, *7-2, *8-2).
constexpr int kMaxDataBytesPerMessage = 256;

// ---- 4.2 address mapping ---------------------------------------------------
// Addresses are three 7-bit bytes [hh mm ll] -> linear = hh*16384 + mm*128 + ll.
constexpr uint32_t toLinear (uint8_t hh, uint8_t mm, uint8_t ll) noexcept
{
    return (uint32_t (hh & 0x7F) << 14) | (uint32_t (mm & 0x7F) << 7) | uint32_t (ll & 0x7F);
}
void toAddress (uint32_t linear, uint8_t out[3]) noexcept;

constexpr int kBlockSize = 64;               // every partial/common/patch block = 64 bytes
constexpr int kTempSize  = 7 * kBlockSize;   // 448 = whole temporary area

// Temporary area, block start addresses exactly as listed in 4.2
constexpr uint32_t kUpperPartial1 = toLinear (0x00, 0x00, 0x00);
constexpr uint32_t kUpperPartial2 = toLinear (0x00, 0x00, 0x40);
constexpr uint32_t kUpperCommon   = toLinear (0x00, 0x01, 0x00);
constexpr uint32_t kLowerPartial1 = toLinear (0x00, 0x01, 0x40);
constexpr uint32_t kLowerPartial2 = toLinear (0x00, 0x02, 0x00);
constexpr uint32_t kLowerCommon   = toLinear (0x00, 0x02, 0x40);
constexpr uint32_t kPatchFactors  = toLinear (0x00, 0x03, 0x00);

// Memory area: Patch Memory 1-1 .. 8-8, then Reverb Data 17 .. 32
constexpr uint32_t kPatchMemoryBase = toLinear (0x02, 0x00, 0x00);
constexpr int      kNumPatches      = 64;
constexpr int      kPatchStride     = kTempSize; // 448: 1-2 sits at [02-03-40]
constexpr uint32_t kReverbBase      = toLinear (0x03, 0x60, 0x00);
constexpr int      kReverbStride    = 376;      // 188 data, nibblised (*4-7)
constexpr int      kFirstReverb = 17, kLastReverb = 32;
constexpr uint32_t kMemoryEnd = kReverbBase + uint32_t ((kLastReverb - kFirstReverb + 1) * kReverbStride);

constexpr uint32_t patchMemoryAddress (int index0to63) noexcept
{
    return kPatchMemoryBase + uint32_t (index0to63 * kPatchStride);
}
constexpr uint32_t reverbAddress (int number17to32) noexcept
{
    return kReverbBase + uint32_t ((number17to32 - kFirstReverb) * kReverbStride);
}

// ---- Block layouts (*4-4 partial, *4-5 common, *4-6 patch) ----------------
enum class Block { UpperPartial1, UpperPartial2, UpperCommon, LowerPartial1, LowerPartial2, LowerCommon, Patch };
Block blockForTempOffset (int offset0to447) noexcept;

struct ParamInfo
{
    const char* name;
    uint8_t maxValue;  // document's value range is 0..maxValue
};

const ParamInfo& partialParam (int offset0to63) noexcept;
const ParamInfo& commonParam  (int offset0to63) noexcept;
const ParamInfo& patchParam   (int offset0to63) noexcept;
uint8_t maxValueForTempOffset (int offset0to447) noexcept;

// Partial parameter offsets (*4-4)
namespace partial
{
    enum : int
    {
        Coarse = 0, Fine = 1, Keyfollow = 2, LfoMode = 3, PEnvMode = 4, BendMode = 5,
        Waveform = 6, PcmWave = 7, PulseWidth = 8, PwVelocity = 9, PwLfoSelect = 10, PwLfoDepth = 11, PwAftertouch = 12,
        TvfCutoff = 13, TvfResonance = 14, TvfKeyfollow = 15, TvfBiasPoint = 16, TvfBiasLevel = 17,
        TvfEnvDepth = 18, TvfEnvVelocity = 19, TvfEnvDepthKf = 20, TvfEnvTimeKf = 21,
        TvfT1 = 22, TvfT2 = 23, TvfT3 = 24, TvfT4 = 25, TvfT5 = 26,
        TvfL1 = 27, TvfL2 = 28, TvfL3 = 29, TvfSustain = 30, TvfEnd = 31,
        TvfLfoSelect = 32, TvfLfoDepth = 33, TvfAftertouch = 34,
        TvaLevel = 35, TvaVelocity = 36, TvaBiasPoint = 37, TvaBiasLevel = 38,
        TvaT1 = 39, TvaT2 = 40, TvaT3 = 41, TvaT4 = 42, TvaT5 = 43,
        TvaL1 = 44, TvaL2 = 45, TvaL3 = 46, TvaSustain = 47, TvaEnd = 48,
        TvaVelocityFollow = 49, TvaTimeKf = 50, TvaLfoSelect = 51, TvaLfoDepth = 52, TvaAftertouch = 53
    };
}
// Common parameter offsets (*4-5)
namespace common
{
    enum : int
    {
        Name = 0, Structure = 10, PEnvVelocity = 11, PEnvTimeKf = 12,
        PEnvT1 = 13, PEnvT2 = 14, PEnvT3 = 15, PEnvT4 = 16,
        PEnvL0 = 17, PEnvL1 = 18, PEnvL2 = 19, PEnvSustain = 20, PEnvEnd = 21,
        PModLfoDepth = 22, PModLever = 23, PModAftertouch = 24,
        Lfo1Wave = 25, Lfo1Rate = 26, Lfo1Delay = 27, Lfo1Sync = 28,
        Lfo2Wave = 29, Lfo2Rate = 30, Lfo2Delay = 31, Lfo2Sync = 32,
        Lfo3Wave = 33, Lfo3Rate = 34, Lfo3Delay = 35, Lfo3Sync = 36,
        LowEqFreq = 37, LowEqGain = 38, HighEqFreq = 39, HighEqQ = 40, HighEqGain = 41,
        ChorusType = 42, ChorusRate = 43, ChorusDepth = 44, ChorusBalance = 45,
        PartialMute = 46, PartialBalance = 47
    };
}
// Patch factor offsets (*4-6)
namespace patch
{
    enum : int
    {
        Name = 0, KeyMode = 18, SplitPoint = 19, PortamentoMode = 20, HoldMode = 21,
        UpperKeyShift = 22, LowerKeyShift = 23, UpperFineTune = 24, LowerFineTune = 25,
        BenderRange = 26, AftertouchBendRange = 27, PortamentoTime = 28, OutputMode = 29,
        ReverbType = 30, ReverbBalance = 31, TotalVolume = 32, ToneBalance = 33,
        ChaseMode = 34, ChaseLevel = 35, ChaseTime = 36,
        MidiTxChannel = 37, MidiSepRxChannel = 38, MidiTxProgramChange = 39
    };
}

// Lookup tables printed in the document
extern const float kLowEqFreqHz[16];   // common offset 37
extern const float kHighEqFreqHz[22];  // common offset 39
extern const float kHighEqQ[9];        // common offset 40
extern const float kPitchKeyfollow[17];// partial offset 2 (s1/s2 approximated as 1.0)
extern const float kTvfKeyfollow[15];  // partial offset 15

// Name characters (Tone Name / Patch Name value 0..63):  ' ', A-Z, a-z, 1-9, 0, -
char    decodeChar (uint8_t v) noexcept;
uint8_t encodeChar (char c) noexcept;

// ---- Checksum (4.1): (address + data + checksum) & 0x7F == 0 ---------------
uint8_t checksum (const uint8_t* bytes, size_t count) noexcept;

// ---- Parse / build ---------------------------------------------------------
enum class ParseResult { Ok, NotRoland, WrongModel, BadChecksum, Malformed, UnsupportedCommand };

struct Message
{
    uint8_t device = 0;
    uint8_t command = 0;
    uint32_t address = 0;     // linear
    uint32_t size = 0;        // RQ1 / RQD / WSD
    const uint8_t* data = nullptr; // DT1 / DAT payload (points into caller's buffer)
    int dataLength = 0;
};

// Accepts the message with or without the surrounding F0 ... F7.
ParseResult parse (const uint8_t* bytes, size_t length, Message& out) noexcept;

// Builds a full F0..F7 message into out (needs dataLength + 11 bytes). Returns bytes written.
size_t buildDT1 (uint8_t device, uint32_t address, const uint8_t* data, int dataLength, uint8_t* out) noexcept;
size_t buildRQ1 (uint8_t device, uint32_t address, uint32_t size, uint8_t* out) noexcept;

// The same Roland "type IV" one-way/handshake format is used by other models with 3-byte
// addresses (e.g. the JD-800, model 3DH). These take the model ID explicitly.
ParseResult parseForModel (uint8_t modelId, const uint8_t* bytes, size_t length, Message& out) noexcept;
size_t buildDT1ForModel (uint8_t modelId, uint8_t device, uint32_t address, const uint8_t* data, int dataLength, uint8_t* out) noexcept;
size_t buildRQ1ForModel (uint8_t modelId, uint8_t device, uint32_t address, uint32_t size, uint8_t* out) noexcept;

} // namespace d50
