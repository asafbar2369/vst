#include "JD800SysEx.h"

namespace jd800
{
Block blockForOffset (int o) noexcept
{
    if (o < kEffectOffset) return Block::Common;
    if (o < kToneOffset[0]) return Block::Effect;
    if (o < kToneOffset[1]) return Block::ToneA;
    if (o < kToneOffset[2]) return Block::ToneB;
    if (o < kToneOffset[3]) return Block::ToneC;
    return Block::ToneD;
}

int localOffset (int o) noexcept
{
    switch (blockForOffset (o))
    {
        case Block::Common: return o - kCommonOffset;
        case Block::Effect: return o - kEffectOffset;
        case Block::ToneA:  return o - kToneOffset[0];
        case Block::ToneB:  return o - kToneOffset[1];
        case Block::ToneC:  return o - kToneOffset[2];
        case Block::ToneD:
        default:            return o - kToneOffset[3];
    }
}

// *4-5-1-1 Patch Common
static const ParamInfo commonTable[kCommonSize] = {
    { "Patch name 1", 32, 127 }, { "Patch name 2", 32, 127 }, { "Patch name 3", 32, 127 }, { "Patch name 4", 32, 127 },
    { "Patch name 5", 32, 127 }, { "Patch name 6", 32, 127 }, { "Patch name 7", 32, 127 }, { "Patch name 8", 32, 127 },
    { "Patch name 9", 32, 127 }, { "Patch name 10", 32, 127 }, { "Patch name 11", 32, 127 }, { "Patch name 12", 32, 127 },
    { "Patch name 13", 32, 127 }, { "Patch name 14", 32, 127 }, { "Patch name 15", 32, 127 }, { "Patch name 16", 32, 127 },
    { "Patch level", 0, 100 },
    { "Key range L (Tone A)", 0, 127 }, { "Key range H (Tone A)", 0, 127 },
    { "Key range L (Tone B)", 0, 127 }, { "Key range H (Tone B)", 0, 127 },
    { "Key range L (Tone C)", 0, 127 }, { "Key range H (Tone C)", 0, 127 },
    { "Key range L (Tone D)", 0, 127 }, { "Key range H (Tone D)", 0, 127 },
    { "Bender range down", 0, 48 }, { "Bender range up", 0, 12 }, { "A-touch bend", 0, 26 },
    { "Solo SW", 0, 1 }, { "Solo Legato", 0, 1 }, { "Portamento SW", 0, 1 }, { "Portamento mode", 0, 1 },
    { "Portamento time", 0, 100 }, { "Layer tone", 0, 15 }, { "Active tone", 0, 15 },
    { "Low freq", 0, 1 }, { "Low gain", 0, 30 }, { "Mid freq", 0, 16 }, { "Mid Q", 0, 4 }, { "Mid gain", 0, 30 },
    { "High freq", 0, 1 }, { "High gain", 0, 30 },
    { "Key mode", 0, 2 }, { "Split point", 0, 85 }, { "Lower channel", 0, 15 }, { "Upper channel", 0, 15 },
    { "Lower program change", 0, 127 }, { "Upper program change", 0, 127 }, { "Hold mode", 0, 2 }, { "<dummy>", 0, 0 },
};

// *4-5-1-2 Patch Effect
static const ParamInfo effectTable[kEffectSize] = {
    { "Group-A sequence", 0, 23 }, { "Group-B sequence", 0, 5 },
    { "Group-A block-1 sw", 0, 1 }, { "Group-A block-2 sw", 0, 1 }, { "Group-A block-3 sw", 0, 1 }, { "Group-A block-4 sw", 0, 1 },
    { "Group-B block-1 sw", 0, 1 }, { "Group-B block-2 sw", 0, 1 }, { "Group-B block-3 sw", 0, 1 },
    { "Group-B effect balance", 0, 100 },
    { "Distortion Type", 0, 6 }, { "Distortion Drive", 0, 100 }, { "Distortion Level", 0, 100 },
    { "Phaser Manual", 0, 99 }, { "Phaser Rate", 0, 99 }, { "Phaser Depth", 0, 100 }, { "Phaser Resonance", 0, 100 },
    { "Phaser Mix", 0, 100 },
    { "Spectrum Band 1", 0, 30 }, { "Spectrum Band 2", 0, 30 }, { "Spectrum Band 3", 0, 30 }, { "Spectrum Band 4", 0, 30 },
    { "Spectrum Band 5", 0, 30 }, { "Spectrum Band 6", 0, 30 }, { "Spectrum Band width", 0, 4 },
    { "Enhancer Sens", 0, 100 }, { "Enhancer Mix", 0, 100 },
    { "Delay Center tap", 0, 125 }, { "Delay Center level", 0, 100 }, { "Delay Left tap", 0, 125 },
    { "Delay Left level", 0, 100 }, { "Delay Right tap", 0, 125 }, { "Delay Right level", 0, 100 },
    { "Delay Feedback", 0, 98 },
    { "Chorus Rate", 0, 99 }, { "Chorus Depth", 0, 100 }, { "Chorus Delay time", 0, 99 }, { "Chorus Feedback", 0, 98 },
    { "Chorus Level", 0, 100 },
    { "Reverb Type", 0, 9 }, { "Reverb Pre delay", 0, 120 }, { "Reverb Early ref level", 0, 100 },
    { "Reverb HF damp", 0, 16 }, { "Reverb Time", 0, 100 }, { "Reverb Level", 0, 100 }, { "<dummy>", 0, 0 },
};

// *4-5-1-3 Patch Tone
static const ParamInfo toneTable[kToneSize] = {
    { "Velocity curve", 0, 3 }, { "Hold control", 0, 1 },
    { "LFO 1 Rate", 0, 100 }, { "LFO 1 Delay", 0, 101 }, { "LFO 1 Fade", 0, 100 }, { "LFO 1 Waveform", 0, 4 },
    { "LFO 1 Offset", 0, 2 }, { "LFO 1 Key trigger", 0, 1 },
    { "LFO 2 Rate", 0, 100 }, { "LFO 2 Delay", 0, 101 }, { "LFO 2 Fade", 0, 100 }, { "LFO 2 Waveform", 0, 4 },
    { "LFO 2 Offset", 0, 2 }, { "LFO 2 Key trigger", 0, 1 },
    { "WG Wave source", 0, 1 }, { "WG Waveform MSB", 0, 1 }, { "WG Waveform LSB", 0, 127 },
    { "WG Pitch coarse", 0, 96 }, { "WG Pitch fine", 0, 100 }, { "WG Pitch random", 0, 100 }, { "WG Key follow", 0, 16 },
    { "WG Bender", 0, 1 }, { "WG A-touch bend", 0, 1 }, { "WG LFO1 sens", 0, 100 }, { "WG LFO2 sens", 0, 100 },
    { "WG Lever sens", 0, 100 }, { "WG A-touch mod sens", 0, 100 },
    { "Pitch ENV Velo", 0, 100 }, { "Pitch ENV Time velo", 0, 100 }, { "Pitch ENV Time KF", 0, 20 },
    { "Pitch ENV Level 0", 0, 100 }, { "Pitch ENV Time 1", 0, 100 }, { "Pitch ENV Level 1", 0, 100 },
    { "Pitch ENV Time 2", 0, 100 }, { "Pitch ENV Time 3", 0, 100 }, { "Pitch ENV Level 2", 0, 100 },
    { "TVF Filter mode", 0, 2 }, { "TVF Cutoff freq", 0, 100 }, { "TVF Resonance", 0, 100 }, { "TVF Key follow", 0, 40 },
    { "TVF A-touch sens", 0, 100 }, { "TVF LFO select", 0, 1 }, { "TVF LFO depth", 0, 100 }, { "TVF ENV depth", 0, 100 },
    { "TVF ENV Velo", 0, 100 }, { "TVF ENV Time velo", 0, 100 }, { "TVF ENV Time KF", 0, 20 },
    { "TVF ENV Time 1", 0, 100 }, { "TVF ENV Level 1", 0, 100 }, { "TVF ENV Time 2", 0, 100 }, { "TVF ENV Level 2", 0, 100 },
    { "TVF ENV Time 3", 0, 100 }, { "TVF ENV Sustain level", 0, 100 }, { "TVF ENV Time 4", 0, 100 },
    { "TVF ENV Level 4", 0, 100 },
    { "TVA Bias direction", 0, 2 }, { "TVA Bias point", 0, 127 }, { "TVA Bias level", 0, 20 }, { "TVA Level", 0, 100 },
    { "TVA A-touch sens", 0, 100 }, { "TVA LFO select", 0, 1 }, { "TVA LFO depth", 0, 100 },
    { "TVA ENV Velo", 0, 100 }, { "TVA ENV Time velo", 0, 100 }, { "TVA ENV Time KF", 0, 20 },
    { "TVA ENV Time 1", 0, 100 }, { "TVA ENV Level 1", 0, 100 }, { "TVA ENV Time 2", 0, 100 }, { "TVA ENV Level 2", 0, 100 },
    { "TVA ENV Time 3", 0, 100 }, { "TVA ENV Sustain level", 0, 100 }, { "TVA ENV Time 4", 0, 100 },
};

const ParamInfo& commonParam (int l) noexcept { return commonTable[l < 0 ? 0 : (l >= kCommonSize ? kCommonSize - 1 : l)]; }
const ParamInfo& effectParam (int l) noexcept { return effectTable[l < 0 ? 0 : (l >= kEffectSize ? kEffectSize - 1 : l)]; }
const ParamInfo& toneParam (int l) noexcept   { return toneTable[l < 0 ? 0 : (l >= kToneSize ? kToneSize - 1 : l)]; }

const ParamInfo& paramForOffset (int o) noexcept
{
    const int l = localOffset (o);
    switch (blockForOffset (o))
    {
        case Block::Common: return commonParam (l);
        case Block::Effect: return effectParam (l);
        case Block::ToneA: case Block::ToneB: case Block::ToneC: case Block::ToneD:
        default:            return toneParam (l);
    }
}

// [*2] of *4-5-1-1 and *4-2-1
const float kMidEqFreqHz[17] = { 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000 };
const float kMidEqQ[5] = { 0.5f, 1.0f, 2.0f, 4.0f, 9.0f };
// [*8] HF damp: 500 .. 16k, BYPASS
const float kHfDampHz[17] = { 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 0 };
// Tone [*2] WG key follow
const int kWgKeyFollowPercent[17] = { -100, -50, -20, -10, -5, 0, 5, 10, 20, 50, 98, 99, 100, 101, 102, 150, 200 };

const char* const kGroupASequences[24] = {
    "DS-PH-SP-EN", "DS-PH-EN-SP", "DS-SP-EN-PH", "DS-SP-PH-EN", "DS-EN-PH-SP", "DS-EN-SP-PH",
    "PH-DS-SP-EN", "PH-DS-EN-SP", "PH-SP-EN-DS", "PH-SP-DS-EN", "PH-EN-DS-SP", "PH-EN-SP-DS",
    "SP-PH-DS-EN", "SP-PH-EN-DS", "SP-DS-EN-PH", "SP-DS-PH-EN", "SP-EN-PH-DS", "SP-EN-DS-PH",
    "EN-PH-SP-DS", "EN-PH-DS-SP", "EN-SP-DS-PH", "EN-SP-PH-DS", "EN-DS-PH-SP", "EN-DS-SP-PH",
};
const char* const kGroupBSequences[6] = { "CHO-DLY-REV", "CHO-REV-DLY", "DLY-CHO-REV", "DLY-REV-CHO", "REV-CHO-DLY", "REV-DLY-CHO" };
const char* const kReverbTypes[10] = { "ROOM1", "ROOM2", "HALL1", "HALL2", "HALL3", "HALL4", "GATE", "REVERSE", "FLYING1", "FLYING2" };
const char* const kDistortionTypes[7] = { "MELLOW DRIVE", "OVERDRIVE", "CRY DRIVE", "MELLOW DIST", "LIGHT DIST", "FAT DIST", "FUZZ DIST" };

// WAVEFORM LIST (V-34 .. V-36)
const char* const kWaveNames[kNumWaveforms] = {
    "Syn Saw 1", "Syn Saw 2", "FAT Saw", "FAT Square", "Syn Pulse1", "Syn Pulse2", "Syn Pulse3", "Syn Pulse4",
    "Syn Pulse5", "Pulse Mod", "Triangle", "Syn Sine", "Soft Pad", "Wire Str", "MIDI Clav", "Spark Vox1",
    "Spark Vox2", "Syn Sax", "Clav Wave", "Cello Wave", "BrightDigi", "Cutters", "Syn Bass", "Rad Hose",
    "Vocal Wave", "Wally Wave", "Brusky Ip", "Digiwave", "Can Wave 1", "Can Wave 2", "EML 5th", "Wave Scan",
    "Nasty", "Wave Table", "Fine Wine", "Funk Bass1", "Funk Bass2", "Strat Sust", "Harp Harm", "Full Organ",
    "Full Draw", "Doo", "ZZZ Voz", "Org Vox", "Male Vox", "Kalimba", "Xylo", "Marim Wave",
    "Log Drum", "AgogoBells", "Bottle Hit", "Gamelan 1", "Gamelan 2", "Gamelan 3", "Table", "Pole Ip",
    "Pluck Harp", "Nylon Str", "Hooky", "Muters", "Klack Wave", "Crystal", "Digi Bell", "FingerBell",
    "Digi Chime", "Bell Wave", "Org Bell", "Scrape Gut", "Strat Atk", "Hellow Bs", "Piano Atk", "EP Hard",
    "Clear Keys", "EP Distone", "Flute Push", "Shami", "Wood Crak", "Klmba Atk", "Block", "Org Atk 1",
    "Org Atk 2", "Cowbell", "Sm Metal", "StrikePole", "Pizz", "Switch", "Tuba Slap", "Plink",
    "Plunk", "EP Atk", "TVF_Trig", "Flute Tone", "Pan Pipe", "BottleBlow", "Shaku Atk", "FlugelWave",
    "French", "WhiteNoise", "Pink Noise", "Pitch Wind", "Vox Noise1", "Vox Noise2", "CrunchWind", "ThroatWind",
    "Metal Wind", "Windago", "Anklungs", "Wind Chime",
};
const char kWaveLoopType[kNumWaveforms] = {
    'S','S','S','S','S','S','S','S','S','L','S','S','L','S','L','S','S','S','S','S','S','S','S','S','S','S','S','S',
    'S','S','S','S','S','S','L','S','S','S','S','S','S','S','L','L','L','S','S','S','S','S','S','S','S','S','S','L',
    'S','S','S','S','S','S','S','L','S','S','S','S','S','S','S','S','S','S','O','O','O','O','O','O','O','O','O','O',
    'O','O','O','O','O','O','O','S','L','L','L','S','S','S','S','L','L','L','L','L','L','L','L','L',
};

int tvfKeyFollowPercent (int v) noexcept
{
    // [*3]: -100 .. 0 % in 10 % steps, 0 .. +150 % in 5 % steps (41 values)
    if (v < 10) return -100 + 10 * v;
    return (v - 10) * 5;
}

int aftertouchBendSemis (int v) noexcept
{
    if (v <= 0) return -36;
    if (v == 1) return -24;
    return v - 14; // 2 -> -12 .. 26 -> +12
}

float phaserManualHz (int v) noexcept
{
    // [*4]: 50-300 Hz (10 Hz step), 320 Hz, 350-1010 Hz (30 Hz step), 1.1k-8.1 kHz (0.2 kHz step), 8.5k-15 kHz (0.5 kHz step)
    if (v < 26) return 50.0f + 10.0f * float (v);
    if (v == 26) return 320.0f;
    if (v < 50) return 350.0f + 30.0f * float (v - 27);
    if (v < 86) return 1100.0f + 200.0f * float (v - 50);
    return 8500.0f + 500.0f * float (v - 86);
}

float rate01to10Hz (int v) noexcept { return 0.1f * float (v + 1); }

float delayTapMs (int v) noexcept
{
    // [*5]: 0.1-5 ms (0.1), 5.5-10 ms (0.5), 11-40 ms (1), 50-200 ms (10), 220-600 ms (20)
    if (v < 50) return 0.1f * float (v + 1);
    if (v < 60) return 5.5f + 0.5f * float (v - 50);
    if (v < 90) return 11.0f + float (v - 60);
    if (v < 106) return 50.0f + 10.0f * float (v - 90);
    return 220.0f + 20.0f * float (v - 106);
}

float chorusDelayMs (int v) noexcept
{
    // [*6]: 0.1-5 ms (0.1), 5.5-10 ms (0.5), 11-50 ms (1)
    if (v < 50) return 0.1f * float (v + 1);
    if (v < 60) return 5.5f + 0.5f * float (v - 50);
    return 11.0f + float (v - 60);
}

int feedbackPercent (int v) noexcept { return (v - 49) * 2; }

d50::ParseResult parse (const uint8_t* bytes, size_t length, d50::Message& out) noexcept
{
    return d50::parseForModel (kModelId, bytes, length, out);
}
size_t buildDT1 (uint8_t device, uint32_t address, const uint8_t* data, int dataLength, uint8_t* out) noexcept
{
    return d50::buildDT1ForModel (kModelId, device, address, data, dataLength, out);
}
size_t buildRQ1 (uint8_t device, uint32_t address, uint32_t size, uint8_t* out) noexcept
{
    return d50::buildRQ1ForModel (kModelId, device, address, size, out);
}
} // namespace jd800
