#pragma once
// Original JD-800 style engine driven by a 384-byte patch laid out exactly as the JD-800
// "Patch Temporary Area" (Owner's Manual II, *4-5-1): Common, Effect, Tone A-D.
// Structure and parameter meaning follow the manual; curve shapes and scalings that the manual
// does not quantify are estimates (see docs/VERIFICATION.md). No Roland waveform ROM is used.

#include "DspCommon.h"
#include "JD800SysEx.h"
#include "JdEffects.h"
#include "JdWaves.h"
#include "UserWaves.h"

namespace dc
{
struct JdLfoParams
{
    float rateHz = 1, delaySec = 0, fadeSec = 0;
    bool delayRelease = false, fadeIn = true;
    int wave = 0, offset = 1; // offset 0 '+', 1 '0', 2 '-'
    bool keyTrigger = false;
};

struct JdToneParams
{
    int velocityCurve = 0; bool holdControl = true;
    JdLfoParams lfo[2];
    int waveform = 0;
    float coarse = 0, fineCents = 0, randomCents = 0, keyFollow = 1;
    bool bender = true, aftertouchBend = false;
    float lfoSemis[2] {}, leverSemis = 0, atModSemis = 0; // signed: <0 -> LFO2, >0 -> LFO1 (lever/at)
    float peVelo = 0, peTimeVelo = 0, peTimeKf = 0, peL0 = 0, peT1 = 0, peL1 = 0, peT2 = 0, peT3 = 0, peL2 = 0;
    int filterMode = 2; float cutoff = 100, resonance = 0, cutoffKf = 0, tvfAt = 0;
    int tvfLfo = 0; float tvfLfoOct = 0, tvfEnvOct = 0;
    float fVelo = 0, fTimeVelo = 0, fTimeKf = 0, fT[4] {}, fL1 = 0, fL2 = 0, fSus = 0, fL4 = 0;
    int biasDir = 0, biasPoint = 60; float biasDbPerOct = 0;
    float level = 1, tvaAt = 0; int tvaLfo = 0; float tvaLfoDepth = 0;
    float aVelo = 0, aTimeVelo = 0, aTimeKf = 0, aT[4] {}, aL1 = 1, aL2 = 1, aSus = 1;
};

struct JdCommonParams
{
    float level = 1;
    int keyLow[4] { 0, 0, 0, 0 }, keyHigh[4] { 127, 127, 127, 127 };
    int bendDown = 2, bendUp = 2, atBendSemis = 0;
    bool solo = false, legato = false, portamento = false; int portaMode = 0; float portaSec = 0;
    int layerMask = 15;
    float lowHz = 200, lowDb = 0, midHz = 1000, midQ = 1, midDb = 0, highHz = 4000, highDb = 0;
};

struct JdPatch
{
    JdCommonParams common;
    JdEffectParams effect;
    JdToneParams tone[4];
    static JdPatch decode (const uint8_t* raw384) noexcept;
};

float jdVelocityCurve (int curve0to3, float velocity01) noexcept; // shapes of Owner's Manual II p. I-17

class JD800Engine
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare (double sampleRate, int maxBlock);
    // Waves the user loaded from their own files (may be null). Set from the audio thread.
    void setUserWaves (const UserWaveSet* s) noexcept { userWaves = s; }
    void reset() noexcept;
    void setPatch (const JdPatch& p) noexcept { patch = p; }

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;
    void setPitchBend (float minus1to1) noexcept { bend = minus1to1; }
    void setModWheel (float zeroTo1) noexcept { modWheel = zeroTo1; }
    void setAftertouch (float zeroTo1) noexcept { aftertouch = zeroTo1; }

    void render (float* L, float* R, int n) noexcept;
    int activeVoiceCount() const noexcept;
    int activeToneCount (int tone) const noexcept;

private:
    const UserWaveSet* userWaves = nullptr;
    struct LfoState
    {
        float phase = 0, held = 0, from = 0, to = 0, elapsed = 0;
        bool running = false;
        Rng rng;
    };
    struct ToneState
    {
        bool active = false, released = false, releaseHandled = false;
        JdOscillator osc;
        SegmentEnvelope penv, fenv, aenv;
        LfoState lfo[2];
        Svf filter;
        float lastAmp = 0, velocity = 1, biasGain = 1, envVeloF = 1, randomCents = 0, elapsed = 0, sinceRelease = 0;
    };
    struct Voice
    {
        int note = -1;
        float velocity = 0, glideOffset = 0, glideRate = 0;
        bool held = false, sustained = false;
        uint64_t age = 0;
        ToneState tone[4];
        bool isActive() const noexcept
        {
            for (auto& t : tone) if (t.active) return true;
            return false;
        }
    };

    void startTone (Voice& v, int t, bool retrigger) noexcept;
    void releaseTone (ToneState& ts) noexcept;
    void releaseVoice (Voice& v) noexcept;
    float lfoValue (LfoState& s, const JdLfoParams& p, int lfoIndex, int toneIndex, float dt, bool released) noexcept;
    void renderTone (Voice& v, int t, float* out, int n) noexcept;

    Voice voices[kMaxVoices];
    JdPatch patch;
    float sampleRate = 44100.0f;
    uint64_t counter = 0;
    bool sustainDown = false;
    float bend = 0, modWheel = 0, aftertouch = 0;
    float freeLfoPhase[4][2] {};
    int lastNote = -1;
    int heldStack[32] {}; int heldCount = 0;
    Rng noteRng;

    std::vector<float> bus, wetL, wetR;
    Biquad eqLow, eqMid, eqHigh;
    JdEffects effects;
    DcBlocker dcL, dcR;
};
} // namespace dc
