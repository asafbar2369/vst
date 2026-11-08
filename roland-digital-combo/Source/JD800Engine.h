#pragma once
// Original 4-tone layered synth engine in the style of the JD-800 (tones A-D, each with
// its own waveform, TVF and TVA, plus a shared effects chain).
// No Roland waveform ROM is used: every waveform is generated algorithmically.

#include "DspCommon.h"

namespace dc
{
enum class JdWave
{
    Saw, Square, Pulse25, Pulse12, Triangle, Sine, SuperSaw, Noise,
    Organ, FmBell, FmPiano, Metal, SyncSaw, Vox, Pwm, SubSaw, Count
};
const char* jdWaveName (int index) noexcept;

struct JdToneParams
{
    bool on = true;
    int wave = 0;
    float coarse = 0, fineCents = 0, level = 0.8f, pan = 0;
    int filterMode = 0;               // 0 LPF, 1 BPF, 2 HPF
    float cutoff = 80, resonance = 0, envDepth = 0; // cutoff/res 0..100, envDepth -1..1
    float fA = 0, fD = 50, fS = 50, fR = 40;        // 0..100 time/level values
    float aA = 0, aD = 50, aS = 80, aR = 40;
    float velSens = 0.5f;             // 0..1
};

struct JdParams
{
    JdToneParams tone[4];
    float lfoRate = 5.0f, lfoPitchDepth = 0.3f;
    int bendRange = 2;
    float chorusMix = 0.3f, chorusRate = 0.6f;
    float delayTimeMs = 350, delayFeedback = 0.3f, delayMix = 0.0f;
    float reverbSize = 0.5f, reverbMix = 0.2f;
    float volume = 0.8f;
};

class JD800Engine
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare (double sampleRate, int maxBlock);
    void reset() noexcept;
    void setParams (const JdParams& p) noexcept { params = p; }

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;
    void setPitchBend (float minus1to1) noexcept { bend = minus1to1; }
    void setModWheel (float zeroTo1) noexcept { modWheel = zeroTo1; }

    void render (float* L, float* R, int n) noexcept;
    int activeVoiceCount() const noexcept;

private:
    struct ToneState
    {
        bool active = false;
        float phase[8] {};
        float modPhase = 0, fmEnv = 0, pwmPhase = 0;
        Svf filter, formant1, formant2;
        SegmentEnvelope fEnv, aEnv;
        Rng rng;
        float lastAmp = 0;
    };
    struct Voice
    {
        int note = -1;
        float velocity = 0;
        bool held = false, sustained = false;
        uint64_t age = 0;
        ToneState tone[4];
        bool isActive() const noexcept
        {
            for (auto& t : tone) if (t.active) return true;
            return false;
        }
    };

    void releaseVoice (Voice& v) noexcept;
    void renderTone (ToneState& ts, const JdToneParams& tp, float note, float velocity, float* L, float* R, int n) noexcept;
    float oscillator (ToneState& ts, int wave, float dt) noexcept;

    Voice voices[kMaxVoices];
    JdParams params;
    float sampleRate = 44100.0f;
    uint64_t counter = 0;
    bool sustainDown = false;
    float bend = 0, modWheel = 0, lfoPhase = 0;

    std::vector<float> busL, busR, mono, wetL, wetR, revL, revR;
    Chorus chorus;
    StereoDelay delay;
    Reverb reverb;
};
} // namespace dc
