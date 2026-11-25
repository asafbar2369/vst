#include "../Source/AI/PatchOps.h"
#include "../Source/Presets/SoundRecipes.h"

/*
    Prompt size for local models. Ollama and LM Studio often run 4,096-token contexts; the full system prompt measured
    about 15,600 tokens with a real local model (llama.cpp server, Qwen2.5 1.5B tokenizer, 2026-11-22) and was rejected.
    The compact prompt (local providers) must leave room for the patch context, the request and the answer.
    VSX_DUMP_DIR: writes both prompts and an INIT context for token counting with a real tokenizer.
*/
namespace vsx
{
class AIPromptSizeTests final : public juce::UnitTest
{
public:
    AIPromptSizeTests() : juce::UnitTest ("AI prompt size", "VICE SYNTH X") {}

    void runTest() override
    {
        beginTest ("Compact system prompt for local models");
        const auto full = PatchOps::systemPrompt (false), compact = PatchOps::systemPrompt (true);
        const auto ctx = PatchOps::contextFor (Recipes::init ("Init", "Test"), {});
        logMessage ("MEASURE|ai prompt|full system prompt|" + juce::String (full.length()) + "|chars");
        logMessage ("MEASURE|ai prompt|compact system prompt|" + juce::String (compact.length()) + "|chars");
        logMessage ("MEASURE|ai prompt|patch context (INIT)|" + juce::String (ctx.length()) + "|chars");
        if (const auto dir = juce::SystemStats::getEnvironmentVariable ("VSX_DUMP_DIR", {}); dir.isNotEmpty())
        {
            juce::File (dir).getChildFile ("prompt_full.txt").replaceWithText (full);
            juce::File (dir).getChildFile ("prompt_compact.txt").replaceWithText (compact);
            juce::File (dir).getChildFile ("context_init.txt").replaceWithText (ctx);
            juce::File (dir).getChildFile ("schema.txt").replaceWithText (juce::JSON::toString (PatchOps::responseSchema(), true));
        }
        // every parameter id the compact prompt lists must be a real parameter (no stale ids)
        int lines = 0, unknown = 0;
        bool inParams = false;
        for (const auto& line : juce::StringArray::fromLines (compact))
        {
            if (line.startsWith ("PARAMETERS")) { inParams = true; continue; }
            if (! inParams || line.isEmpty()) continue;
            ++lines;
            if (ParamRegistry::get().indexOf (line.upToFirstOccurrenceOf (" ", false, false)) < 0) ++unknown;
        }
        logMessage ("MEASURE|ai prompt|compact parameter lines|" + juce::String (lines) + "|lines");
        expectEquals (unknown, 0, "unknown parameter ids in the compact prompt");
        expect (compact.length() * 2 < full.length(), "compact prompt is less than half the full prompt");
    }
};

static AIPromptSizeTests aiPromptSizeTests;
} // namespace vsx
