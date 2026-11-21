#pragma once

#include "../Core/DspUtils.h"
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/**
    Six-operator FM voice in the style of the classic 1983 six-operator FM architecture (clean-room implementation).

    Written from the publicly documented architecture and MIDI / SysEx data format only: 6 sine operators,
    32 algorithms, operator feedback, ratio / fixed frequency, 4-rate / 4-level envelopes, keyboard level and rate
    scaling, velocity sensitivity, a pitch envelope and an LFO. No code or data from other FM implementations and no
    factory ROM content are included; the factory FM voices here are original. The level, rate and modulation-index
    curves are this project's own approximations and are NOT claimed to be identical to the hardware.

    Operator numbering follows the hardware convention: OP1 ... OP6 (op[0] ... op[5]); SysEx data stores OP6 first.
*/
namespace vsx::fm
{
constexpr int kOps = 6;
constexpr int kAlgorithms = 32;
constexpr int kVcedSize = 155;     // single voice, unpacked
constexpr int kPackedSize = 128;   // one voice in a 32-voice bulk dump
constexpr int kBankVoices = 32;

struct Operator
{
    std::array<uint8_t, 4> rate { 99, 99, 99, 99 }, level { 99, 99, 99, 0 };
    uint8_t breakPoint = 39;            // 0-99, 39 = C3 (MIDI 60)
    uint8_t leftDepth = 0, rightDepth = 0; // 0-99
    uint8_t leftCurve = 0, rightCurve = 0; // 0 -LIN, 1 -EXP, 2 +EXP, 3 +LIN
    uint8_t rateScale = 0;              // 0-7
    uint8_t ampModSens = 0;             // 0-3
    uint8_t velSens = 0;                // 0-7
    uint8_t outputLevel = 0;            // 0-99
    uint8_t fixedMode = 0;              // 0 ratio, 1 fixed
    uint8_t coarse = 1;                 // 0-31
    uint8_t fine = 0;                   // 0-99
    uint8_t detune = 7;                 // 0-14, 7 = centre
    bool operator== (const Operator&) const = default;
};

struct Patch
{
    std::array<Operator, kOps> op {};   // op[0] = OP1
    std::array<uint8_t, 4> pitchRate { 99, 99, 99, 99 }, pitchLevel { 50, 50, 50, 50 };
    uint8_t algorithm = 0;              // 0-31 (displayed 1-32)
    uint8_t feedback = 0;               // 0-7
    uint8_t oscSync = 1;
    uint8_t lfoSpeed = 35, lfoDelay = 0, lfoPmd = 0, lfoAmd = 0, lfoSync = 1, lfoWave = 0, lfoPms = 3; // wave: 0 tri, 1 saw down, 2 saw up, 3 square, 4 sine, 5 s/h
    uint8_t transpose = 24;             // 0-48, 24 = C3 (no shift)
    std::array<char, 10> name { 'I', 'N', 'I', 'T', ' ', 'V', 'O', 'I', 'C', 'E' };
    uint8_t opEnabled = 0x3F;           // bit i = OP(i+1) on (mute / solo in the editor)

    juce::String getName() const;
    void setName (const juce::String& n);
    /** Clamps every field to its legal range (used after parsing untrusted data). Returns the number of fields changed. */
    int clamp();
    bool operator== (const Patch&) const = default;

    std::array<uint8_t, kVcedSize> toVced() const;
    static Patch fromVced (const uint8_t* data);
    std::array<uint8_t, kPackedSize> toPacked() const;
    static Patch fromPacked (const uint8_t* data);

    /** Compact text form used in presets / project state: hex of VCED + enable mask. */
    juce::String toHex() const;
    static bool fromHex (const juce::String& hex, Patch& out);

    /** The basic starting voice: OP1 sine carrier at full level, other operators silent, algorithm 1. */
    static Patch init();
};

struct Algorithm
{
    std::array<uint8_t, kOps> modulators {}; // bit j set in modulators[i] = OP(j+1) modulates OP(i+1)
    uint8_t carriers = 0;                    // bit i = OP(i+1) is a carrier
    int feedbackSource = 5, feedbackTarget = 5; // 0-based; equal = self feedback
    int numCarriers() const;
};

const Algorithm& algorithm (int index);

// ---- SysEx ---------------------------------------------------------------------------------
struct SysexResult
{
    bool ok = false;
    std::vector<Patch> voices;
    juce::String error, warning;
    bool checksumOk = true;
    bool isBank = false;
};

/** Parses a .syx file: a 32-voice bulk dump (format 9) or a single voice (format 0). Several messages in one file
    are accepted (all voices are collected). Raw 4096-byte bank data without a header is accepted with a warning.
    A checksum mismatch is reported in `warning` and `checksumOk` (voices are still returned so the user can decide). */
SysexResult parseSysex (const juce::MemoryBlock& data);
SysexResult loadSysexFile (const juce::File& f);
juce::MemoryBlock makeBankSysex (const std::vector<Patch>& voices); // pads / truncates to 32
juce::MemoryBlock makeVoiceSysex (const Patch& p);
uint8_t checksum (const uint8_t* data, int size);

/** Amp-envelope release (s) long enough for the voice's own carrier release to finish (0.05 - 12 s). */
float releaseSeconds (const Patch& voice);

/** Original factory FM voices (not derived from any ROM). */
const std::vector<Patch>& factoryVoices();

// ---- per-voice renderer --------------------------------------------------------------------
class FmRenderer
{
public:
    void prepare (double sampleRate);
    void noteOn (const Patch& p, int note, int velocity, float pitchSemis, uint32_t seed);
    void noteOff() noexcept;
    /** Call once per control block before rendering (pitch in absolute MIDI semitones, amount scales modulation). */
    void setPitch (float pitchSemis, float fmAmount) noexcept { basePitch = pitchSemis; amount = fmAmount; }
    void setPatch (const Patch* p) noexcept;
    float render() noexcept;
    bool finished() const noexcept;
    /** Current envelope level of each operator in dB (tests / display). */
    float operatorLevelDb (int op) const noexcept { return egDb (op); }

    static float levelToDb (float level) noexcept { return -0.75f * (99.0f - level); }
    static float rateToSeconds (float rate, bool rising) noexcept; // time to cross the full 99-unit range

private:
    struct Eg
    {
        int stage = 0;          // 0-2 moving to L1 / L2 / L3, 3 sustain at L3, 4 release to L4, 5 done
        float level = 0.0f;     // current level units (0-99)
        bool released = false;
    };
    void updateControl() noexcept;
    float egDb (int op) const noexcept;
    void stepEg (Eg& e, const Operator& o, float rateBoost, int n) noexcept;

    const Patch* patch = nullptr;
    Patch local;
    double sr = 48000.0;
    int note = 60, velocity = 100;
    float basePitch = 60.0f, amount = 1.0f;
    std::array<uint32_t, kOps> phase {}, inc {};
    std::array<float, kOps> out {}, ampCur {}, ampStep {}, staticDb {}, rateBoost {};
    std::array<Eg, kOps> eg {};
    Eg pitchEg;
    float pitchEgLevel = 50.0f;
    float fb1 = 0.0f, fb2 = 0.0f;
    double lfoPhase = 0.0;
    float lfoValue = 0.0f, lfoHold = 0.0f, lfoDelayTime = 0.0f, lfoAge = 0.0f;
    uint32_t rng = 1;
    int controlCounter = 0;
    static constexpr int kControl = 16;
};
} // namespace vsx::fm
