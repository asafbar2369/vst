#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace vsx
{
struct AISettings
{
    enum Provider { Offline = 0, Anthropic = 1, OpenAICompatible = 2 };
    int provider = Offline;
    juce::String anthropicBaseUrl { "https://api.anthropic.com" };
    juce::String anthropicModel { "claude-opus-5-5" };
    juce::String effort { "medium" };               // Anthropic output_config.effort
    bool serverSideFallback = true;                 // Anthropic "fallbacks": "default" (refusal fallback)
    juce::String openAiBaseUrl { "http://localhost:11434/v1" }; // e.g. Ollama / LM Studio / any compatible service
    juce::String openAiModel { "llama3.1" };
    int timeoutSeconds = 180;
    int maxRetries = 2;
    bool fallbackToOffline = true;   // use the built-in designer if the AI service fails
    bool autoPreview = true;         // audition generated sounds automatically
    bool optimiseAfterGenerate = true; // run the measurable-target refinement loop

    juce::String providerId() const { return provider == Anthropic ? "anthropic" : provider == OpenAICompatible ? "openai-compatible" : "offline"; }
    juce::var toVar() const;
    static AISettings fromVar (const juce::var& v);
    static juce::File settingsFile();
    void save() const;
    static AISettings load();
};

struct AIMessage
{
    juce::String role; // "user" | "assistant"
    juce::String text;
};

struct AIRequest
{
    juce::String systemPrompt;           // large, stable (cached) part
    juce::String context;                // per-request context (current patch summary, reference analysis)
    std::vector<AIMessage> history;
    juce::String userMessage;
    juce::var responseSchema;            // JSON schema for the structured response
};

struct AIResult
{
    bool ok = false;
    int httpStatus = 0;
    juce::String error;
    juce::var json;          // parsed structured response
    juce::String raw;
    juce::String modelUsed;
    double latencyMs = 0.0;
    bool refused = false;
    int attempts = 0;
    int inputTokens = 0, outputTokens = 0;
};

/** Minimal blocking HTTP POST used by the providers (runs on a worker thread only). */
struct HttpResponse { int status = 0; juce::String body; juce::String error; juce::StringPairArray headers; };
HttpResponse httpPostJson (const juce::String& url, const juce::StringPairArray& headers, const juce::String& body, int timeoutMs);
HttpResponse httpGet (const juce::String& url, const juce::StringPairArray& headers, int timeoutMs);

/** Extracts the first top-level JSON object from free text (handles ```json fences). */
juce::var extractJsonObject (const juce::String& text);

class AIProvider
{
public:
    virtual ~AIProvider() = default;
    virtual juce::String name() const = 0;
    virtual AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) = 0;
    /** Lightweight request used by "Test connection". */
    virtual AIResult diagnose (const juce::String& apiKey) = 0;
};

/** Anthropic Messages API (raw HTTPS; structured JSON via output_config.format). */
class AnthropicProvider : public AIProvider
{
public:
    explicit AnthropicProvider (const AISettings& s) : settings (s) {}
    juce::String name() const override { return "Anthropic Claude (" + settings.anthropicModel + ")"; }
    AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    AIResult diagnose (const juce::String& apiKey) override;
    juce::String buildBody (const AIRequest& request) const; // exposed for tests

private:
    AIResult post (const juce::String& body, const juce::String& apiKey, const std::atomic<bool>& cancel, bool structured);
    AISettings settings;
};

/** OpenAI-compatible Chat Completions (cloud services and local servers such as Ollama / LM Studio / llama.cpp). */
class OpenAICompatibleProvider : public AIProvider
{
public:
    explicit OpenAICompatibleProvider (const AISettings& s) : settings (s) {}
    juce::String name() const override { return "OpenAI-compatible (" + settings.openAiModel + ")"; }
    AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    AIResult diagnose (const juce::String& apiKey) override;
    juce::String buildBody (const AIRequest& request) const;

private:
    AISettings settings;
};

std::unique_ptr<AIProvider> createProvider (const AISettings& s);

} // namespace vsx
