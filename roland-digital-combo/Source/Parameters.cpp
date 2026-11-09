#include "Parameters.h"
#include <cstring>

namespace params
{
juce::StringArray modeNames() { return { "JD-800", "D-50", "LAYER", "SPLIT" }; }

static juce::String noteName (int midiNote)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (n[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1); // C4 = 60
}
static juce::String signedText (int v) { return v > 0 ? "+" + juce::String (v) : juce::String (v); }

// ---------------------------------------------------------------------------
const std::vector<D50Binding>& d50Bindings()
{
    static const std::vector<D50Binding> list = []
    {
        using namespace d50;
        std::vector<D50Binding> b;
        struct P { int off; const char* suffix; };
        static const P partialParams[] = {
            { partial::Coarse, "coarse" }, { partial::Fine, "fine" }, { partial::Keyfollow, "keyfollow" },
            { partial::Waveform, "wave" }, { partial::PcmWave, "pcm" }, { partial::PulseWidth, "pw" },
            { partial::TvfCutoff, "cutoff" }, { partial::TvfResonance, "reso" }, { partial::TvfKeyfollow, "tvfKf" },
            { partial::TvfEnvDepth, "tvfDepth" }, { partial::TvfT1, "tvfT1" }, { partial::TvfT2, "tvfT2" },
            { partial::TvfSustain, "tvfSus" }, { partial::TvfT5, "tvfT5" },
            { partial::TvaLevel, "level" }, { partial::TvaVelocity, "velo" }, { partial::TvaT1, "tvaT1" },
            { partial::TvaT2, "tvaT2" }, { partial::TvaSustain, "tvaSus" }, { partial::TvaT5, "tvaT5" },
        };
        for (int p = 0; p < 2; ++p)
            for (auto& pp : partialParams)
            {
                const int base = p == 0 ? (int) kUpperPartial1 : (int) kUpperPartial2;
                b.push_back ({ "d50_up" + juce::String (p + 1) + "_" + pp.suffix,
                               "D-50 Upper P" + juce::String (p + 1) + " " + partialParam (pp.off).name,
                               base + pp.off });
            }
        static const P commonParams[] = {
            { common::Structure, "structure" }, { common::PModLever, "lever" }, { common::Lfo1Wave, "lfo1Wave" },
            { common::Lfo1Rate, "lfo1Rate" }, { common::LowEqGain, "lowEq" }, { common::HighEqGain, "highEq" },
            { common::ChorusType, "chorusType" }, { common::ChorusRate, "chorusRate" },
            { common::ChorusDepth, "chorusDepth" }, { common::ChorusBalance, "chorusBal" },
            { common::PartialMute, "mute" }, { common::PartialBalance, "balance" },
        };
        for (auto& cp : commonParams)
            b.push_back ({ juce::String ("d50_uc_") + cp.suffix,
                           juce::String ("D-50 Upper ") + commonParam (cp.off).name,
                           (int) kUpperCommon + cp.off });
        static const P patchParams[] = {
            { patch::KeyMode, "keyMode" }, { patch::SplitPoint, "split" }, { patch::UpperKeyShift, "upperShift" },
            { patch::LowerKeyShift, "lowerShift" }, { patch::BenderRange, "bender" }, { patch::ReverbType, "revType" },
            { patch::ReverbBalance, "revBal" }, { patch::TotalVolume, "volume" }, { patch::ToneBalance, "toneBal" },
        };
        for (auto& pp : patchParams)
            b.push_back ({ juce::String ("d50_pt_") + pp.suffix,
                           juce::String ("D-50 ") + patchParam (pp.off).name,
                           (int) kPatchFactors + pp.off });
        return b;
    }();
    return list;
}

juce::String formatD50Value (int offset, int v)
{
    using namespace d50;
    const int local = offset % kBlockSize;
    const auto block = blockForTempOffset (offset);
    // Value names as printed in "D-50 MIDI Implementation" *4-4 .. *4-6
    static const char* kf17[] = { "-1", "-1/2", "-1/4", "0", "1/8", "1/4", "3/8", "1/2", "5/8", "3/4", "7/8", "1",
                                  "5/4", "3/2", "2", "s1", "s2" };
    static const char* lfoSel[] = { "+1", "-1", "+2", "-2", "+3", "-3" };
    static const char* uLul[] = { "U", "L", "UL" };
    auto biasText = [] (int b) { return juce::String (b < 64 ? "<" : ">") + noteName (33 + (b & 63)); };
    auto noteChar = [] (int ch) { return "'" + juce::String::charToString (decodeChar ((uint8_t) ch)) + "'"; };
    auto offOr = [] (int x, const char* zero) { return x == 0 ? juce::String (zero) : juce::String (x); };

    if (block == Block::Patch)
    {
        static const char* keyModes[] = { "Whole", "Dual", "Split", "Separate", "Whole-S", "Dual-S",
                                           "Split-US", "Split-LS", "Separate-S" };
        static const char* chase[] = { "UL", "ULL", "ULU" };
        if (local <= 17) return noteChar (v);
        switch (local)
        {
            case patch::KeyMode: return keyModes[juce::jlimit (0, 8, v)];
            case patch::SplitPoint: return noteName (36 + v);
            case patch::PortamentoMode: case patch::HoldMode: return uLul[juce::jlimit (0, 2, v)];
            case patch::UpperKeyShift: case patch::LowerKeyShift: return signedText (v - 24);
            case patch::UpperFineTune: case patch::LowerFineTune: return signedText (v - 50);
            case patch::AftertouchBendRange: return signedText (v - 12);
            case patch::OutputMode: return juce::String (v + 1);
            case patch::ReverbType: return juce::String (v + 1);
            case patch::ChaseMode: return chase[juce::jlimit (0, 2, v)];
            case patch::MidiTxChannel: return v == 0 ? juce::String ("Basic") : juce::String (v);
            case patch::MidiSepRxChannel: return offOr (v, "OFF");
            case patch::MidiTxProgramChange: return offOr (v, "OFF");
            default: return juce::String (v);
        }
    }
    if (block == Block::UpperCommon || block == Block::LowerCommon)
    {
        static const char* lfoWaves[] = { "TRI", "SAW", "SQU", "RND" };
        static const char* structures[] = { "1 S+S", "2 S+RM", "3 P+S", "4 P+RM", "5 S+RM(P)", "6 P+P", "7 P+RM" };
        static const char* mutes[] = { "00", "01", "10", "11" };
        static const char* sync3[] = { "OFF", "ON", "KEY" };
        if (local <= 9) return noteChar (v);
        switch (local)
        {
            case common::Structure: return structures[juce::jlimit (0, 6, v)];
            case common::PEnvL0: case common::PEnvL1: case common::PEnvL2: case common::PEnvSustain: case common::PEnvEnd:
                return signedText (v - 50);
            case common::Lfo1Wave: case common::Lfo2Wave: case common::Lfo3Wave: return lfoWaves[juce::jlimit (0, 3, v)];
            case common::Lfo1Sync: return sync3[juce::jlimit (0, 2, v)];
            case common::Lfo2Sync: case common::Lfo3Sync: return sync3[juce::jlimit (0, 1, v)];
            case common::LowEqFreq: return juce::String ((int) kLowEqFreqHz[juce::jlimit (0, 15, v)]) + " Hz";
            case common::HighEqFreq:
            {
                const float hz = kHighEqFreqHz[juce::jlimit (0, 21, v)];
                return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String ((int) hz) + " Hz";
            }
            case common::HighEqQ: return juce::String (kHighEqQ[juce::jlimit (0, 8, v)], 1);
            case common::LowEqGain: case common::HighEqGain: return signedText (v - 12) + " dB";
            case common::ChorusType: return juce::String (v + 1);
            case common::PartialMute: return mutes[juce::jlimit (0, 3, v)];
            default: return juce::String (v);
        }
    }
    switch (local)
    {
        case partial::Coarse: return noteName (24 + v);  // 0..72 = C1..C7, 36 = C4
        case partial::Fine: return signedText (v - 50);
        case partial::Keyfollow: return kf17[juce::jlimit (0, 16, v)];
        case partial::TvfKeyfollow: return kf17[juce::jlimit (0, 14, v)];
        case partial::LfoMode: { static const char* m[] = { "OFF", "(+)", "(-)", "A&L" }; return m[juce::jlimit (0, 3, v)]; }
        case partial::PEnvMode: { static const char* m[] = { "OFF", "(+)", "(-)" }; return m[juce::jlimit (0, 2, v)]; }
        case partial::BendMode: { static const char* m[] = { "OFF", "Keyfollow", "Normal" }; return m[juce::jlimit (0, 2, v)]; }
        case partial::Waveform: return v == 0 ? "SQU" : "SAW";
        case partial::PcmWave: return juce::String (v + 1) + " " + kPcmDisplay[juce::jlimit (0, 99, v)] + " *";
        case partial::PwVelocity: case partial::PwAftertouch: case partial::TvfBiasLevel:
        case partial::TvfAftertouch: case partial::TvaAftertouch:
            return signedText (v - 7);
        case partial::PwLfoSelect: case partial::TvfLfoSelect: case partial::TvaLfoSelect: return lfoSel[juce::jlimit (0, 5, v)];
        case partial::TvfBiasPoint: case partial::TvaBiasPoint: return biasText (v);
        case partial::TvfEnd: case partial::TvaEnd: return v == 0 ? "0" : "100";
        case partial::TvaVelocity: return signedText (v - 50);
        case partial::TvaBiasLevel: return signedText (v - 12);
        default: return juce::String (v);
    }
}

// ---------------------------------------------------------------------------
static void writeName (uint8_t* dst, int len, const char* text)
{
    for (int i = 0; i < len; ++i)
        dst[i] = (text != nullptr && i < (int) std::strlen (text)) ? d50::encodeChar (text[i]) : 0;
}

void writeInitD50Tone (uint8_t* raw) noexcept
{
    using namespace d50;
    std::fill (raw, raw + kTempSize, uint8_t (0));

    auto writePartial = [] (uint8_t* p, int waveform, int fine, int level)
    {
        using namespace partial;
        p[Coarse] = 36; p[Fine] = (uint8_t) fine; p[Keyfollow] = 11; p[LfoMode] = 1; p[PEnvMode] = 0; p[BendMode] = 2;
        p[Waveform] = (uint8_t) waveform; p[PcmWave] = 7; p[PulseWidth] = 30; p[PwVelocity] = 7; p[PwLfoSelect] = 0;
        p[PwLfoDepth] = 0; p[PwAftertouch] = 7;
        p[TvfCutoff] = 60; p[TvfResonance] = 5; p[TvfKeyfollow] = 7; p[TvfBiasPoint] = 64; p[TvfBiasLevel] = 7;
        p[TvfEnvDepth] = 40; p[TvfEnvVelocity] = 50; p[TvfEnvDepthKf] = 0; p[TvfEnvTimeKf] = 0;
        const uint8_t tvfT[5] = { 10, 40, 50, 50, 40 }, tvfL[3] = { 100, 70, 60 };
        for (int i = 0; i < 5; ++i) p[TvfT1 + i] = tvfT[i];
        for (int i = 0; i < 3; ++i) p[TvfL1 + i] = tvfL[i];
        p[TvfSustain] = 50; p[TvfEnd] = 0; p[TvfLfoSelect] = 0; p[TvfLfoDepth] = 0; p[TvfAftertouch] = 7;
        p[TvaLevel] = (uint8_t) level; p[TvaVelocity] = 70; p[TvaBiasPoint] = 64; p[TvaBiasLevel] = 12;
        const uint8_t tvaT[5] = { 5, 30, 40, 50, 45 }, tvaL[3] = { 100, 90, 85 };
        for (int i = 0; i < 5; ++i) p[TvaT1 + i] = tvaT[i];
        for (int i = 0; i < 3; ++i) p[TvaL1 + i] = tvaL[i];
        p[TvaSustain] = 80; p[TvaEnd] = 0; p[TvaVelocityFollow] = 0; p[TvaTimeKf] = 0;
        p[TvaLfoSelect] = 0; p[TvaLfoDepth] = 0; p[TvaAftertouch] = 7;
    };
    auto writeCommon = [] (uint8_t* c, const char* name, int structure)
    {
        using namespace common;
        writeName (c + Name, 10, name);
        c[Structure] = (uint8_t) structure; c[PEnvVelocity] = 0; c[PEnvTimeKf] = 0;
        c[PEnvT1] = 0; c[PEnvT2] = 10; c[PEnvT3] = 10; c[PEnvT4] = 20;
        for (int i = PEnvL0; i <= PEnvEnd; ++i) c[i] = 50;
        c[PModLfoDepth] = 0; c[PModLever] = 30; c[PModAftertouch] = 0;
        c[Lfo1Wave] = 0; c[Lfo1Rate] = 55; c[Lfo1Delay] = 30; c[Lfo1Sync] = 0;
        c[Lfo2Wave] = 0; c[Lfo2Rate] = 40; c[Lfo2Delay] = 0; c[Lfo2Sync] = 0;
        c[Lfo3Wave] = 0; c[Lfo3Rate] = 30; c[Lfo3Delay] = 0; c[Lfo3Sync] = 0;
        c[LowEqFreq] = 4; c[LowEqGain] = 12; c[HighEqFreq] = 14; c[HighEqQ] = 3; c[HighEqGain] = 12;
        c[ChorusType] = 0; c[ChorusRate] = 40; c[ChorusDepth] = 50; c[ChorusBalance] = 40;
        c[PartialMute] = 3; c[PartialBalance] = 50;
    };

    writePartial (raw + kUpperPartial1, 1, 50, 90);
    writePartial (raw + kUpperPartial2, 0, 53, 70);
    writeCommon (raw + kUpperCommon, "INIT UPPER", 0);
    writePartial (raw + kLowerPartial1, 1, 50, 90);
    writePartial (raw + kLowerPartial2, 0, 47, 80);
    writeCommon (raw + kLowerCommon, "INIT LOWER", 2);

    using namespace patch;
    uint8_t* p = raw + kPatchFactors;
    writeName (p + Name, 18, "COMBO INIT");
    p[KeyMode] = 0; p[SplitPoint] = 24; p[PortamentoMode] = 0; p[HoldMode] = 2;
    p[UpperKeyShift] = 24; p[LowerKeyShift] = 24; p[UpperFineTune] = 50; p[LowerFineTune] = 50;
    p[BenderRange] = 2; p[AftertouchBendRange] = 12; p[PortamentoTime] = 0; p[OutputMode] = 0;
    p[ReverbType] = 2; p[ReverbBalance] = 30; p[TotalVolume] = 85; p[ToneBalance] = 50;
    p[ChaseMode] = 0; p[ChaseLevel] = 50; p[ChaseTime] = 50;
    p[MidiTxChannel] = 0; p[MidiSepRxChannel] = 0; p[MidiTxProgramChange] = 0;
}

// ---------------------------------------------------------------------------
// JD-800
static void writeAscii (uint8_t* dst, int len, const char* text)
{
    const int n = text != nullptr ? (int) std::strlen (text) : 0;
    for (int i = 0; i < len; ++i)
        dst[i] = (uint8_t) (i < n ? juce::jlimit (32, 127, (int) text[i]) : 32);
}

void writeInitJdPatch (uint8_t* raw) noexcept
{
    using namespace jd800;
    std::fill (raw, raw + kPatchSize, uint8_t (0));
    uint8_t* c = raw + kCommonOffset;
    writeAscii (c + common::Name, 16, "COMBO JD INIT");
    c[common::Level] = 80;
    for (int t = 0; t < 4; ++t) { c[common::KeyRangeLowA + 2 * t] = 0; c[common::KeyRangeHighA + 2 * t] = 127; }
    c[common::BenderDown] = 2; c[common::BenderUp] = 2; c[common::AftertouchBend] = 14; // 0 semitones
    c[common::SoloSw] = 0; c[common::SoloLegato] = 0; c[common::PortamentoSw] = 0; c[common::PortamentoMode] = 0;
    c[common::PortamentoTime] = 30; c[common::LayerTone] = 0x03; c[common::ActiveTone] = 0x01;
    c[common::LowFreq] = 0; c[common::LowGain] = 15; c[common::MidFreq] = 7; c[common::MidQ] = 1; c[common::MidGain] = 15;
    c[common::HighFreq] = 0; c[common::HighGain] = 15;
    c[common::KeyMode] = 0; c[common::SplitPoint] = 36; c[common::HoldMode] = 2;

    uint8_t* e = raw + kEffectOffset;
    e[effect::GroupASequence] = 0; e[effect::GroupBSequence] = 0;
    for (int i = 0; i < 4; ++i) e[effect::GroupABlock1 + i] = 0;
    for (int i = 0; i < 3; ++i) e[effect::GroupBBlock1 + i] = 1;
    e[effect::GroupBBalance] = 50;
    e[effect::DistType] = 1; e[effect::DistDrive] = 30; e[effect::DistLevel] = 60;
    e[effect::PhaserManual] = 30; e[effect::PhaserRate] = 4; e[effect::PhaserDepth] = 50; e[effect::PhaserResonance] = 30;
    e[effect::PhaserMix] = 50;
    for (int i = 0; i < 6; ++i) e[effect::SpectrumBand1 + i] = 15;
    e[effect::SpectrumWidth] = 2; e[effect::EnhancerSens] = 50; e[effect::EnhancerMix] = 30;
    e[effect::DelayCenterTap] = 110; e[effect::DelayCenterLevel] = 25; e[effect::DelayLeftTap] = 100;
    e[effect::DelayLeftLevel] = 15; e[effect::DelayRightTap] = 115; e[effect::DelayRightLevel] = 15; e[effect::DelayFeedback] = 64;
    e[effect::ChorusRate] = 4; e[effect::ChorusDepth] = 50; e[effect::ChorusDelay] = 69; e[effect::ChorusFeedback] = 49;
    e[effect::ChorusLevel] = 50;
    e[effect::ReverbType] = 2; e[effect::ReverbPreDelay] = 10; e[effect::ReverbEarlyRef] = 30; e[effect::ReverbHfDamp] = 11;
    e[effect::ReverbTime] = 30; e[effect::ReverbLevel] = 35;

    static const int waves[4] = { 0, 5, 62, 98 };      // Syn Saw 1, Syn Pulse2, Digi Bell, Pink Noise
    static const int coarse[4] = { 48, 36, 60, 48 };
    static const int fine[4] = { 50, 53, 50, 50 };
    for (int t = 0; t < 4; ++t)
    {
        using namespace tone;
        uint8_t* p = raw + kToneOffset[t];
        p[VelocityCurve] = 1; p[HoldControl] = 1;
        const uint8_t lfo1[6] = { 55, 30, 70, 0, 1, 0 }, lfo2[6] = { 30, 0, 50, 0, 1, 0 };
        for (int i = 0; i < 6; ++i) { p[Lfo1Rate + i] = lfo1[i]; p[Lfo2Rate + i] = lfo2[i]; }
        p[WaveSource] = 0; p[WaveMsb] = (uint8_t) (waves[t] >> 7); p[WaveLsb] = (uint8_t) (waves[t] & 127);
        p[PitchCoarse] = (uint8_t) coarse[t]; p[PitchFine] = (uint8_t) fine[t]; p[PitchRandom] = 0; p[PitchKeyFollow] = 12;
        p[BenderSw] = 1; p[AftertouchBendSw] = 0; p[Lfo1Sens] = 50; p[Lfo2Sens] = 50; p[LeverSens] = 58; p[AftertouchModSens] = 50;
        p[PEnvVelo] = 50; p[PEnvTimeVelo] = 50; p[PEnvTimeKf] = 10; p[PEnvL0] = 50; p[PEnvT1] = 0; p[PEnvL1] = 50;
        p[PEnvT2] = 0; p[PEnvT3] = 0; p[PEnvL2] = 50;
        p[FilterMode] = 2; p[Cutoff] = 70; p[Resonance] = 15; p[CutoffKf] = 20; p[TvfAftertouch] = 50; p[TvfLfoSelect] = 0;
        p[TvfLfoDepth] = 50; p[TvfEnvDepth] = 65;
        p[TvfEnvVelo] = 60; p[TvfEnvTimeVelo] = 50; p[TvfEnvTimeKf] = 10; p[TvfT1] = 10; p[TvfL1] = 100; p[TvfT2] = 50;
        p[TvfL2] = 60; p[TvfT3] = 50; p[TvfSustain] = 50; p[TvfT4] = 50; p[TvfL4] = 0;
        p[BiasDirection] = 0; p[BiasPoint] = 60; p[BiasLevel] = 10; p[Level] = (uint8_t) (t == 0 ? 90 : 75);
        p[TvaAftertouch] = 50; p[TvaLfoSelect] = 0; p[TvaLfoDepth] = 50;
        p[TvaEnvVelo] = 70; p[TvaEnvTimeVelo] = 50; p[TvaEnvTimeKf] = 10; p[TvaT1] = 5; p[TvaL1] = 100; p[TvaT2] = 50;
        p[TvaL2] = 90; p[TvaT3] = 40; p[TvaSustain] = 80; p[TvaT4] = 55;
    }
}

const juce::StringArray& jdToneSuffixes()
{
    static const juce::StringArray s { "on", "wave", "coarse", "fine", "filterMode", "cutoff", "reso", "cutoffKf",
                                       "envDepth", "fT1", "fT2", "fSus", "fT4", "level", "aVelo", "aT1", "aT2", "aSus",
                                       "aT4", "lfo1Rate", "lfo1Sens" };
    return s;
}

juce::String jdToneId (int tone, const juce::String& suffix)
{
    return "jd_" + juce::String::charToString (juce::juce_wchar ('a' + tone)) + "_" + suffix;
}

const std::vector<JdBinding>& jdBindings()
{
    static const std::vector<JdBinding> list = []
    {
        using namespace jd800;
        std::vector<JdBinding> b;
        struct P { int off; const char* suffix; };
        static const P toneBytes[] = {
            { tone::PitchCoarse, "coarse" }, { tone::PitchFine, "fine" }, { tone::FilterMode, "filterMode" },
            { tone::Cutoff, "cutoff" }, { tone::Resonance, "reso" }, { tone::CutoffKf, "cutoffKf" },
            { tone::TvfEnvDepth, "envDepth" }, { tone::TvfT1, "fT1" }, { tone::TvfT2, "fT2" }, { tone::TvfSustain, "fSus" },
            { tone::TvfT4, "fT4" }, { tone::Level, "level" }, { tone::TvaEnvVelo, "aVelo" }, { tone::TvaT1, "aT1" },
            { tone::TvaT2, "aT2" }, { tone::TvaSustain, "aSus" }, { tone::TvaT4, "aT4" }, { tone::Lfo1Rate, "lfo1Rate" },
            { tone::Lfo1Sens, "lfo1Sens" },
        };
        for (int t = 0; t < 4; ++t)
        {
            const juce::String pre = "JD Tone " + juce::String::charToString (juce::juce_wchar ('A' + t)) + " ";
            b.push_back ({ jdToneId (t, "on"), pre + "On (Layer)", kCommonOffset + common::LayerTone, JdBinding::LayerBit, t, 0, 1 });
            b.push_back ({ jdToneId (t, "wave"), pre + "Waveform", kToneOffset[t] + tone::WaveMsb, JdBinding::Waveform, 0, 0, kNumWaveforms - 1 });
            for (auto& p : toneBytes)
            {
                const auto& info = toneParam (p.off);
                b.push_back ({ jdToneId (t, p.suffix), pre + info.name, kToneOffset[t] + p.off, JdBinding::Byte, 0, info.minValue, info.maxValue });
            }
        }
        static const P commonBytes[] = {
            { common::Level, "jd_patchLevel" }, { common::BenderUp, "jd_benderUp" }, { common::BenderDown, "jd_benderDown" },
            { common::SoloSw, "jd_solo" }, { common::PortamentoSw, "jd_portaSw" }, { common::PortamentoTime, "jd_portaTime" },
            { common::LowGain, "jd_lowGain" }, { common::MidGain, "jd_midGain" }, { common::HighGain, "jd_highGain" },
        };
        for (auto& p : commonBytes)
        {
            const auto& info = commonParam (p.off);
            b.push_back ({ p.suffix, juce::String ("JD ") + info.name, kCommonOffset + p.off, JdBinding::Byte, 0, info.minValue, info.maxValue });
        }
        static const P effectBytes[] = {
            { effect::GroupASequence, "jd_seqA" }, { effect::GroupABlock1, "jd_aBlock1" }, { effect::GroupABlock1 + 1, "jd_aBlock2" },
            { effect::GroupABlock1 + 2, "jd_aBlock3" }, { effect::GroupABlock1 + 3, "jd_aBlock4" },
            { effect::GroupBSequence, "jd_seqB" }, { effect::GroupBBlock1, "jd_bBlock1" }, { effect::GroupBBlock1 + 1, "jd_bBlock2" },
            { effect::GroupBBlock1 + 2, "jd_bBlock3" }, { effect::GroupBBalance, "jd_bBalance" },
            { effect::DistType, "jd_distType" }, { effect::DistDrive, "jd_distDrive" }, { effect::PhaserRate, "jd_phRate" },
            { effect::PhaserDepth, "jd_phDepth" }, { effect::ChorusLevel, "jd_chorusLevel" }, { effect::DelayCenterLevel, "jd_delayLevel" },
            { effect::DelayFeedback, "jd_delayFeedback" }, { effect::ReverbType, "jd_revType" }, { effect::ReverbTime, "jd_revTime" },
            { effect::ReverbLevel, "jd_revLevel" },
        };
        for (auto& p : effectBytes)
        {
            const auto& info = effectParam (p.off);
            b.push_back ({ p.suffix, juce::String ("JD ") + info.name, kEffectOffset + p.off, JdBinding::Byte, 0, info.minValue, info.maxValue });
        }
        return b;
    }();
    return list;
}

juce::StringArray mixerIds()
{
    using namespace id;
    return { d50Level, d50Pan, d50Transpose, d50KeyLow, d50KeyHigh, jdLevel, jdPan, jdTranspose, jdKeyLow, jdKeyHigh };
}

const juce::StringArray& jdCommonIds()
{
    static const juce::StringArray s = []
    {
        juce::StringArray r;
        for (auto& b : jdBindings())
            if (jd800::blockForOffset (b.offset) == jd800::Block::Common && b.kind == JdBinding::Byte) r.add (b.id);
        return r;
    }();
    return s;
}

const juce::StringArray& jdEffectIds()
{
    static const juce::StringArray s = []
    {
        juce::StringArray r;
        for (auto& b : jdBindings())
            if (jd800::blockForOffset (b.offset) == jd800::Block::Effect) r.add (b.id);
        return r;
    }();
    return s;
}

juce::String formatJdValue (int offset, int v)
{
    using namespace jd800;
    const int l = localOffset (offset);
    auto onOff = [v] { return juce::String (v ? "ON" : "OFF"); };
    auto sgn = [] (int x) { return signedText (x); };
    static const char* masks[16] = { "none", "A", "B", "AB", "C", "AC", "BC", "ABC", "D", "AD", "BD", "ABD", "CD", "ACD", "BCD", "ABCD" };
    auto hz = [] (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "kHz"
                            : juce::String ((int) f) + "Hz";
    };
    auto ms = [] (float m) { return (m < 10.0f ? juce::String (m, 1) : juce::String ((int) m)) + "ms"; };

    switch (blockForOffset (offset))
    {
        case Block::Common:
            if (l < common::Level) return "'" + juce::String::charToString ((juce::juce_wchar) juce::jlimit (32, 127, v)) + "'";
            switch (l)
            {
                case common::BenderDown: case common::BenderUp: return juce::String (v);
                case common::AftertouchBend: return sgn (aftertouchBendSemis (v));
                case common::SoloSw: case common::SoloLegato: case common::PortamentoSw: return onOff();
                case common::PortamentoMode: return v ? "LEGATO" : "NORMAL";
                case common::LayerTone: case common::ActiveTone: return masks[v & 15];
                case common::LowFreq: return v ? "400Hz" : "200Hz";
                case common::LowGain: case common::MidGain: case common::HighGain: return sgn (v - 15) + "dB";
                case common::MidFreq: return hz (kMidEqFreqHz[juce::jlimit (0, 16, v)]);
                case common::MidQ: return juce::String (kMidEqQ[juce::jlimit (0, 4, v)], 1);
                case common::HighFreq: return v ? "8kHz" : "4kHz";
                case common::KeyMode: { static const char* k[] = { "WHOLE", "SPLIT", "DUAL" }; return k[juce::jlimit (0, 2, v)]; }
                case common::SplitPoint: return noteName (24 + v);
                case common::LowerChannel: case common::UpperChannel: case common::LowerProgram: case common::UpperProgram:
                    return juce::String (v + 1);
                case common::HoldMode: { static const char* h[] = { "UPPER", "LOWER", "BOTH" }; return h[juce::jlimit (0, 2, v)]; }
                default:
                    if (l >= common::KeyRangeLowA && l <= common::KeyRangeHighA + 6) return noteName (v);
                    return juce::String (v);
            }
        case Block::Effect:
            switch (l)
            {
                case effect::GroupASequence: return kGroupASequences[juce::jlimit (0, 23, v)];
                case effect::GroupBSequence: return kGroupBSequences[juce::jlimit (0, 5, v)];
                case effect::GroupBBalance: return juce::String (100 - v) + ":" + juce::String (v);
                case effect::DistType: return kDistortionTypes[juce::jlimit (0, 6, v)];
                case effect::PhaserManual: return hz (phaserManualHz (v));
                case effect::PhaserRate: case effect::ChorusRate: return juce::String (rate01to10Hz (v), 1) + "Hz";
                case effect::SpectrumWidth: return juce::String (v + 1);
                case effect::DelayCenterTap: case effect::DelayLeftTap: case effect::DelayRightTap: return ms (delayTapMs (v));
                case effect::DelayFeedback: case effect::ChorusFeedback: return sgn (feedbackPercent (v)) + "%";
                case effect::ChorusDelay: return ms (chorusDelayMs (v));
                case effect::ReverbType: return kReverbTypes[juce::jlimit (0, 9, v)];
                case effect::ReverbPreDelay: return juce::String (v) + "ms";
                case effect::ReverbHfDamp: return v >= 16 ? juce::String ("BYPASS") : hz (kHfDampHz[juce::jlimit (0, 15, v)]);
                default:
                    if (l >= effect::GroupABlock1 && l <= effect::GroupBBlock1 + 2) return onOff();
                    if (l >= effect::SpectrumBand1 && l < effect::SpectrumWidth) return sgn (v - 15) + "dB";
                    return juce::String (v);
            }
        case Block::ToneA: case Block::ToneB: case Block::ToneC: case Block::ToneD:
        default:
        {
            const int lfoLocal = (l >= tone::Lfo1Rate && l < tone::WaveSource) ? (l - tone::Lfo1Rate) % 6 : -1;
            if (lfoLocal == 1) return v >= 101 ? juce::String ("REL") : juce::String (v);
            if (lfoLocal == 2) return sgn (v - 50);
            if (lfoLocal == 3) { static const char* w[] = { "TRI", "SAW", "SQU", "S/H", "RND" }; return w[juce::jlimit (0, 4, v)]; }
            if (lfoLocal == 4) { static const char* o[] = { "+", "0", "-" }; return o[juce::jlimit (0, 2, v)]; }
            if (lfoLocal == 5) return onOff();
            switch (l)
            {
                case tone::VelocityCurve: return juce::String (v + 1);
                case tone::HoldControl: case tone::BenderSw: case tone::AftertouchBendSw: return onOff();
                case tone::WaveSource: return v ? "CRD" : "INT";
                case tone::PitchCoarse: return sgn (v - 48);
                case tone::PitchFine: return sgn (v - 50);
                case tone::PitchKeyFollow: return sgn (kWgKeyFollowPercent[juce::jlimit (0, 16, v)]) + "%";
                case tone::Lfo1Sens: case tone::Lfo2Sens: case tone::PEnvVelo: case tone::PEnvTimeVelo: case tone::PEnvL0:
                case tone::PEnvL1: case tone::PEnvL2: case tone::TvfAftertouch: case tone::TvfLfoDepth: case tone::TvfEnvDepth:
                case tone::TvfEnvVelo: case tone::TvfEnvTimeVelo: case tone::TvaAftertouch: case tone::TvaLfoDepth:
                case tone::TvaEnvVelo: case tone::TvaEnvTimeVelo:
                    return sgn (v - 50);
                case tone::LeverSens: case tone::AftertouchModSens:
                    return v == 50 ? juce::String ("0") : (v > 50 ? "LFO1 " + juce::String (v - 50) : "LFO2 " + juce::String (50 - v));
                case tone::PEnvTimeKf: case tone::TvfEnvTimeKf: case tone::TvaEnvTimeKf: case tone::BiasLevel: return sgn (v - 10);
                case tone::FilterMode: { static const char* m[] = { "HPF", "BPF", "LPF" }; return m[juce::jlimit (0, 2, v)]; }
                case tone::CutoffKf: return sgn (tvfKeyFollowPercent (v)) + "%";
                case tone::TvfLfoSelect: case tone::TvaLfoSelect: return v ? "LFO2" : "LFO1";
                case tone::BiasDirection: { static const char* d[] = { "UP", "LOW", "U&L" }; return d[juce::jlimit (0, 2, v)]; }
                case tone::BiasPoint: return noteName (v);
                default: return juce::String (v);
            }
        }
    }
}

static juce::String jdWaveText (int v)
{
    return juce::String (v + 1).paddedLeft ('0', 3) + " " + jd800::kWaveNames[juce::jlimit (0, jd800::kNumWaveforms - 1, v)] + " *";
}

// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    constexpr int version = 1;

