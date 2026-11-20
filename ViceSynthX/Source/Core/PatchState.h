#pragma once

#include "DspUtils.h"
#include "Params.h"
#include "../Synth/SampleBank.h"
#include "../Synth/Wavetable.h"
#include <array>

namespace vsx
{
/** Modulation-matrix slot routing (the amount is the host parameter "modN_amount"). */
struct ModSlotState
{
    enum Polarity { Native = 0, Bipolar = 1, Unipolar = 2 };
    std::atomic<int> source { modsrc::None };
    std::atomic<int> dest { -1 };           // parameter index or -1
    std::atomic<int> polarity { Native };
    std::atomic<int> aux { modsrc::None };  // optional scaling source
    std::atomic<float> curve { 0.0f };      // -1..1 response curve

    bool active() const noexcept { return source.load() != modsrc::None && dest.load() >= 0; }
};

/** User-drawn LFO curve. Points are (x 0..1, y -1..1) with per-segment tension. */
struct LfoCurve
{
    struct Point { float x, y, tension; };
    std::vector<Point> points { { 0.0f, 0.0f, 0.0f }, { 0.25f, 1.0f, 0.0f }, { 0.75f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    float evaluate (float x) const;
    juce::var toVar() const;
    static LfoCurve fromVar (const juce::var& v);
};

struct LfoTable
{
    static constexpr int kSize = 512;
    std::array<float, kSize + 1> v {};
    float read (float phase) const noexcept
    {
        const float idx = phase * (float) kSize;
        const int i = juce::jlimit (0, kSize - 1, (int) idx);
        return v[(size_t) i] + (idx - (float) i) * (v[(size_t) i + 1] - v[(size_t) i]);
    }
};

struct ArpStep
{
    bool on = true;
    float velocity = 1.0f;   // 0..1 (used in Accent velocity mode and as a scale)
    int octave = 0;          // -2..2 additional octave offset
    float gate = 1.0f;       // 0..2 multiplier
};

struct ArpPattern
{
    int length = 16;
    std::array<ArpStep, 32> steps {};
};

struct SeqStep
{
    static constexpr int kNone = -128;
    bool on = false;
    std::array<int, 4> notes { 0, kNone, kNone, kNone }; // semitones relative to the root (C4 or the held key)
    float velocity = 0.8f;
    float gate = 0.5f;          // fraction of a step (up to 2 = legato)
    float probability = 1.0f;
    float mod = 0.0f;           // -1..1, exposed as the "Seq Mod" modulation source
    bool tie = false;
};

struct SeqPattern
{
    std::array<SeqStep, 32> steps {};
};

SeqPattern defaultSeqPattern();

struct OscResources
{
    RtShared<Wavetable> wavetable;
    RtShared<SampleData> sample;
    RtShared<MultiSample> multi;
    // message-thread descriptors
    juce::String wavetableId { "factory:Basic Shapes" };
    juce::String sampleId;
    juce::String multiId;
    juce::String lastError;
};

/**
    Non-parameter patch state shared by the audio engine, GUI, presets and AI.
    Audio thread: reads atomics and RtShared pointers only.
    Message thread: everything else.
*/
class PatchState : private juce::AsyncUpdater
{
public:
    PatchState();
    ~PatchState();

    std::array<ModSlotState, kNumModSlots> slots;
    std::array<std::atomic<int>, pid::NumFx> fxOrder;
    std::array<OscResources, kNumOscs> osc;
    std::array<RtShared<LfoTable>, kNumLfos> lfoTables;
    std::array<LfoCurve, kNumLfos> lfoCurves;    // message thread
    RtShared<ArpPattern> arpPattern;
    RtShared<SeqPattern> seqPattern;
    juce::String userIrPath;                      // message thread
    std::atomic<int> irGeneration { 0 };

    // ---- message-thread API ---------------------------------------------------------
    void setLfoCurve (int lfo, const LfoCurve& curve);
    void setArpPattern (const ArpPattern& p) { arpPattern.set (std::make_shared<ArpPattern> (p)); }
    void setSeqPattern (const SeqPattern& p) { seqPattern.set (std::make_shared<SeqPattern> (p)); }
    ArpPattern getArpPattern() const { auto p = arpPattern.getShared(); return p ? *p : ArpPattern(); }
    SeqPattern getSeqPattern() const { auto p = seqPattern.getShared(); return p ? *p : SeqPattern(); }

    /** Load resources. async = true uses the background loader; completion is reported via onResourceChanged. */
    bool setWavetable (int oscIndex, const juce::String& sourceId, bool async);
    bool setWavetableObject (int oscIndex, std::shared_ptr<const Wavetable> wt);
    bool setSample (int oscIndex, const juce::String& sourceId, bool async, std::function<void (bool)> done = {});
    bool setMultiSample (int oscIndex, const juce::String& sourceId, bool async, std::function<void (bool)> done = {});
    void clearSample (int oscIndex);
    void cancelResourceLoads (int oscIndex)
    {
        if (juce::isPositiveAndBelow (oscIndex, kNumOscs))
            for (auto& revision : resourceRevision[(size_t) oscIndex]) ++revision;
    }

    std::function<void (int oscIndex)> onResourceChanged; // called on the message thread

    void resetToDefaults();
    void collectGarbage();
    void waitForLoads (int timeoutMs = 20000);
    bool isLoading() const { return pendingLoads.load() > 0; }

    juce::var toVar() const;                 // excludes parameters
    void fromVar (const juce::var& v, bool asyncLoads);

    /** Missing resources referenced by the current state. */
    juce::StringArray missingResources() const;

private:
    void notifyChanged (int oscIndex);
    void handleAsyncUpdate() override;
    void enqueueCompletion (std::function<void()> completion);
    // Descriptors and generation counters are owned by the message/owner thread.
    std::array<std::array<uint64_t, 3>, kNumOscs> resourceRevision {};
    std::mutex completionMutex;
    std::vector<std::function<void()>> completions;
    juce::ThreadPool loader { juce::ThreadPoolOptions().withThreadName ("VSX Loader").withNumberOfThreads (1) };
    std::atomic<int> pendingLoads { 0 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace vsx
