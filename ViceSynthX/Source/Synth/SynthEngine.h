#pragma once
#include "../Core/Tuning.h"

#include "ArpSeq.h"
#include "Macros.h"
#include "Voice.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace vsx
{
/**
    Polyphonic voice engine.

    - Sample-accurate MIDI: blocks are split at every event; modulation runs at a 16-sample control rate.
    - The complete voice path (oscillators + filters) runs at 1x/2x/4x and is decimated with
      polyphase IIR half-band filters (juce::dsp::Oversampling), pre-allocated for every quality.
    - Voice stealing: released voices with the lowest level first, then the oldest; stolen voices
      fade out over 4 ms in a spare slot so steals are click-free.
    - No allocation, locking or I/O on the audio thread.
*/
class SynthEngine
{
public:
    static constexpr int kVoiceSlots = kMaxVoices + 8;
    static constexpr int kTick = 16;
    static constexpr int kChunk = 512;

    explicit SynthEngine (PatchState& patchState);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void allNotesOff();

    /** Renders into buffer (overwritten). params must hold the raw parameter values; on return it holds the
        effective values (engine macros applied) and params.norm is filled for modulated destinations. */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, ParamSnapshot& params, const TransportInfo& transport);

    int getLatencySamples() const noexcept { return latencyFor (quality); }
    int latencyFor (int q) const noexcept;
    double getSampleRate() const noexcept { return sampleRate; }

    const ModOffsets& getGlobalMods() const noexcept { return globalMods; }
    const MatrixSnapshot& getMatrix() const noexcept { return matrix; }

    // ---- feedback for the GUI (written on the audio thread) -------------------------
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> lastNote { -1 };
    /** Microtuning table (see Core/Tuning.h); call from any thread except the audio thread. */
    void publishTuning (const tuning::Table& t) noexcept;
    std::array<std::atomic<float>, pid::kNumParams> modDisplay; // normalised modulated value, < 0 = not modulated
    std::array<std::atomic<float>, kNumEnvs> envDisplay;
    std::array<std::atomic<float>, kNumLfos> lfoDisplay;
    std::atomic<int> seqStepDisplay { -1 };

private:
    void processChunk (float* outL, float* outR, int numSamples, const juce::MidiBuffer& midi, ParamSnapshot& params,
                       const TransportInfo& transport);
    void renderTick (float* osL, float* osR, int startBase, int numBase, const ParamSnapshot& params, const TransportInfo& t,
                     double tickPpq);
    void handleMidi (const juce::MidiMessage& m, const ParamSnapshot& params);
    void noteOn (int note, int velocity, const ParamSnapshot& params);
    void noteOff (int note, const ParamSnapshot& params);
    Voice* findFreeVoice();
    Voice* chooseVictim();
    Voice* monoVoice();
    Voice* newestVoice();
    void setQuality (int q);
    void publishDisplay (const ParamSnapshot& params);

    PatchState& patch;
    // ~1 MB of voices: kept on the heap so an engine (or OfflineRenderer) can safely live on a
    // thread stack (Windows threads default to 1 MB). Allocated once in the constructor;
    // never resized, so voice pointers stay stable during processing (same intent as PR #6).
    std::unique_ptr<std::array<Voice, kVoiceSlots>> voicePool { std::make_unique<std::array<Voice, kVoiceSlots>>() };
    std::array<Voice, kVoiceSlots>& voices = *voicePool;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> oversamplers;
    juce::AudioBuffer<float> chunkBuffer;
    juce::MidiBuffer chunkMidi, arpMidi;
    ArpSeq arp;

    double sampleRate = 44100.0;
    int quality = 1, osFactor = 2;
    bool prepared = false;

    VoiceContext ctx;
    MatrixSnapshot matrix;
    ModOffsets globalMods;
    std::array<float, modsrc::Count> globalSources {};
    std::array<double, kNumLfos> lfoPhase {}, lfoCycle {};

    float modWheel = 0.0f, pressure = 0.0f, bend = 0.0f, expression = 1.0f, breath = 0.0f, timbre = 0.0f;
    bool sustainPedal = false;
    std::array<int, 128> noteStack {};
    int noteStackSize = 0;
    uint64_t ageCounter = 0;
    float lastPlayedPitch = -1.0f;
    // MPE: per-channel expression (index = MIDI channel 1..16), applied to the notes of that channel
    std::array<float, 17> chBend {}, chPressure {}, chTimbre {};
    void startedVoice (Voice& v) noexcept;
    int curChannel = 1;     // channel of the MIDI message being handled
    bool curMpe = false;    // MPE on and the message is on a member channel (2..16)
    // microtuning: written by publishTuning (any non-audio thread), copied by the audio thread when a complete version is
    // available (per-entry atomics + version counter: never blocks, never reads a half-written table)
    std::array<std::atomic<float>, 128> tuningShared;
    std::atomic<uint32_t> tuningVersion { 0 };
    uint32_t tuningSeen = 0;
    tuning::Table tuningLocal = tuning::standard();
    void refreshTuning() noexcept;
    ParamSnapshot macroBase;
    double motionTime = 0.0;
    std::array<int, pid::kNumParams> displayed {};
    int numDisplayed = 0;
};

} // namespace vsx
