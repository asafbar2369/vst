#include "Parameters.h"

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
        case partial::PcmWave: return "#" + juce::String (v + 1) + " (subst.)";
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
const juce::StringArray& jdToneSuffixes()
{
    static const juce::StringArray s { "on", "wave", "coarse", "fine", "level", "pan", "filterMode", "cutoff",
                                       "reso", "envDepth", "fA", "fD", "fS", "fR", "aA", "aD", "aS", "aR", "vel" };
    return s;
}

juce::String jdToneId (int tone, const juce::String& suffix)
{
    return "jd_" + juce::String::charToString (juce::juce_wchar ('a' + tone)) + "_" + suffix;
}

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
    switchParam (id::sysexRx, "D-50 SysEx Receive", true);
    intParam (id::sysexChannel, "D-50 SysEx Channel", 1, 16, 1);

    // ---- D-50 (stored in the temporary area) ----
    uint8_t init[d50::kTempSize];
    writeInitD50Tone (init);
    for (auto& b : d50Bindings())
    {
        const int off = b.tempOffset;
        intParam (b.id, b.name, 0, d50::maxValueForTempOffset (off), init[off],
                  [off] (int v, int) { return formatD50Value (off, v); });
    }

    // ---- JD-800 tones A-D ----
    StringArray waveNames;
    for (int w = 0; w < (int) dc::JdWave::Count; ++w) waveNames.add (dc::jdWaveName (w));
    static const int defWave[4] = { (int) dc::JdWave::SuperSaw, (int) dc::JdWave::Square, (int) dc::JdWave::FmBell, (int) dc::JdWave::Noise };
    for (int t = 0; t < 4; ++t)
    {
        const String letter = String::charToString (juce_wchar ('A' + t));
        const String pre = "JD Tone " + letter + " ";
        switchParam (jdToneId (t, "on"), pre + "On", t < 2);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { jdToneId (t, "wave"), version }, pre + "Wave", waveNames, defWave[t]));
        intParam (jdToneId (t, "coarse"), pre + "Coarse", -24, 24, t == 1 ? -12 : 0, [] (int v, int) { return signedText (v); });
        intParam (jdToneId (t, "fine"), pre + "Fine", -50, 50, t == 1 ? 4 : 0, [] (int v, int) { return signedText (v); });
        intParam (jdToneId (t, "level"), pre + "Level", 0, 100, t == 0 ? 80 : 60);
        intParam (jdToneId (t, "pan"), pre + "Pan", -50, 50, t == 0 ? -15 : (t == 1 ? 15 : 0), [] (int v, int) { return v == 0 ? String ("C") : (v < 0 ? "L" + String (-v) : "R" + String (v)); });
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { jdToneId (t, "filterMode"), version }, pre + "Filter Mode", StringArray { "LPF", "BPF", "HPF" }, 0));
        intParam (jdToneId (t, "cutoff"), pre + "Cutoff", 0, 100, 65);
        intParam (jdToneId (t, "reso"), pre + "Resonance", 0, 100, 15);
        intParam (jdToneId (t, "envDepth"), pre + "Filter Env Depth", -50, 50, 20, [] (int v, int) { return signedText (v); });
        intParam (jdToneId (t, "fA"), pre + "Filter Attack", 0, 100, 10);
        intParam (jdToneId (t, "fD"), pre + "Filter Decay", 0, 100, 55);
        intParam (jdToneId (t, "fS"), pre + "Filter Sustain", 0, 100, 40);
        intParam (jdToneId (t, "fR"), pre + "Filter Release", 0, 100, 50);
        intParam (jdToneId (t, "aA"), pre + "Amp Attack", 0, 100, 5);
        intParam (jdToneId (t, "aD"), pre + "Amp Decay", 0, 100, 60);
        intParam (jdToneId (t, "aS"), pre + "Amp Sustain", 0, 100, 80);
        intParam (jdToneId (t, "aR"), pre + "Amp Release", 0, 100, 55);
        intParam (jdToneId (t, "vel"), pre + "Velocity Sens", 0, 100, 50);
    }

    // ---- JD-800 common / effects ----
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::jdLfoRate, version }, "JD LFO Rate",
                                                       NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.4f), 5.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")));
    intParam (id::jdLfoDepth, "JD LFO Pitch Depth (Mod Wheel)", 0, 100, 30);
    intParam (id::jdBend, "JD Bend Range", 0, 12, 2);
    intParam (id::jdChorusMix, "JD Chorus Mix", 0, 100, 35);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id::jdChorusRate, version }, "JD Chorus Rate",
                                                       NormalisableRange<float> (0.05f, 5.0f, 0.01f, 0.5f), 0.6f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")));
    intParam (id::jdDelayTime, "JD Delay Time", 1, 1000, 350, [] (int v, int) { return String (v) + " ms"; });
    intParam (id::jdDelayFb, "JD Delay Feedback", 0, 95, 30);
    intParam (id::jdDelayMix, "JD Delay Mix", 0, 100, 0);
    intParam (id::jdRevSize, "JD Reverb Size", 0, 100, 50);
    intParam (id::jdRevMix, "JD Reverb Mix", 0, 100, 20);
    intParam (id::jdVolume, "JD Volume", 0, 100, 80);
    return layout;
}

