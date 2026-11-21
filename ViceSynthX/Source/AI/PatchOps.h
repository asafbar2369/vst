#pragma once

#include "../Presets/PresetFormat.h"

namespace vsx
{
struct OpReport
{
    int applied = 0;
    juce::StringArray warnings, errors;
    juce::String summary() const;
};

/** Outcome of one sound-design request (LLM or offline designer). */
struct DesignResult
{
    juce::String reply;            // explanation shown in the chat
    juce::String question;         // clarification question (if any)
    bool hasPatch = false;
    PatchSpec patch;
    std::vector<PatchSpec> variations;
    OpReport report;
    juce::StringArray descriptors; // measurable intents ("darker", "wider"...) for the refinement loop
    juce::String engine;           // "Offline designer" / provider name
};

/**
    Strictly validated, transaction-based patch operations. No code is ever executed:
    operations are data (set parameter, add modulation, choose wavetable, ...) that are
    checked against the parameter registry, clamped or rejected, and applied to a copy
    of the patch which is then committed as a single undoable step.
*/
class OfflineRenderer;

namespace PatchOps
{
    juce::var responseSchema();
    juce::String systemPrompt();
    juce::String contextFor (const PatchSpec& current, const juce::String& referenceSummary);
    /** Agent tool: renders the patch offline (C4, velocity 100) and describes the measured result in plain numbers
        (level, pitch, brightness and its evolution, noisiness, envelope, stereo image, warnings). */
    juce::String renderReport (const PatchSpec& patch, OfflineRenderer& renderer);

    /** Applies operations to spec. Invalid operations are skipped and reported. */
    OpReport apply (PatchSpec& spec, const juce::var& operations);

    /** Converts a structured AI response into a DesignResult (validating every operation). */
    DesignResult fromResponse (const juce::var& response, const PatchSpec& current, const juce::String& prompt);

    /** Starting point for "new patch" requests. */
    PatchSpec initPatch();
}

} // namespace vsx
