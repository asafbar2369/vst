#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace vsx
{
/**
    AI connection settings. Three operating modes:
      - OFFLINE: built-in rule-based designer (no network, no account, always available)
      - LOCAL:   a model running on this computer or the local network (Ollama, LM Studio,
                 any OpenAI-compatible server on a loopback / private address)
      - CLOUD:   Anthropic Claude, OpenAI or another HTTPS OpenAI-compatible service
    API keys are never stored here (see SecureStorage).
*/
struct AISettings
{
    enum Provider { Offline = 0, Anthropic = 1, OpenAICompatible = 2, OpenAI = 3, Ollama = 4, LMStudio = 5, NumProviders = 6 };
    int provider = Offline;

    juce::String anthropicBaseUrl { "https://api.anthropic.com" };
    juce::String anthropicModel { "claude-opus-5-5" };   // default; the live list comes from GET /v1/models
    juce::String effort { "medium" };                    // Anthropic output_config.effort
    bool serverSideFallback = true;                      // Anthropic "fallbacks": "default" (refusal fallback)

    juce::String openAiCloudBaseUrl { "https://api.openai.com/v1" };
    juce::String openAiCloudModel;                       // chosen from GET /v1/models (no hard-coded default)

    juce::String ollamaBaseUrl { "http://localhost:11434" };   // native API (/api/chat, /api/tags)
    juce::String ollamaModel;                            // empty = first installed model

    juce::String lmStudioBaseUrl { "http://localhost:1234/v1" }; // OpenAI-compatible server
    juce::String lmStudioModel;                          // empty = first loaded model

    juce::String openAiBaseUrl { "http://localhost:8080/v1" };   // any other OpenAI-compatible server
    juce::String openAiModel;

    int timeoutSeconds = 180;
    int maxRetries = 2;
    bool fallbackToOffline = true;       // use the built-in designer if the AI service fails
    bool autoPreview = true;             // audition generated sounds automatically
    bool optimiseAfterGenerate = true;   // render-and-measure refinement loop
    bool shareReferenceAnalysis = false; // consent: send reference-audio *analysis numbers* (never audio) to cloud providers
    int matchIterations = 40;            // sound-matching budget (accuracy vs. time)

    static juce::String providerName (int p);
    static juce::String providerKeyId (int p);
    juce::String providerId() const { return providerKeyId (provider); }
    juce::String baseUrl() const;
    juce::String model() const;
    void setModel (const juce::String& m);
    bool isCloud() const;
    bool isLocal() const;
    juce::String modeLabel() const { return provider == Offline ? "OFFLINE" : isCloud() ? "CLOUD" : "LOCAL"; }
    bool requiresApiKey() const { return provider == Anthropic || provider == OpenAI; }

    juce::var toVar() const;
    static AISettings fromVar (const juce::var& v);
    static juce::File settingsFile();
    /** Redirects settings storage (tests use a temporary folder so real user settings are never touched). */
    static void setSettingsFolderOverride (const juce::File& folder);
    void save() const;
    static AISettings load();
};

/** True for loopback and private (RFC 1918 / link-local / .local) hosts. */
bool isLocalHost (const juce::String& url);
/** Remote providers must use HTTPS; plain HTTP is accepted only for local / private-network hosts. */
bool isUrlAllowed (const juce::String& url, juce::String& error);

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
    bool cancelled = false;
    int attempts = 0;
    int inputTokens = 0, outputTokens = 0;
};

struct ModelInfo
{
    juce::String id, displayName;
    int structuredOutputs = -1; // -1 unknown, 0 no, 1 yes (reported by the provider)
};

struct ModelListResult
{
    bool ok = false;
    int httpStatus = 0;
    juce::String error;
    std::vector<ModelInfo> models;
    double latencyMs = 0.0;
};

/** Blocking HTTP used by the providers (worker thread only). Cancellable from any thread via the flag. */
struct HttpResponse { int status = 0; juce::String body; juce::String error; juce::StringPairArray headers; bool cancelled = false; };
HttpResponse httpRequest (const juce::String& url, const juce::StringPairArray& headers, const juce::String* postBody, int timeoutMs,
                          const std::atomic<bool>* cancel);
inline HttpResponse httpPostJson (const juce::String& url, const juce::StringPairArray& headers, const juce::String& body, int timeoutMs,
                                  const std::atomic<bool>* cancel = nullptr)
{ return httpRequest (url, headers, &body, timeoutMs, cancel); }
inline HttpResponse httpGet (const juce::String& url, const juce::StringPairArray& headers, int timeoutMs, const std::atomic<bool>* cancel = nullptr)
{ return httpRequest (url, headers, nullptr, timeoutMs, cancel); }

/** Extracts the first top-level JSON object from free text (handles ```json fences). */
juce::var extractJsonObject (const juce::String& text);

class AIProvider
{
public:
    virtual ~AIProvider() = default;
    virtual juce::String name() const = 0;
    virtual AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) = 0;
    /** Lists the models the service offers (no tokens are used). */
    virtual ModelListResult listModels (const juce::String& apiKey, const std::atomic<bool>& cancel) = 0;
    /** Connection test: lists models and checks that the selected one is available. */
    AIResult diagnose (const juce::String& apiKey, const std::atomic<bool>& cancel);
    virtual juce::String selectedModel() const = 0;
};

/** Anthropic Messages API (raw HTTPS; structured JSON via output_config.format; models via GET /v1/models). */
class AnthropicProvider : public AIProvider
{
public:
    explicit AnthropicProvider (const AISettings& s) : settings (s) {}
    juce::String name() const override { return "Claude (" + settings.anthropicModel + ")"; }
    AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    ModelListResult listModels (const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    juce::String selectedModel() const override { return settings.anthropicModel; }
    juce::String buildBody (const AIRequest& request) const; // exposed for tests
    static constexpr const char* kApiVersion = "2026-10-21";

private:
    juce::StringPairArray headers (const juce::String& apiKey) const;
    AISettings settings;
};

/** OpenAI Chat Completions protocol: OpenAI, LM Studio and other compatible servers. */
class ChatCompletionsProvider : public AIProvider
{
public:
    explicit ChatCompletionsProvider (const AISettings& s) : settings (s) {}
    juce::String name() const override;
    AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    ModelListResult listModels (const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    juce::String selectedModel() const override { return settings.model(); }
    enum class Format { JsonSchema, JsonObject, None };
    juce::String buildBody (const AIRequest& request, const juce::String& model, Format format) const;

private:
    AISettings settings;
};

/** Ollama native API: POST /api/chat with "format" = JSON schema, GET /api/tags. */
class OllamaProvider : public AIProvider
{
public:
    explicit OllamaProvider (const AISettings& s) : settings (s) {}
    juce::String name() const override { return "Ollama (" + (settings.ollamaModel.isNotEmpty() ? settings.ollamaModel : juce::String ("auto")) + ")"; }
    AIResult send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    ModelListResult listModels (const juce::String& apiKey, const std::atomic<bool>& cancel) override;
    juce::String selectedModel() const override { return settings.ollamaModel; }
    juce::String buildBody (const AIRequest& request, const juce::String& model) const;

private:
    AISettings settings;
};

std::unique_ptr<AIProvider> createProvider (const AISettings& s);

/** Detects unsupported Hebrew input/output for the English-only AI policy. */
bool containsHebrew (const juce::String& text);

} // namespace vsx