// ---------------------------------------------------------------------------
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = []
    {
        std::vector<Preset> p;
        p.push_back ({ "Init Combo (Layer)", {} });
        p.push_back ({ "Glass Choir Layer", {
            { id::mode, ModeLayer }, { "d50_uc_structure", 2 }, { "d50_up1_pcm", 12 }, { "d50_up2_cutoff", 55 },
            { "d50_uc_chorusType", 1 }, { "d50_uc_chorusBal", 70 }, { "d50_pt_revBal", 55 },
            { jdToneId (0, "wave"), (float) dc::JdWave::Vox }, { jdToneId (0, "aA"), 45 }, { jdToneId (0, "aR"), 70 },
            { jdToneId (1, "wave"), (float) dc::JdWave::Sine }, { jdToneId (1, "coarse"), 12 }, { id::jdRevMix, 45 } } });
        p.push_back ({ "Split Bass / Lead", {
            { id::mode, ModeSplit }, { id::splitPoint, 55 },
            { "d50_up1_coarse", 24 }, { "d50_up2_coarse", 24 }, { "d50_up1_cutoff", 45 }, { "d50_up1_reso", 18 },
            { "d50_up1_tvfDepth", 70 }, { "d50_uc_chorusBal", 0 }, { "d50_pt_revBal", 5 },
            { jdToneId (0, "wave"), (float) dc::JdWave::Saw }, { jdToneId (1, "wave"), (float) dc::JdWave::Pulse25 },
            { jdToneId (0, "reso"), 40 }, { id::jdDelayMix, 30 } } });
        p.push_back ({ "LA Bell Pad (D-50)", {
            { id::mode, ModeD50 }, { "d50_uc_structure", 1 }, { "d50_up2_coarse", 55 },
            { "d50_up1_tvaT1", 45 }, { "d50_up1_tvaT5", 70 }, { "d50_up2_tvaSus", 20 }, { "d50_up2_tvaT2", 60 },
            { "d50_uc_chorusType", 1 }, { "d50_uc_chorusBal", 80 }, { "d50_pt_revType", 6 }, { "d50_pt_revBal", 60 } } });
        p.push_back ({ "Super Strings (JD-800)", {
            { id::mode, ModeJD800 }, { jdToneId (0, "wave"), (float) dc::JdWave::SuperSaw }, { jdToneId (0, "aA"), 40 },
            { jdToneId (0, "aR"), 65 }, { jdToneId (0, "cutoff"), 70 },
            { jdToneId (1, "wave"), (float) dc::JdWave::Pwm }, { jdToneId (1, "coarse"), 0 }, { jdToneId (1, "aA"), 45 },
            { id::jdChorusMix, 60 }, { id::jdRevMix, 40 } } });
        p.push_back ({ "Ring Mod Bells (D-50)", {
            { id::mode, ModeD50 }, { "d50_uc_structure", 1 }, { "d50_up2_coarse", 43 }, { "d50_up2_fine", 57 },
            { "d50_up1_tvaSus", 0 }, { "d50_up2_tvaSus", 0 }, { "d50_up1_tvaT2", 70 }, { "d50_up2_tvaT2", 65 },
            { "d50_up1_cutoff", 85 }, { "d50_up2_cutoff", 85 }, { "d50_pt_revBal", 50 } } });
        p.push_back ({ "Fat Brass Layer", {
            { id::mode, ModeLayer }, { "d50_up1_tvfT1", 35 }, { "d50_up1_tvfDepth", 60 }, { "d50_up2_wave", 1 },
            { jdToneId (0, "wave"), (float) dc::JdWave::Saw }, { jdToneId (0, "fA"), 30 }, { jdToneId (0, "envDepth"), 35 },
            { jdToneId (1, "wave"), (float) dc::JdWave::SubSaw } } });
        p.push_back ({ "FM Piano / Pad Split", {
            { id::mode, ModeSplit }, { id::splitSwap, 1 }, { id::splitPoint, 60 },
            { jdToneId (0, "wave"), (float) dc::JdWave::FmPiano }, { jdToneId (0, "aS"), 0 }, { jdToneId (0, "aD"), 75 },
            { jdToneId (1, "on"), 0 }, { "d50_up1_tvaT1", 50 }, { "d50_up2_tvaT1", 55 }, { "d50_pt_revBal", 50 } } });
        return p;
    }();
    return list;
}
} // namespace params
