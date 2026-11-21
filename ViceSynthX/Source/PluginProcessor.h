#pragma once

#include "AI/AIController.h"
#include "Core/PatchState.h"
#include "FX/EffectsRack.h"
#include "Presets/PresetManager.h"
#include "Synth/SynthEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace vsx
{
class ViceSynthProcessor : public juce::AudioProcessor, public PatchHost, private juce::Timer
{
public:
    ViceSynthProcessor();
    ~ViceSynthProcessor() override;

    // ---- AudioProcessor -----------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VICE SYNTH X"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return currentMeta.name; }
    void changeProgramName (int, const juce::String& n) override { currentMeta.name = n; }
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- PatchHost ---------------------------------------------------------------------------
    PatchSpec getCurrentPatch() override;
    void applyPatch (const PatchSpec& spec, const juce::String& undoLabel) override;
    void previewPatch (const PatchSpec& spec) override { applyPatchInternal (spec); }
    bool undo() override;
    bool redo() override;
    bool canUndo() const override { return ! undoStack.empty(); }
    bool canRedo() const override { return ! redoStack.empty(); }
    void audition (int note, int velocity, double seconds) override;
    PresetManager& getPresetManager() override { return presets; }

    /** Lock-free MIDI injection from the GUI (wheels, pads). */
    void injectMidi (const juce::MidiMessage& m);
    /** Stops every note source: keyboard, arp/seq, sustain/hold and all voices. */
    void panic();
    void setHold (bool on) { hold = on; injectMidi (juce::MidiMessage::controllerEvent (1, 64, on ? 127 : 0)); }
    bool isHold() const { return hold; }

    // ---- accessors for the editor ---------------------------------------------------------
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    PatchState& getPatch() { return patch; }
    SynthEngine& getEngine() { return engine; }
    AIController& getAI();
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    float getParam (int index) const { return paramPtr[(size_t) index]->load (std::memory_order_relaxed); }
    void setParam (int index, float realValue);
    juce::RangedAudioParameter* getParameterObject (int index) { return params[(size_t) index]; }

    const PresetMeta& getCurrentMeta() const { return currentMeta; }
    void setCurrentMeta (const PresetMeta& m) { currentMeta = m; }
    void loadPresetEntry (const PresetManager::Entry& e);
    void stepPreset (int delta);
    juce::String getUndoLabel() const { return undoStack.empty() ? juce::String() : undoStack.back().second; }

    // ---- sample / multisample / wavetable import (message thread) ----------------------------
    enum class ImportKind { Auto, Sample, Multisample, Wavetable };
    struct ImportStatus { juce::String text; bool loading = false, error = false; };
    using ImportCallback = std::function<void (bool ok, const juce::String& message)>;
    /** Decodes and validates on a worker thread, then commits resource + oscillator mode together as one
        undoable step on the message thread. A failed, superseded (newer import on the same oscillator) or
        stale (preset / undo / state change while loading) import never replaces the current sound. */
    void importToOscillator (int oscIndex, const juce::File& file, ImportKind kind = ImportKind::Auto, ImportCallback done = {});
    ImportStatus getImportStatus (int oscIndex) const;
    static bool isAudioFile (const juce::File& f);
    static bool isImportable (const juce::File& f);
    /** Delivers finished imports now (normally asynchronous); returns the number still pending. */
    int deliverPendingImports();
    std::function<void (int oscIndex)> onImportStatusChanged; // message thread (GUI)

    /** Modulation-matrix helpers (message thread). Returns slot index or -1. */
    int addModulation (int source, int destParam, float amount);
    void removeModulation (int slot);
    void clearModulationSlot (int slot);
    juce::Array<int> slotsTargeting (int destParam) const;

    // ---- metering / status (written on the audio thread) ------------------------------------
    std::atomic<float> outputPeakL { 0.0f }, outputPeakR { 0.0f };
    std::atomic<float> cpuLoad { 0.0f };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<int> nanResets { 0 };
    juce::AudioBuffer<float> scopeBuffer; // last output (message thread reads with a copy)
    std::atomic<int> scopeWrite { 0 };

    juce::var uiState; // editor size, tab etc. persisted with the project

private:
    void timerCallback() override;
    void snapshotParams (ParamSnapshot& s) const;
    void pushUndo (const juce::String& label);
    void applyPatchInternal (const PatchSpec& spec);

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, pid::kNumParams> paramPtr {};
    std::array<juce::RangedAudioParameter*, pid::kNumParams> params {};

    PatchState patch;
    SynthEngine engine { patch };
    EffectsRack rack;
    PresetManager presets;
    std::unique_ptr<AIController> ai;
    PresetMeta currentMeta;

    juce::MidiKeyboardState keyboardState;
    ParamSnapshot snapshot, rawSnapshot;
    juce::AudioBuffer<float> chunk;
    juce::MidiBuffer chunkMidi;
    juce::AudioProcessLoadMeasurer loadMeasurer;
    Smoother masterGain;
    double sampleRate = 44100.0;
    int lastLatency = -1;

    // audition: lock-free note injection from the message thread
    struct AuditionEvent { int note, velocity; bool on; };
    juce::AbstractFifo auditionFifo { 64 };
    std::array<AuditionEvent, 64> auditionEvents {};
    std::vector<std::pair<double, int>> pendingAuditionOffs; // message thread: time, note
    juce::AbstractFifo injectFifo { 256 };
    std::array<std::array<juce::uint8, 3>, 256> injectData {};

    std::vector<std::pair<PatchSpec, juce::String>> undoStack, redoStack;
    int recoveryCounter = 0;
    juce::String lastRecoveryJson;
    bool processing = false;
    bool hold = false;

    // import machinery (declared last: the worker pool is destroyed first)
    struct ImportResult
    {
        int osc = 0; uint32_t gen = 0, serial = 0; ImportKind kind = ImportKind::Sample;
        bool ok = false; juce::String sourceId, name, error, info; ImportCallback done;
    };
    class ImportQueue : public juce::AsyncUpdater
    {
    public:
        explicit ImportQueue (ViceSynthProcessor& p) : owner (p) {}
        ~ImportQueue() override { cancelPendingUpdate(); }
        void push (ImportResult r) { { const juce::ScopedLock sl (lock); results.push_back (std::move (r)); } triggerAsyncUpdate(); }
        std::vector<ImportResult> take() { const juce::ScopedLock sl (lock); auto r = std::move (results); results.clear(); return r; }
        void handleAsyncUpdate() override { owner.deliverPendingImports(); }
    private:
        ViceSynthProcessor& owner;
        juce::CriticalSection lock;
        std::vector<ImportResult> results;
    };
    void commitImport (ImportResult r);
    void setImportStatus (int osc, ImportStatus s);
    std::array<ImportStatus, kNumOscs> importStatus;
    std::array<uint32_t, kNumOscs> importGen {};
    uint32_t externalPatchSerial = 0;
    bool committingImport = false;
    std::atomic<int> pendingImports { 0 };
    ImportQueue importQueue { *this };
    juce::ThreadPool importPool { juce::ThreadPoolOptions{}.withThreadName ("VSX import").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViceSynthProcessor)
};

} // namespace vsx
