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
    bool undo() override;
    bool redo() override;
    bool canUndo() const override { return ! undoStack.empty(); }
    bool canRedo() const override { return ! redoStack.empty(); }
    void audition (int note, int velocity, double seconds) override;
    PresetManager& getPresetManager() override { return presets; }

    /** Lock-free MIDI injection from the GUI (wheels, pads). */
    void injectMidi (const juce::MidiMessage& m);

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
    ParamSnapshot snapshot;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViceSynthProcessor)
};

} // namespace vsx