    // Switches are two-option choice parameters: JUCE's bool parameter keeps the raw float a host
    // writes (e.g. 0.54), which then fails state-restore checks in pluginval.
    auto switchParam = [&] (const String& pid, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, version }, name, StringArray { "Off", "On" }, def ? 1 : 0));
    };
    auto intParam = [&] (const String& pid, const String& name, int lo, int hi, int def,
                         std::function<String (int, int)> fmt = nullptr)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid, version }, name, lo, hi, def,
                                                         AudioParameterIntAttributes().withStringFromValueFunction (fmt)));
    };

    // ---- Global ----
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::mode, version }, "Play Mode", modeNames(), ModeLayer));
    intParam (id::splitPoint, "Split Point", 0, 127, 60, [] (int v, int) { return noteName (v); });
    switchParam (id::splitSwap, "Split: JD-800 Lower", false);
    intParam (id::layerBalance, "D-50/JD-800 Balance", 0, 100, 50);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::masterVolume, version }, "Master Volume",
                                                       NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -6.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    switchParam (id::sysexRx, "SysEx Receive (D-50 + JD-800)", true);
    intParam (id::sysexChannel, "D-50 SysEx Channel", 1, 16, 1);
    intParam (id::jdUnit, "JD-800 Unit Number", jd800::kFirstUnit, jd800::kLastUnit, jd800::kFirstUnit);

    // ---- Per-engine mixer and keyboard zones (all play modes) ----
    auto panText = [] (int v, int) { return v == 0 ? String ("C") : (v < 0 ? "L" + String (-v) : "R" + String (v)); };
    auto noteText = [] (int v, int) { return noteName (v); };
    for (int e = 0; e < 2; ++e)
    {
        const String pre = e == 0 ? "D-50 " : "JD-800 ";
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { e == 0 ? id::d50Level : id::jdLevel, version }, pre + "Engine Level",
                                                           NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes().withLabel ("dB")));
        intParam (e == 0 ? id::d50Pan : id::jdPan, pre + "Engine Pan", -50, 50, 0, panText);
        intParam (e == 0 ? id::d50Transpose : id::jdTranspose, pre + "Engine Transpose", -24, 24, 0,
                  [] (int v, int) { return signedText (v); });
        intParam (e == 0 ? id::d50KeyLow : id::jdKeyLow, pre + "Zone Low Key", 0, 127, 0, noteText);
        intParam (e == 0 ? id::d50KeyHigh : id::jdKeyHigh, pre + "Zone High Key", 0, 127, 127, noteText);
    }

    // ---- D-50 (stored in the temporary area) ----
    uint8_t init[d50::kTempSize];
    writeInitD50Tone (init);
    for (auto& b : d50Bindings())
    {
        const int off = b.tempOffset;
        intParam (b.id, b.name, 0, d50::maxValueForTempOffset (off), init[off],
                  [off] (int v, int) { return formatD50Value (off, v); });
    }

    // ---- JD-800 (stored in the patch temporary area) ----
    uint8_t jd[jd800::kPatchSize];
    writeInitJdPatch (jd);
    for (auto& b : jdBindings())
    {
        const int off = b.offset;
        switch (b.kind)
        {
            case JdBinding::LayerBit:
                switchParam (b.id, b.name, ((jd[off] >> b.bit) & 1) != 0);
                break;
            case JdBinding::Waveform:
                intParam (b.id, b.name, b.minValue, b.maxValue, (jd[off] << 7) | jd[off + 1], [] (int v, int) { return jdWaveText (v); });
                break;
            case JdBinding::Byte:
            default:
                intParam (b.id, b.name, b.minValue, b.maxValue, jd[off], [off] (int v, int) { return formatJdValue (off, v); });
                break;
        }
    }
    return layout;
}

