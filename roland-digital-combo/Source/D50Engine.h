#pragma once
// Original LA-style synthesis engine driven directly by a D-50 format temporary area
// (448 bytes laid out exactly as in "D-50 MIDI Implementation" *4-3 .. *4-6).
//
// No Roland ROM, PCM samples or reverb microcode are used: PCM partials play
// algorithmically generated substitute waves (see PcmBank), the reverb is an original FDN.

#include "D50SysEx.h"
#include "DspCommon.h"

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
};

struct D50ToneParams
{
    D50PartialParams partial[2];
    int structure = 0;
    float penvTimes[4] {}, penvLevels[5] {}; // L0, L1, L2, Sustain, End  (semitones)
    float penvVelRange = 0;
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
    void reset() noexcept;

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void allNotesOff() noexcept;  // release
    void allSoundOff() noexcept;  // immediate
    void setPitchBend (float minus1to1) noexcept { bend = minus1to1; }
    void setModWheel (float zeroTo1) noexcept { modWheel = zeroTo1; }
    void setAftertouch (float zeroTo1) noexcept { aftertouch = zeroTo1; }

    // Decoded parameters for this block (decode once per block from the raw area).
    void setParams (const D50Params& p) noexcept { params = p; }

    // Adds a stereo render of n samples to L/R.
    void render (float* L, float* R, int n) noexcept;

    int activeVoiceCount() const noexcept;

private:
    struct PartialVoice
    {
        float phase = 0, pcmPos = 0;
        Svf f1, f2;
        SegmentEnvelope tvf, tva;
        float lastOsc = 0;
    };
    struct ToneVoice
    {
        bool active = false;
        PartialVoice p[2];
        SegmentEnvelope penv;
        Lfo lfo[3];
        float elapsed = 0;
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

    void startTone (ToneVoice& tv, const D50ToneParams& tp, float velocity) noexcept;
    void releaseVoice (Voice& v) noexcept;
    void renderTone (ToneVoice& tv, const D50ToneParams& tp, float note, float velocity, float* out, int n) noexcept;
    void configureEnvelopes (PartialVoice& pv, const D50PartialParams& pp) noexcept;

    Voice voices[kMaxVoices];
    D50Params params;
    float sampleRate = 44100.0f;
    uint64_t counter = 0;
    bool sustainDown = false;
    float bend = 0, modWheel = 0, aftertouch = 0;
    float freeLfoPhase[2][3] {};

    std::vector<float> upperBuf, lowerBuf, tmpL, tmpR, revInL, revInR, revOutL, revOutR;
    Biquad eqLow[2], eqHigh[2];
    Chorus chorus[2];
    Reverb reverb;
};

// Algorithmically generated substitute for the D-50's 100 PCM waves.
// 0..49 are one-shot attack transients, 50..99 are single-cycle loops.
struct PcmBank
{
    struct Wave
    {
        std::vector<float> data;
        bool loop = false;
        float rootHz = 261.6256f;  // pitch the wave was generated at
        float rate = 32000.0f;     // nominal sample rate of the data
    };
    Wave waves[100];
    static const PcmBank& get();
private:
    PcmBank();
};
} // namespace dc
