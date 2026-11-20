#pragma once

#include "../Core/PatchState.h"

namespace vsx
{
struct PresetMeta
{
    juce::String name { "Init" };
    juce::String author { "User" };
    juce::String category { "Analog" };
    juce::StringArray tags;
    juce::String description;
    juce::String source { "user" };   // "factory" | "user" | "ai"
    juce::String prompt;              // AI prompt that produced the preset (if any)
    juce::String uuid;
    juce::Time created, modified;
};

/** Modulation route as stored in a patch (amount is copied to the slot's amount parameter). */
struct ModRoute
{
    int source = 0;
    int dest = -1;
    float amount = 0.0f;
    int polarity = 0;
    int aux = 0;
    float curve = 0.0f;
};

/**
    Complete, self-contained description of a sound: every parameter value plus all non-parameter
    state (matrix routing, FX order, oscillator resources, LFO curves, arp/seq patterns).
    Used by presets, the AI layer (transactions), undo snapshots and offline rendering.
*/
struct PatchSpec
{
    PatchSpec();

    std::array<float, pid::kNumParams> values {};
    std::vector<ModRoute> mods;                         // max kNumModSlots, slot index = position
    std::array<int, pid::NumFx> fxOrder {};
    std::array<juce::String, kNumOscs> wavetableId, sampleId, multiId;
    std::array<std::shared_ptr<const Wavetable>, kNumOscs> customWavetable; // for "custom:" ids
    std::array<LfoCurve, kNumLfos> lfoCurves;
    ArpPattern arp;
    SeqPattern seq;
    juce::String userIr;
    PresetMeta meta;

    // ---- convenience (real units; choice values by index) ------------------------------------
    float get (const juce::String& id) const;
    bool set (const juce::String& id, float value);
    bool setChoice (const juce::String& id, const juce::String& choiceName);
    PatchSpec& p (const juce::String& id, float value) { set (id, value); return *this; }
    PatchSpec& c (const juce::String& id, const juce::String& choice) { setChoice (id, choice); return *this; }
    PatchSpec& addMod (int source, const juce::String& destId, float amount, int polarity = 0, int aux = 0, float curve = 0.0f);
    PatchSpec& addMod (const juce::String& source, const juce::String& destId, float amount, int polarity = 0);
    int findMod (int source, int dest) const;
    void removeMod (int source, int dest);

    /** Non-parameter state in the PatchState JSON layout (see PatchState::toVar). */
    juce::var stateToVar() const;
    void stateFromVar (const juce::var& v);

    /** Full preset document. */
    juce::var toPresetVar() const;
    static bool fromPresetVar (const juce::var& v, PatchSpec& out, juce::String& error);

    juce::String toJson() const;
    static bool fromJson (const juce::String& json, PatchSpec& out, juce::String& error);

    /** Human-readable summary of non-default settings (used for the AI context and tests). */
    juce::String summarise (int maxItems = 120) const;
};

namespace PresetFormat
{
    constexpr int kVersion = 1;
    const juce::String kFormatId = "VICE-SYNTH-X-PRESET";
    const juce::String kExtension = ".vsxpreset";
    const juce::String kPackExtension = ".vsxpack";
}

} // namespace vsx
