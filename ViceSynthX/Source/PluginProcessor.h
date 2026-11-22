#pragma once

#include "AI/AIController.h"
#include "Core/PatchState.h"
#include "FX/EffectsRack.h"
#include "Presets/PresetManager.h"
#include "Synth/SynthEngine.h"
#include "Synth/SoundLayer.h"
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
    /** Replaces the current sound with the clean INIT patch (one analog saw, open ladder filter, no effects). Undoable. */
    void loadInitPatch();
    /** AUTO GAIN: renders the current sound offline on a worker thread (silently: nothing is played) as a single note
        and a full-velocity six-note chord, and sets Preset Volume with the factory-library rule (single-note RMS
        -20 dBFS, single peak <= -6 dBFS, chord peak <= -1.6 dBFS). Undoable. done(newDb, report) on the message thread. */
    void autoGain (std::function<void (float, juce::String)> done = {});
    /** The calibration rule shared by AUTO GAIN and the factory library: dB to add to Preset Volume. */
    static float autoGainDelta (float singleRmsDb, float singlePeakDb, float chordPeakDb);
    /** Six-operator FM voices (undoable). asNewPatch: a clean patch around the voice on OSC 1 (filters and effects
        off, amp envelope following the voice); otherwise the voice replaces OSC n's sound and the oscillator switches
        to FM mode, keeping the rest of the patch. */
    void loadFmVoice (int oscIndex, const fm::Patch& voice, bool asNewPatch);
    /** Amp-envelope release (s) that lets the voice's own carrier release finish. */
    static float fmReleaseSeconds (const fm::Patch& voice);
    void previewPatch (const PatchSpec& spec) override { applyPatchInternal (spec); }
    bool undo() override;
    bool redo() override;
    bool canUndo() const override { return ! undoStack.empty(); }
    bool canRedo() const override { return ! redoStack.empty(); }
    PresetManager& getPresetManager() override { return presets; }

    /** Lock-free MIDI injection from the GUI (wheels, pads). */
    void injectMidi (const juce::MidiMessage& m);
    /** Stops every note source: keyboard, arp/seq, sustain/hold and all voices. */
    void panic();

    /** MIDI Learn: the next controller (CC) that arrives is bound to paramIndex. One CC per parameter; stored with the
        project by parameter ID. CC 64 (sustain) and 120-127 (channel mode) are not learnable. Any thread. */
    void startMidiLearn (int paramIndex) noexcept { learnTarget.store (paramIndex); }
    void cancelMidiLearn() noexcept { learnTarget.store (-1); }
    int getMidiLearnTarget() const noexcept { return learnTarget.load(); }
    int ccForParam (int paramIndex) const noexcept;
    void forgetMidiCc (int paramIndex) noexcept;
    void clearMidiLearn() noexcept;
    static bool isLearnableCc (int cc) noexcept { return cc >= 0 && cc < 120 && cc != 64; }
    /** Sends MIDI-controlled values to the host / UI (the processor timer calls it at 10 Hz). Message thread. */
    void flushMidiPending();

    /** Layers: layer A is the editable patch (its key / velocity range and transpose: getLayerARouting());
        layers B, C, D (index 0..2) are complete sounds with their own engine and effects. Message thread. */
    static constexpr int kExtraLayers = 3;
    SoundLayer& getLayer (int index) { return *layers[(size_t) index]; }
    LayerRouting& getLayerARouting() { return layerA; }
    /** EDIT: exchanges the sound of layer index (B..D) with the editable patch (layer A); settings stay with the slot. */
    void swapLayerWithMain (int index);

    /** Preset morphing: the Morph parameter blends every preset parameter of the editor's patch (0) towards the
        morph target (1); continuous parameters interpolate (in their normalised range, so frequencies and times move
        geometrically), switches and choices change at 50 %. Resources (wavetables, samples, FM banks, matrix and rack
        order) stay those of the editor's patch. Saved with the project. Message thread. */
    void setMorphTarget (const PatchSpec& spec);
    void clearMorphTarget();
    bool hasMorphTarget() const noexcept { return morphLoaded.load(); }
    juce::String getMorphTargetName() const { return morphName; }
    const PatchSpec& getMorphTarget() const noexcept { return morphSpec; }

    /** Microtuning (Scala). Empty texts = 12-TET. Returns false (and keeps the current tuning) on a parse error.
        Stored with the project (not in presets). Message thread. */
    bool setTuning (const juce::String& sclText, const juce::String& kbmText, juce::String& error);
    juce::String getTuningName() const { return tuningName; }
    juce::String getTuningScl() const { return tuningScl; }
    juce::String getTuningKbm() const { return tuningKbm; }
    const tuning::Table& getTuningTable() const { return tuningTable; }
    /** Output metering: true once the output safety stage engaged (any sample above -1 dBFS) since the last reset. */
    bool getClipLatched() const noexcept { return clipLatched.load(); }
    void resetClipLatch() noexcept { clipLatched.store (false); }
    std::atomic<bool> clipLatched { false };
    std::shared_ptr<int> aliveToken = std::make_shared<int> (0);
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
    /** Common asynchronous file-chooser / drop path. Changes the mode only after a successful load. */
    bool importSampleFile (int oscillator, const juce::File& file);
    bool importSampleFiles (int oscillator, const juce::StringArray& files);
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
    /** Message thread (tests, offline tools): waits until every queued import has been committed or rejected. */
    bool waitForImports (int timeoutMs = 20000);
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
    juce::String tuningScl, tuningKbm, tuningName { "12-TET" };
    // MIDI Learn: CC -> parameter index (-1 = none); values from MIDI reach the engine in the same block (raw parameter
    // value) and the host / UI from the message thread (midiPending, flushed by timerCallback with a gesture)
    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnTarget { -1 };
    std::array<std::atomic<float>, pid::kNumParams> midiPending;
    void applyMidiControllers (const juce::MidiBuffer& midi) noexcept;
    tuning::Table tuningTable = tuning::standard();
    EffectsRack rack;
    std::array<std::unique_ptr<SoundLayer>, 3> layers { std::make_unique<SoundLayer>(), std::make_unique<SoundLayer>(), std::make_unique<SoundLayer>() };
    LayerRouting layerA;
    std::array<std::atomic<float>, pid::kNumParams> morphValues;
    std::atomic<bool> morphLoaded { false };
    PatchSpec morphSpec;
    juce::String morphName;
    void applyMorph (ParamSnapshot& s, float amount) const noexcept;
    juce::MidiBuffer routedA;
    juce::AudioBuffer<float> layerSum; // layers B..D after their own Preset Volume, level and pan
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
    // preset change / PANIC: the audio thread silences everything at the start of the next block
    std::atomic<bool> silenceRequest { false };
    float lastOutL = 0.0f, lastOutR = 0.0f;
    int fadeRemaining = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterOutGain; // linear: reaches exact silence
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
    void startImport (int osc, const juce::String& sourceId, ImportKind kind, const juce::String& displayName, ImportCallback done);
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
