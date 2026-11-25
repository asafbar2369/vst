#include "../Source/AI/AIProvider.h"
#include "../Source/AI/PatchOps.h"
#include "../Source/Analysis/SoundMatcher.h"

/*
    LIVE provider tests. They send real requests and are skipped unless explicitly enabled:
      VSX_LIVE_ANTHROPIC=1   (key from ANTHROPIC_API_KEY)            optional VSX_LIVE_ANTHROPIC_MODEL
      VSX_LIVE_OPENAI=1      (key from OPENAI_API_KEY, or a proxy-injected credential) optional VSX_LIVE_OPENAI_MODEL
      VSX_LIVE_OLLAMA=<base url, e.g. http://localhost:11434>       optional VSX_LIVE_OLLAMA_MODEL
      VSX_LIVE_LMSTUDIO=<base url, e.g. http://localhost:1234/v1>   optional VSX_LIVE_LMSTUDIO_MODEL
    Each enabled provider: list models, connection test, two English design requests, validation of the
    returned operations, and an offline render of the resulting patch. Results are printed as LIVE|... lines.
*/
namespace vsx
{
class LiveProviderTests final : public juce::UnitTest
{
public:
    LiveProviderTests() : juce::UnitTest ("Live AI providers", "VICE SYNTH X") {}

    static juce::String env (const char* name) { return juce::SystemStats::getEnvironmentVariable (name, {}); }

    void runProvider (AISettings s, const juce::String& key, const juce::String& label)
    {
        beginTest ("LIVE " + label);
        std::atomic<bool> cancel { false };
        auto provider = createProvider (s);
        const auto list = provider->listModels (key, cancel);
        logMessage ("LIVE|" + label + "|models|" + (list.ok ? juce::String ((int) list.models.size()) : "error: " + list.error) + "|"
                    + juce::String ((int) list.latencyMs) + " ms");
        expect (list.ok, label + " model listing: " + list.error);
        if (! list.ok) return;
        if (s.model().isEmpty() && ! list.models.empty() && s.provider == AISettings::OpenAI)
        {
            for (const auto& m : list.models) if (m.id.startsWith ("gpt")) { s.setModel (m.id); break; }
            provider = createProvider (s);
        }
        const auto diag = provider->diagnose (key, cancel);
        logMessage ("LIVE|" + label + "|connection test|" + (diag.ok ? "ok" : "failed: " + diag.error) + "|" + diag.raw);

        const juce::StringArray prompts { "Create a warm early-1990s digital pad", "Create a deep analog bass" };
        auto renderer = std::make_unique<OfflineRenderer> (44100.0);
        for (const auto& prompt : prompts)
        {
            AIRequest req;
            req.systemPrompt = PatchOps::systemPrompt (! s.isCloud()); // as AIController: compact for local servers
            req.compactPrompt = ! s.isCloud();
            req.context = PatchOps::contextFor (PatchOps::initPatch(), {});
            req.userMessage = prompt;
            req.responseSchema = PatchOps::responseSchema();
            const auto r = provider->send (req, key, cancel);
            juce::String line = "LIVE|" + label + "|" + prompt + "|";
            if (! r.ok)
            {
                logMessage (line + "request failed: " + r.error + " (HTTP " + juce::String (r.httpStatus) + ")");
                expect (false, label + ": " + r.error);
                continue;
            }
            const auto design = PatchOps::fromResponse (r.json, PatchOps::initPatch(), prompt);
            float peak = 0.0f;
            if (design.hasPatch)
            {
                const auto out = renderer->render (design.patch, 60, 100, 1.5, 0.5, 1);
                peak = out.getMagnitude (0, out.getNumSamples());
            }
            logMessage (line + "model " + r.modelUsed + ", " + juce::String (r.latencyMs / 1000.0, 1) + " s, tokens " + juce::String (r.inputTokens)
                        + "/" + juce::String (r.outputTokens) + ", patch " + (design.hasPatch ? "yes" : "no") + ", applied ops "
                        + juce::String (design.report.applied) + ", rejected " + juce::String (design.report.errors.size())
                        + ", render peak " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS, reply: "
                        + design.reply.substring (0, 160).replace ("\n", " "));
            if (design.report.errors.size() > 0 || ! design.hasPatch) logMessage ("LIVE|" + label + "|raw reply|" + r.raw.substring (0, 1500).replace ("\n", " "));
            for (int e = 0; e < std::min (4, design.report.errors.size()); ++e)
                logMessage ("LIVE|" + label + "|rejected operation|" + design.report.errors[e].substring (0, 200));
            expect (design.hasPatch || design.question.isNotEmpty(), label + ": no playable result");
            if (design.hasPatch) expect (peak > 1.0e-4f && std::isfinite (peak), label + ": generated patch is silent or invalid");
        }
    }

    void runTest() override
    {
        bool any = false;
        if (env ("VSX_LIVE_ANTHROPIC").isNotEmpty())
        {
            AISettings s;
            s.provider = AISettings::Anthropic;
            if (env ("VSX_LIVE_ANTHROPIC_MODEL").isNotEmpty()) s.anthropicModel = env ("VSX_LIVE_ANTHROPIC_MODEL");
            runProvider (s, env ("ANTHROPIC_API_KEY"), "Anthropic");
            any = true;
        }
        if (env ("VSX_LIVE_OPENAI").isNotEmpty())
        {
            AISettings s;
            s.provider = AISettings::OpenAI;
            s.openAiCloudModel = env ("VSX_LIVE_OPENAI_MODEL");
            const auto key = env ("OPENAI_API_KEY").isNotEmpty() ? env ("OPENAI_API_KEY") : juce::String ("proxy-injected");
            runProvider (s, key, "OpenAI");
            any = true;
        }
        if (env ("VSX_LIVE_OLLAMA").isNotEmpty())
        {
            AISettings s;
            s.provider = AISettings::Ollama;
            s.ollamaBaseUrl = env ("VSX_LIVE_OLLAMA");
            s.ollamaModel = env ("VSX_LIVE_OLLAMA_MODEL");
            s.timeoutSeconds = 900;
            runProvider (s, {}, "Ollama");
            any = true;
        }
        if (env ("VSX_LIVE_LMSTUDIO").isNotEmpty())
        {
            AISettings s;
            s.provider = AISettings::LMStudio;
            s.lmStudioBaseUrl = env ("VSX_LIVE_LMSTUDIO");
            s.lmStudioModel = env ("VSX_LIVE_LMSTUDIO_MODEL");
            s.timeoutSeconds = 900; // CPU-only local inference with schema-constrained decoding is slow
            runProvider (s, {}, "LM Studio");
            any = true;
        }
        if (! any)
        {
            beginTest ("Live providers (skipped: no VSX_LIVE_* variable set)");
            expect (true);
        }
    }
};

static LiveProviderTests liveProviderTests;
} // namespace vsx
