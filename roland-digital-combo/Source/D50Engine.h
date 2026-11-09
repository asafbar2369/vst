#pragma once
// Original LA-style synthesis engine driven directly by a D-50 format temporary area
// (448 bytes laid out exactly as in "D-50 MIDI Implementation" *4-3 .. *4-6).
//
// No Roland ROM, PCM samples or reverb microcode are used: PCM partials play
// algorithmically generated substitute waves (see PcmBank), the reverb is an original FDN.

#include "D50SysEx.h"
#include "DspCommon.h"
#include "UserWaves.h"

namespace dc
{
struct D50PartialParams
{
    bool synth = true;          // true: synth WG, false: PCM (decided by structure)
    int waveform = 0;           // 0 square, 1 sawtooth
    int pcmWave = 0;            // 0..99 -> substitute PCM wave
    float coarseSemis = 0, fineCents = 0, keyfollow = 1;
    int lfoMode = 0, penvMode = 0, bendMode = 2;
    float pulseWidth = 0;       // 0..1
    int pwLfoSelect = 0; float pwLfoDepth = 0;
    float cutoff = 100, resonance = 0, tvfKeyfollow = 0;
    float tvfEnvDepth = 0, tvfVelRange = 0;
    float tvfTimes[5] {}, tvfLevels[3] {}, tvfSustain = 0, tvfEnd = 0;
    int tvfLfoSelect = 0; float tvfLfoDepth = 0;
    float tvaLevel = 1, tvaVelRange = 0;
    float tvaTimes[5] {}, tvaLevels[3] {}, tvaSustain = 1, tvaEnd = 0;
    int tvaLfoSelect = 0; float tvaLfoDepth = 0;
    // Key / velocity / aftertouch scaling (offsets 9, 12, 16, 17, 20, 21, 34, 37, 38, 49, 50, 53)
    float pwVelRange = 0, pwAtRange = 0;      // -1..1  (doc -7..+7)
    int tvfBiasPoint = 64; float tvfBiasLevel = 0; // level -1..1 (doc -7..+7)
    int tvfDepthKf = 0, tvfTimeKf = 0;        // 0..4
    float tvfAtRange = 0;                     // -1..1
    int tvaBiasPoint = 64; float tvaBiasLevelDb = 0; // doc -12..0
    int tvaVelFollow = 0, tvaTimeKf = 0;      // 0..4
    float tvaAtRange = 0;                     // -1..1
};

struct D50ToneParams
{
    D50PartialParams partial[2];
    int structure = 0;
    float penvTimes[4] {}, penvLevels[5] {}; // L0, L1, L2, Sustain, End  (semitones)
    float penvVelRange = 0;
    int penvTimeKf = 0;
    float pModLfoDepth = 0, pModLever = 0, pModAftertouch = 0;
    int lfoWave[3] {}; float lfoRate[3] {}, lfoDelay[3] {}; int lfoSync[3] {};
    float lowEqHz = 100, lowEqDb = 0, highEqHz = 4000, highEqQ = 1, highEqDb = 0;
    int chorusType = 0; float chorusRate = 0, chorusDepth = 0, chorusBalance = 0;
    bool partialOn[2] { true, true };
    float partialBalance = 50;
};

struct D50PatchParams
{
    int keyMode = 0, splitNote = 60;
    int upperShift = 0, lowerShift = 0;
    float upperFine = 0, lowerFine = 0;
    int benderRange = 2;
    int reverbType = 0; float reverbBalance = 0;
    float totalVolume = 1, toneBalance = 50;
    int portamentoMode = 2, holdMode = 2;     // 0 U, 1 L, 2 UL
    float portamentoSeconds = 0;
    int aftertouchBend = 0;                   // semitones -12..+12
};

struct D50Params
{
    D50ToneParams upper, lower;
    D50PatchParams patch;
    static D50Params decode (const uint8_t* raw448) noexcept;
};

class D50Engine
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare (double sampleRate, int maxBlock);
    // Waves the user loaded from their own files (may be null). Set from the audio thread.
    void setUserWaves (const UserWaveSet* s) noexcept { userWaves = s; }
    void reset() noexcept;

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void allNotesOff() noexcept;  // release
    void allSoundOff() noexcept;  // immediate
    void setPitchBend (float minus1to1) noexcept { bend = minus1to1; }
    void setModWheel (float zeroTo1) noexcept { modWheel = zeroTo1; }
    void setAftertouch (float zeroTo1) noexcept { aftertouch = zeroTo1; }
    void setPortamento (bool on) noexcept { portamentoOn = on; }

    // Decoded parameters for this block (decode once per block from the raw area).
    void setParams (const D50Params& p) noexcept { params = p; }

    // Adds a stereo render of n samples to L/R.
    void render (float* L, float* R, int n) noexcept;

    int activeVoiceCount() const noexcept;
    int activeToneCount (bool upper) const noexcept;

private:
    struct PartialVoice
    {
        float phase = 0, pcmPos = 0;
        Svf f1, f2;
        SegmentEnvelope tvf, tva;
        float lastOsc = 0;
        float tvaBiasGain = 1, tvfBiasOct = 0, tvfDepthScale = 1; // fixed per note
    };
    struct ToneVoice
    {
        bool active = false;
        PartialVoice p[2];
        SegmentEnvelope penv;
        Lfo lfo[3];
        float elapsed = 0;
        float glideOffset = 0, glideRate = 0; // portamento, semitones and semitones/s
    };
    struct Voice
    {
        int note = -1;
        float velocity = 0;
        bool held = false, sustained = false;
        uint64_t age = 0;
        ToneVoice upper, lower;
        bool isActive() const noexcept { return upper.active || lower.active; }
    };

    void startTone (ToneVoice& tv, const D50ToneParams& tp, float toneNote, float velocity) noexcept;
    void releaseVoice (Voice& v) noexcept;
    static void releaseTone (ToneVoice& tv) noexcept;
    void renderTone (ToneVoice& tv, const D50ToneParams& tp, float note, float velocity, float* out, int n) noexcept;
    void configureEnvelopes (PartialVoice& pv, const D50PartialParams& pp, float note, float velocity) noexcept;

    Voice voices[kMaxVoices];
    D50Params params;
    float sampleRate = 44100.0f;
    uint64_t counter = 0;
    bool sustainDown = false, portamentoOn = false;
    float bend = 0, modWheel = 0, aftertouch = 0;
    int lastNote = -1;
    float freeLfoPhase[2][3] {};

    std::vector<float> upperBuf, lowerBuf, tmpL, tmpR, revInL, revInR, revOutL, revOutR;
    Biquad eqLow[2], eqHigh[2];
    Chorus chorus[2];
    Reverb reverb;
    DcBlocker dcL, dcR;
    const UserWaveSet* userWaves = nullptr;
};

// Algorithmically generated substitute for the D-50's 100 PCM waves. The numbering, names and
// types follow the PCM list in the D-50 Owner's Manual (p. 34): 1-47 are one-shot attack
// sounds, 48-76 are looped waves and 77-100 are loops that combine several of the other sounds.
// The audio itself is synthesised here from each name's description - it is NOT the Roland ROM.
struct PcmBank
{
    using Wave = SampleWave;
    Wave waves[100];
    static const PcmBank& get();
private:
    PcmBank();
};
} // namespace dc
