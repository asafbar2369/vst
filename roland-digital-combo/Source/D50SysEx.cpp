#include "D50SysEx.h"

namespace d50
{
void toAddress (uint32_t linear, uint8_t out[3]) noexcept
{
    out[0] = uint8_t ((linear >> 14) & 0x7F);
    out[1] = uint8_t ((linear >> 7) & 0x7F);
    out[2] = uint8_t (linear & 0x7F);
}

Block blockForTempOffset (int offset) noexcept
{
    switch (offset / kBlockSize)
    {
        case 0:  return Block::UpperPartial1;
        case 1:  return Block::UpperPartial2;
        case 2:  return Block::UpperCommon;
        case 3:  return Block::LowerPartial1;
        case 4:  return Block::LowerPartial2;
        case 5:  return Block::LowerCommon;
        default: return Block::Patch;
    }
}

// *4-4 Each partial block consists of the following.
static const ParamInfo partialTable[64] = {
    { "WG Pitch Coarse", 72 }, { "WG Pitch Fine", 100 }, { "WG Pitch Keyfollow", 16 },
    { "WG Mod LFO Mode", 3 }, { "WG Mod P-ENV Mode", 2 }, { "WG Mod Bend Mode", 2 },
    { "WG Waveform", 1 }, { "WG PCM Wave No.", 99 }, { "WG Pulse Width", 100 },
    { "WG PW Velocity Range", 14 }, { "WG PW LFO Select", 5 }, { "WG PW LFO Depth", 100 },
    { "WG PW Aftertouch Range", 14 },
    { "TVF Cutoff Frequency", 100 }, { "TVF Resonance", 30 }, { "TVF Keyfollow", 14 },
    { "TVF Bias Point/Dir", 127 }, { "TVF Bias Level", 14 },
    { "TVF ENV Depth", 100 }, { "TVF ENV Velocity Range", 100 }, { "TVF ENV Depth Keyfollow", 4 },
    { "TVF ENV Time Keyfollow", 4 },
    { "TVF ENV Time 1", 100 }, { "TVF ENV Time 2", 100 }, { "TVF ENV Time 3", 100 },
    { "TVF ENV Time 4", 100 }, { "TVF ENV Time 5", 100 },
    { "TVF ENV Level 1", 100 }, { "TVF ENV Level 2", 100 }, { "TVF ENV Level 3", 100 },
    { "TVF ENV Sustain Level", 100 }, { "TVF ENV End Level", 1 },
    { "TVF Mod LFO Select", 5 }, { "TVF Mod LFO Depth", 100 }, { "TVF Mod Aftertouch Range", 14 },
    { "TVA Level", 100 }, { "TVA Velocity Range", 100 }, { "TVA Bias Point", 127 }, { "TVA Bias Level", 12 },
    { "TVA ENV Time 1", 100 }, { "TVA ENV Time 2", 100 }, { "TVA ENV Time 3", 100 },
    { "TVA ENV Time 4", 100 }, { "TVA ENV Time 5", 100 },
    { "TVA ENV Level 1", 100 }, { "TVA ENV Level 2", 100 }, { "TVA ENV Level 3", 100 },
    { "TVA ENV Sustain Level", 100 }, { "TVA ENV End Level", 1 },
    { "TVA ENV Velocity Follow", 4 }, { "TVA ENV Time Keyfollow", 4 },
    { "TVA Mod LFO Select", 5 }, { "TVA Mod LFO Depth", 100 }, { "TVA Mod Aftertouch Range", 14 },
    // 54..63 Extension (for future) 0-127
    { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 },
    { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 },
};

// *4-5 Each common block consists of the following.
static const ParamInfo commonTable[64] = {
    { "Tone Name 1", 63 }, { "Tone Name 2", 63 }, { "Tone Name 3", 63 }, { "Tone Name 4", 63 }, { "Tone Name 5", 63 },
    { "Tone Name 6", 63 }, { "Tone Name 7", 63 }, { "Tone Name 8", 63 }, { "Tone Name 9", 63 }, { "Tone Name 10", 63 },
    { "Structure No.", 6 }, { "P-ENV Velocity Range", 2 }, { "P-ENV Time Keyfollow", 4 },
    { "P-ENV Time 1", 50 }, { "P-ENV Time 2", 50 }, { "P-ENV Time 3", 50 }, { "P-ENV Time 4", 50 },
    { "P-ENV Level 0", 100 }, { "P-ENV Level 1", 100 }, { "P-ENV Level 2", 100 },
    { "P-ENV Sustain Level", 100 }, { "P-ENV End Level", 100 },
    { "P-Mod LFO Depth", 100 }, { "P-Mod Lever", 100 }, { "P-Mod After touch", 100 },
    { "LFO-1 Wave Form", 3 }, { "LFO-1 Rate", 100 }, { "LFO-1 Delay Time", 100 }, { "LFO-1 Sync", 2 },
    { "LFO-2 Wave Form", 3 }, { "LFO-2 Rate", 100 }, { "LFO-2 Delay Time", 100 }, { "LFO-2 Sync", 1 },
    { "LFO-3 Wave Form", 3 }, { "LFO-3 Rate", 100 }, { "LFO-3 Delay Time", 100 }, { "LFO-3 Sync", 1 },
    { "Low EQ Frequency", 15 }, { "Low EQ Gain", 24 }, { "High EQ Frequency", 21 }, { "High EQ Q", 8 },
    { "High EQ Gain", 24 },
    { "Chorus Type", 7 }, { "Chorus Rate", 100 }, { "Chorus Depth", 100 }, { "Chorus Balance", 100 },
    { "Partial Mute", 3 }, { "Partial Balance", 100 },
    // 48..63 Extension 0-127
    { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 },
    { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 },
    { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 },
    { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 }, { "Extension", 127 },
};

// *4-6 Each patch block consists of the following.
static const ParamInfo patchTable[64] = {
    { "Patch Name 1", 63 }, { "Patch Name 2", 63 }, { "Patch Name 3", 63 }, { "Patch Name 4", 63 },
    { "Patch Name 5", 63 }, { "Patch Name 6", 63 }, { "Patch Name 7", 63 }, { "Patch Name 8", 63 },
    { "Patch Name 9", 63 }, { "Patch Name 10", 63 }, { "Patch Name 11", 63 }, { "Patch Name 12", 63 },
    { "Patch Name 13", 63 }, { "Patch Name 14", 63 }, { "Patch Name 15", 63 }, { "Patch Name 16", 63 },
    { "Patch Name 17", 63 }, { "Patch Name 18", 63 },
    { "Key Mode", 8 }, { "Split Point", 60 }, { "Portamento Mode", 2 }, { "Hold Mode", 2 },
    { "Upper Tone Key Shift", 48 }, { "Lower Tone Key Shift", 48 },
    { "Upper Tone Fine Tune", 100 }, { "Lower Tone Fine Tune", 100 },
    { "Bender Range", 12 }, { "After touch Bend Range", 24 }, { "Portamento Time", 100 },
    { "Output Mode", 3 }, { "Reverb Type", 31 }, { "Reverb Balance", 100 }, { "Total Volume", 100 },
    { "Tone Balance", 100 }, { "Chase Mode", 2 }, { "Chase Level", 100 }, { "Chase Time", 100 },
    { "MIDI Transmit Channel", 16 }, { "MIDI Separate Rcv Channel", 16 }, { "MIDI Transmit Prog. Change", 100 },
    // 40..63 Extension (for future) 0-100
    { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 },
    { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 },
    { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 },
    { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 },
    { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 },
    { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 }, { "Extension", 100 },
};

const ParamInfo& partialParam (int o) noexcept { return partialTable[o & 63]; }
const ParamInfo& commonParam  (int o) noexcept { return commonTable[o & 63]; }
const ParamInfo& patchParam   (int o) noexcept { return patchTable[o & 63]; }

uint8_t maxValueForTempOffset (int offset) noexcept
{
    if (offset < 0 || offset >= kTempSize)
        return 0;
    const int local = offset % kBlockSize;
    switch (blockForTempOffset (offset))
    {
        case Block::UpperCommon:
        case Block::LowerCommon: return commonTable[local].maxValue;
        case Block::Patch:       return patchTable[local].maxValue;
        case Block::UpperPartial1:
        case Block::UpperPartial2:
        case Block::LowerPartial1:
        case Block::LowerPartial2:
        default:                 return partialTable[local].maxValue;
    }
}

// Common offset 37 "Low EQ Frequency" (63, 75, 88, 105, 125, 150, 175, 210, 250, 300, 350, 420, 500, 600, 700, 840)
const float kLowEqFreqHz[16] = { 63, 75, 88, 105, 125, 150, 175, 210, 250, 300, 350, 420, 500, 600, 700, 840 };
// Common offset 39 "High EQ Frequency" (250 ... 840, 1.0k ... 9.5k)
const float kHighEqFreqHz[22] = { 250, 300, 350, 420, 500, 600, 700, 840, 1000, 1200, 1400, 1700, 2000, 2400,
                                  2800, 3400, 4000, 4800, 5700, 6700, 8000, 9500 };
// Common offset 40 "High EQ Q" (0.3, 0.5, 0.7, 1.0, 1.4, 2.0, 3.0, 4.2, 6.0)
const float kHighEqQ[9] = { 0.3f, 0.5f, 0.7f, 1.0f, 1.4f, 2.0f, 3.0f, 4.2f, 6.0f };
// Partial offset 2 "WG Pitch Keyfollow" (-1, -1/2, -1/4, 0, 1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1, 5/4, 3/2, 2, s1, s2)
const float kPitchKeyfollow[17] = { -1.0f, -0.5f, -0.25f, 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f,
                                    1.0f, 1.25f, 1.5f, 2.0f, 1.0f, 1.0f };
// Partial offset 15 "TVF Keyfollow" (-1, -1/2, -1/4, 0, 1/8, 1/4, 3/8, 1/2, 5/8, 3/4, 7/8, 1, 5/4, 3/2, 2)
const float kTvfKeyfollow[15] = { -1.0f, -0.5f, -0.25f, 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f,
                                  1.0f, 1.25f, 1.5f, 2.0f };

char decodeChar (uint8_t v) noexcept
{
    v &= 63;
    if (v == 0)  return ' ';
    if (v <= 26) return char ('A' + (v - 1));
    if (v <= 52) return char ('a' + (v - 27));
    if (v <= 61) return char ('1' + (v - 53));
    if (v == 62) return '0';
    return '-';
}

uint8_t encodeChar (char c) noexcept
{
    if (c >= 'A' && c <= 'Z') return uint8_t (1 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return uint8_t (27 + (c - 'a'));
    if (c >= '1' && c <= '9') return uint8_t (53 + (c - '1'));
    if (c == '0') return 62;
    if (c == '-') return 63;
    return 0;
}

uint8_t checksum (const uint8_t* bytes, size_t count) noexcept
{
    unsigned sum = 0;
    for (size_t i = 0; i < count; ++i)
        sum += bytes[i] & 0x7Fu;
    return uint8_t ((128u - (sum & 0x7Fu)) & 0x7Fu);
}

ParseResult parse (const uint8_t* bytes, size_t length, Message& out) noexcept
{
    if (bytes == nullptr)
        return ParseResult::Malformed;

    // Strip F0 / F7 if present
    if (length > 0 && bytes[0] == kExclusiveStatus) { ++bytes; --length; }
    if (length > 0 && bytes[length - 1] == kEndOfExclusive) --length;

    if (length < 1)
        return ParseResult::Malformed;
    if (bytes[0] != kRolandId)
        return ParseResult::NotRoland;
    if (length < 4)
        return ParseResult::Malformed;
    if (bytes[2] != kModelId)
        return ParseResult::WrongModel;

    for (size_t i = 0; i < length; ++i)
        if (bytes[i] & 0x80)
            return ParseResult::Malformed;

    out = Message {};
    out.device = bytes[1];
    out.command = bytes[3];
    const uint8_t* body = bytes + 4;
    const size_t bodyLen = length - 4;

    switch (out.command)
    {
        case kRQ1:
        case kRQD:
        case kWSD:
        {
            // address(3) size(3) sum(1)
            if (bodyLen != 7)
                return ParseResult::Malformed;
            if (checksum (body, 6) != body[6])
                return ParseResult::BadChecksum;
            out.address = toLinear (body[0], body[1], body[2]);
            out.size = toLinear (body[3], body[4], body[5]);
            return ParseResult::Ok;
        }
        case kDT1:
        case kDAT:
        {
            // address(3) data(n>=1) sum(1)
            if (bodyLen < 5)
                return ParseResult::Malformed;
            const int n = int (bodyLen) - 4;
            if (n > kMaxDataBytesPerMessage)
                return ParseResult::Malformed;
            if (checksum (body, bodyLen - 1) != body[bodyLen - 1])
                return ParseResult::BadChecksum;
            out.address = toLinear (body[0], body[1], body[2]);
            out.data = body + 3;
            out.dataLength = n;
            return ParseResult::Ok;
        }
        case kACK: case kEOD: case kERR: case kRJC:
            return bodyLen == 0 ? ParseResult::Ok : ParseResult::Malformed;
        default:
            return ParseResult::UnsupportedCommand;
    }
}

size_t buildDT1 (uint8_t device, uint32_t address, const uint8_t* data, int dataLength, uint8_t* out) noexcept
{
    size_t i = 0;
    out[i++] = kExclusiveStatus;
    out[i++] = kRolandId;
    out[i++] = uint8_t (device & 0x0F);
    out[i++] = kModelId;
    out[i++] = kDT1;
    const size_t sumStart = i;
    toAddress (address, out + i);
    i += 3;
    for (int k = 0; k < dataLength; ++k)
        out[i++] = uint8_t (data[k] & 0x7F);
    out[i] = checksum (out + sumStart, i - sumStart);
    ++i;
    out[i++] = kEndOfExclusive;
    return i;
}

size_t buildRQ1 (uint8_t device, uint32_t address, uint32_t size, uint8_t* out) noexcept
{
    size_t i = 0;
    out[i++] = kExclusiveStatus;
    out[i++] = kRolandId;
    out[i++] = uint8_t (device & 0x0F);
    out[i++] = kModelId;
    out[i++] = kRQ1;
    const size_t sumStart = i;
    toAddress (address, out + i);
    i += 3;
    toAddress (size, out + i);
    i += 3;
    out[i] = checksum (out + sumStart, i - sumStart);
    ++i;
    out[i++] = kEndOfExclusive;
    return i;
}
} // namespace d50
