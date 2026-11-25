#pragma once

#include "../Presets/PresetFormat.h"
#include <atomic>

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
class AIProvider;
struct AIRequest;
struct AIResult;

namespace PatchOps
{
    juce::var responseSchema();
    /** compact: for local models with small context windows (Ollama / LM Studio often default to 4,096 tokens):
        the same rules and operations, parameters as "id range" lines without descriptions, effect parameters only
        for the most used effects (every effect keeps its on / mix switch). */
    juce::String systemPrompt (bool compact = false);
    juce::String contextFor (const PatchSpec& current, const juce::String& referenceSummary);
    /** Agent tool: renders the patch offline (C4, velocity 100) and describes the measured result in plain numbers
        (level, pitch, brightness and its evolution, noisiness, envelope, stereo image, warnings). */
    juce::String renderReport (const PatchSpec& patch, OfflineRenderer& renderer);

    /** Applies operations to spec. Invalid operations are skipped and reported. */
    OpReport apply (PatchSpec& spec, const juce::var& operations);

    /** Converts a structured AI response into a DesignResult (validating every operation). */
    DesignResult fromResponse (const juce::var& response, const PatchSpec& current, const juce::String& prompt);

    /** Sends req and validates the answer. When the model's operations were rejected (the transaction is never
        applied in part), the exact errors are returned to the model once and its corrected answer is validated the
        same way. last receives the provider result that produced the returned design; repairs is 0 or 1. */
    DesignResult designWithRepair (AIProvider& provider, const AIRequest& req, const juce::String& apiKey,
                                   const PatchSpec& current, const juce::String& prompt, const std::atomic<bool>& cancel,
                                   AIResult& last, int& repairs);

    /** Starting point for "new patch" requests. */
    PatchSpec initPatch();
}

} // namespace vsx
