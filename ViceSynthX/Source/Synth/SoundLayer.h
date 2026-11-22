#pragma once

#include "SynthEngine.h"
#include "../FX/EffectsRack.h"
#include "../Presets/PresetFormat.h"

/**
    Layers: up to four sounds play together (layer A is the editable patch; B, C, D are complete sounds with their own
    engine and effects rack). Each layer has a key range, a velocity range, a transpose, a level and a pan, so layers
    give keyboard splits, velocity layers and stacks (for example a sampled attack over a synthesised sustain).

    Threading: settings are atomics; a layer's sound is set on the message thread (PatchState loads resources the same
    way as the main patch; parameter values are atomics copied into the audio thread's snapshot each block). The audio
    thread never allocates: buffers are sized in prepare().
*/
namespace vsx
{
/** Key / velocity / transpose routing of one layer, with per-key memory of the note each key started (a note-off
    releases the right note even if the transpose changed while the key was held). */
struct LayerRouting
{
    std::atomic<int> keyLo { 0 }, keyHi { 127 }, velLo { 1 }, velHi { 127 }, transpose { 0 };
    std::array<int, 16 * 128> started {}; // per channel and key: the note this layer started, -1 = none (audio thread)
    LayerRouting() { started.fill (-1); }
    void reset() noexcept { started.fill (-1); }
    /** Filters / transposes in into out (audio thread). Non-note messages pass unchanged. */
    void route (const juce::MidiBuffer& in, juce::MidiBuffer& out) noexcept;
};

class SoundLayer
{
public:
    SoundLayer();
    void prepare (double sampleRate, int maxBlock);

    // ---- message thread -------------------------------------------------------------------------------------
    void setSound (const PatchSpec& spec);   // silences the layer first (no tail of the previous sound)
    const PatchSpec& getSound() const noexcept { return sound; }
    bool hasSound() const noexcept { return loaded; }
    void clearSound();

    // ---- settings (any thread) ------------------------------------------------------------------------------
    std::atomic<bool> enabled { false };
    std::atomic<float> levelDb { 0.0f }, pan { 0.0f };
    LayerRouting routing;
    void requestSilence() noexcept { silence.store (true); }

    // ---- audio thread ---------------------------------------------------------------------------------------
    /** Renders n samples (n <= the prepared chunk) and adds them into outL / outR. */
    void processChunk (const juce::MidiBuffer& midi, int n, const TransportInfo& t, float* outL, float* outR) noexcept;
    SynthEngine& getEngine() noexcept { return engine; }

private:
    PatchState state;
    SynthEngine engine { state };
    EffectsRack rack;
    std::array<std::atomic<float>, pid::kNumParams> values;
    ParamSnapshot snap;
    juce::AudioBuffer<float> buf;
    juce::MidiBuffer routed;
    std::atomic<bool> silence { false };
    PatchSpec sound;
    bool loaded = false;
    juce::String userIr;
};
} // namespace vsx
