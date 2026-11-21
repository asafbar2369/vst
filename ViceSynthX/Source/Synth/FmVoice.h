#pragma once

#include "../Core/DspUtils.h"
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <memory>

/**
    Six-operator FM voice in the style of the classic 1983 six-operator FM architecture.
    The voice data model, VCED / packed / hex codecs, SysEx import / export, algorithm chart and factory voices are
    VICE SYNTH X code. Rendering (FmRenderer) uses MSFA, the Apache-2.0 "Modern" engine vendored unmodified from Dexed
    (Source/ThirdParty/msfa, see NOTICE.md). No Dexed GPLv3 code and no factory ROM content are included.
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
/**
    Per-voice FM renderer built on MSFA (the Apache-2.0 FM core used by Dexed: Dx7Note, envelopes, pitch envelope,
    LFO, FmCore; Source/ThirdParty/msfa, see its LICENSE and NOTICE). MSFA keeps its sample-rate tables in process-wide
    statics, so it always runs at kInternalRate (48 kHz) and the result is resampled (cubic) to the voice rate.
    Per-voice output scaling matches Dexed's (Q24 >> 13 to 16-bit, hard limit at full scale per voice).
    Differences from the Dexed plug-in: the "Modern" FmCore engine (Dexed's default "Mark I" engine is GPLv3 and is not
    used), one LFO per voice instead of one shared LFO, standard tuning only.
*/
class FmRenderer
{
public:
    static constexpr double kInternalRate = 48000.0;
    FmRenderer();
    ~FmRenderer();
    void prepare (double sampleRate);
    void noteOn (const Patch& p, int note, int velocity, float pitchSemis, uint32_t seed);
    void noteOff() noexcept;
    /** Call once per control block (pitch in absolute MIDI semitones incl. bend / glide / octave; FM Amount). */
    void setPitch (float pitchSemis, float fmAmount) noexcept;
    void setPatch (const Patch* p) noexcept;
    float render() noexcept;
    bool finished() const noexcept { return released && silentBlocks > 64; }

    static float rateToSeconds (float rate, bool rising) noexcept; // estimate used to size the amp release
    /** Dexed / MSFA raw patch: 155 VCED bytes + operator switch (bit 0 = OP6 ... bit 5 = OP1). */
    static std::array<uint8_t, 156> rawPatch (const Patch& p, float fmAmount = 1.0f);
    /** Renders one internal block of 64 samples at 48 kHz (tests and the Dexed reference harness). */
    void renderInternalBlock (float* out64) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    const Patch* patch = nullptr;
    Patch local;
    double sr = 48000.0;
    int note = 60, velocity = 100;
    float basePitch = 60.0f, amount = 1.0f, appliedAmount = 1.0f;
    bool started = false, released = false;
    int silentBlocks = 0;
    // resampler state (internal 48 kHz -> voice rate)
    std::array<float, 64> block {};
    int blockPos = 64;
    double frac = 0.0, step = 1.0;
    float h0 = 0.0f, h1 = 0.0f, h2 = 0.0f, h3 = 0.0f;
    float nextInternal() noexcept;
    /** The key MSFA plays: the played key shifted by the voice's transpose (as Dexed does on key-down). */
    int dxNote() const noexcept { return juce::jlimit (0, 127, note + (int) (patch != nullptr ? patch->transpose : 24) - 24); }
};
} // namespace vsx::fm