// ---------------------------------------------------------------------------
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> p;
        auto jt = [] (int t, const char* s) { return jdToneId (t, s); };
        p.push_back ({ "Init Combo (Layer)", {} });
        p.push_back ({ "Glass Choir Layer", {
            { id::mode, ModeLayer }, { "d50_uc_structure", 2 }, { "d50_up1_pcm", 12 }, { "d50_up2_cutoff", 55 },
            { "d50_uc_chorusType", 1 }, { "d50_uc_chorusBal", 70 }, { "d50_pt_revBal", 55 },
            { jt (0, "wave"), 15 }, { jt (0, "aT1"), 45 }, { jt (0, "aT4"), 70 },
            { jt (1, "wave"), 11 }, { jt (1, "coarse"), 60 }, { "jd_revLevel", 60 } } });
        p.push_back ({ "Split Bass / Lead", {
            { id::mode, ModeSplit }, { id::splitPoint, 55 },
            { "d50_up1_coarse", 24 }, { "d50_up2_coarse", 24 }, { "d50_up1_cutoff", 45 }, { "d50_up1_reso", 18 },
            { "d50_up1_tvfDepth", 70 }, { "d50_uc_chorusBal", 0 }, { "d50_pt_revBal", 5 },
            { jt (0, "wave"), 0 }, { jt (1, "wave"), 6 }, { jt (0, "reso"), 40 }, { "jd_delayLevel", 40 } } });
        p.push_back ({ "LA Bell Pad (D-50)", {
            { id::mode, ModeD50 }, { "d50_uc_structure", 1 }, { "d50_up2_coarse", 55 },
            { "d50_up1_tvaT1", 45 }, { "d50_up1_tvaT5", 70 }, { "d50_up2_tvaSus", 20 }, { "d50_up2_tvaT2", 60 },
            { "d50_uc_chorusType", 1 }, { "d50_uc_chorusBal", 80 }, { "d50_pt_revType", 6 }, { "d50_pt_revBal", 60 } } });
        p.push_back ({ "Super Strings (JD-800)", {
            { id::mode, ModeJD800 }, { jt (0, "wave"), 2 }, { jt (0, "aT1"), 40 }, { jt (0, "aT4"), 65 }, { jt (0, "cutoff"), 72 },
            { jt (1, "wave"), 9 }, { jt (1, "coarse"), 48 }, { jt (1, "aT1"), 45 }, { "jd_chorusLevel", 70 }, { "jd_revLevel", 50 } } });
        p.push_back ({ "Ring Mod Bells (D-50)", {
            { id::mode, ModeD50 }, { "d50_uc_structure", 1 }, { "d50_up2_coarse", 43 }, { "d50_up2_fine", 57 },
            { "d50_up1_tvaSus", 0 }, { "d50_up2_tvaSus", 0 }, { "d50_up1_tvaT2", 70 }, { "d50_up2_tvaT2", 65 },
            { "d50_up1_cutoff", 85 }, { "d50_up2_cutoff", 85 }, { "d50_pt_revBal", 50 } } });
        p.push_back ({ "Fat Brass Layer", {
            { id::mode, ModeLayer }, { "d50_up1_tvfT1", 35 }, { "d50_up1_tvfDepth", 60 }, { "d50_up2_wave", 1 },
            { jt (0, "wave"), 0 }, { jt (0, "fT1"), 30 }, { jt (0, "envDepth"), 68 }, { jt (1, "wave"), 2 } } });
        p.push_back ({ "FM Piano / Pad Split", {
            { id::mode, ModeSplit }, { id::splitSwap, 1 }, { id::splitPoint, 60 },
            { jt (0, "wave"), 71 }, { jt (0, "aSus"), 0 }, { jt (0, "aT2"), 75 }, { jt (1, "on"), 0 },
            { "d50_up1_tvaT1", 50 }, { "d50_up2_tvaT1", 55 }, { "d50_pt_revBal", 50 } } });
        p.push_back ({ "Digital Bells (JD-800)", {
            { id::mode, ModeJD800 }, { jt (0, "wave"), 62 }, { jt (0, "aSus"), 0 }, { jt (0, "aT2"), 70 },
            { jt (1, "wave"), 66 }, { jt (1, "coarse"), 60 }, { jt (1, "aSus"), 0 }, { jt (1, "aT2"), 65 },
            { "jd_revType", 4 }, { "jd_revLevel", 60 } } });
        p.push_back ({ "Drive Lead (JD-800, solo)", {
            { id::mode, ModeJD800 }, { "jd_solo", 1 }, { "jd_portaSw", 1 }, { "jd_portaTime", 35 },
            { jt (0, "wave"), 0 }, { jt (1, "wave"), 7 }, { jt (1, "fine"), 56 }, { jt (0, "reso"), 35 },
            { "jd_aBlock1", 1 }, { "jd_distType", 4 }, { "jd_distDrive", 55 }, { "jd_delayLevel", 35 } } });
        return p;
    }();
    return list;
}
} // namespace params
