#include "AIProvider.h"
#include <thread>

namespace vsx
{
// =================================================================================
// Settings
// =================================================================================
juce::String AISettings::providerName (int p)
{
    switch (p)
    {
        case Anthropic:        return "Anthropic Claude";
        case OpenAI:           return "OpenAI";
        case Ollama:           return "Ollama (local)";
        case LMStudio:         return "LM Studio (local)";
        case OpenAICompatible: return "Other OpenAI-compatible server";
        default:               return "Offline rule-based designer";
    }
}

juce::String AISettings::providerKeyId (int p)
{
    switch (p)
    {
        case Anthropic:        return "anthropic";
        case OpenAI:           return "openai";
        case Ollama:           return "ollama";
        case LMStudio:         return "lmstudio";
        case OpenAICompatible: return "openai-compatible";
        default:               return "offline";
    }
}

juce::String AISettings::baseUrl() const
{
    switch (provider)
    {
        case Anthropic:        return anthropicBaseUrl;
        case OpenAI:           return openAiCloudBaseUrl;
        case Ollama:           return ollamaBaseUrl;
        case LMStudio:         return lmStudioBaseUrl;
        case OpenAICompatible: return openAiBaseUrl;
        default:               return {};
    }
}

juce::String AISettings::model() const
{
    switch (provider)
    {
        case Anthropic:        return anthropicModel;
        case OpenAI:           return openAiCloudModel;
        case Ollama:           return ollamaModel;
        case LMStudio:         return lmStudioModel;
        case OpenAICompatible: return openAiModel;
        default:               return {};
    }
}

void AISettings::setModel (const juce::String& m)
{
    switch (provider)
    {
        case Anthropic:        anthropicModel = m; break;
        case OpenAI:           openAiCloudModel = m; break;
        case Ollama:           ollamaModel = m; break;
        case LMStudio:         lmStudioModel = m; break;
        case OpenAICompatible: openAiModel = m; break;
        default: break;
    }
}

bool AISettings::isCloud() const
{
    if (provider == Offline) return false;
    if (provider == Anthropic || provider == OpenAI) return true;
    return ! isLocalHost (baseUrl());
}

bool AISettings::isLocal() const { return provider != Offline && ! isCloud(); }

juce::var AISettings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("provider", provider);
    o->setProperty ("anthropicBaseUrl", anthropicBaseUrl);
    o->setProperty ("anthropicModel", anthropicModel);
    o->setProperty ("effort", effort);
    o->setProperty ("serverSideFallback", serverSideFallback);
    o->setProperty ("openAiCloudBaseUrl", openAiCloudBaseUrl);
    o->setProperty ("openAiCloudModel", openAiCloudModel);
    o->setProperty ("ollamaBaseUrl", ollamaBaseUrl);
    o->setProperty ("ollamaModel", ollamaModel);
    o->setProperty ("lmStudioBaseUrl", lmStudioBaseUrl);
    o->setProperty ("lmStudioModel", lmStudioModel);
    o->setProperty ("openAiBaseUrl", openAiBaseUrl);
    o->setProperty ("openAiModel", openAiModel);
    o->setProperty ("timeoutSeconds", timeoutSeconds);
    o->setProperty ("maxRetries", maxRetries);
    o->setProperty ("fallbackToOffline", fallbackToOffline);
    o->setProperty ("autoPreview", false);
    o->setProperty ("optimiseAfterGenerate", optimiseAfterGenerate);
    o->setProperty ("shareReferenceAnalysis", shareReferenceAnalysis);
    o->setProperty ("matchIterations", matchIterations);
    o->setProperty ("agentSteps", agentSteps);
    return juce::var (o);
}

AISettings AISettings::fromVar (const juce::var& v)
{
    AISettings s;
    if (! v.isObject()) return s;
    auto str = [&v] (const char* key, const juce::String& def) { return v.hasProperty (key) ? v[key].toString() : def; };
    s.provider = juce::jlimit (0, NumProviders - 1, (int) v.getProperty ("provider", s.provider));
    s.anthropicBaseUrl = str ("anthropicBaseUrl", s.anthropicBaseUrl);
    s.anthropicModel = str ("anthropicModel", s.anthropicModel);
    s.effort = str ("effort", s.effort);
    s.serverSideFallback = (bool) v.getProperty ("serverSideFallback", s.serverSideFallback);
    s.openAiCloudBaseUrl = str ("openAiCloudBaseUrl", s.openAiCloudBaseUrl);
    s.openAiCloudModel = str ("openAiCloudModel", s.openAiCloudModel);
    s.ollamaBaseUrl = str ("ollamaBaseUrl", s.ollamaBaseUrl);
    s.ollamaModel = str ("ollamaModel", s.ollamaModel);
    s.lmStudioBaseUrl = str ("lmStudioBaseUrl", s.lmStudioBaseUrl);
    s.lmStudioModel = str ("lmStudioModel", s.lmStudioModel);
    s.openAiBaseUrl = str ("openAiBaseUrl", s.openAiBaseUrl);
    s.openAiModel = str ("openAiModel", s.openAiModel);
    s.timeoutSeconds = juce::jlimit (5, 900, (int) v.getProperty ("timeoutSeconds", s.timeoutSeconds));
    s.maxRetries = juce::jlimit (0, 5, (int) v.getProperty ("maxRetries", s.maxRetries));
    s.fallbackToOffline = (bool) v.getProperty ("fallbackToOffline", s.fallbackToOffline);
    s.autoPreview = false;
    s.optimiseAfterGenerate = (bool) v.getProperty ("optimiseAfterGenerate", s.optimiseAfterGenerate);
    s.shareReferenceAnalysis = (bool) v.getProperty ("shareReferenceAnalysis", s.shareReferenceAnalysis);
    s.matchIterations = juce::jlimit (5, 400, (int) v.getProperty ("matchIterations", s.matchIterations));
    s.agentSteps = juce::jlimit (0, 4, (int) v.getProperty ("agentSteps", s.agentSteps));
    if (s.effort.isEmpty()) s.effort = "medium";
    return s;
}

static juce::File& settingsFolderOverride()
{
    static juce::File f;
    return f;
}

void AISettings::setSettingsFolderOverride (const juce::File& folder) { settingsFolderOverride() = folder; }

juce::File AISettings::settingsFile()
{
    auto dir = settingsFolderOverride() != juce::File() ? settingsFolderOverride()
                                                       : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("VICE SYNTH X");
    dir.createDirectory();
    return dir.getChildFile ("ai_settings.json");
}

void AISettings::save() const
{
    const auto f = settingsFile();
    const auto tmp = f.getSiblingFile (f.getFileName() + ".tmp");
    if (tmp.replaceWithText (juce::JSON::toString (toVar())))
        tmp.moveFileTo (f);
}

AISettings AISettings::load()
{
    return fromVar (juce::JSON::parse (settingsFile().loadFileAsString()));
}

// =================================================================================
// URL policy
// =================================================================================
bool isLocalHost (const juce::String& url)
{
    auto host = juce::URL (url).getDomain().toLowerCase().trim();
    host = host.upToFirstOccurrenceOf (":", false, false);   // strip a port if getDomain kept it
    if (host.startsWith ("[")) host = host.substring (1).upToFirstOccurrenceOf ("]", false, false);
    if (host.isEmpty()) return false;
    if (host == "localhost" || host == "::1" || host.endsWith (".localhost") || host.endsWith (".local") || host == "host.docker.internal")
        return true;
    juce::StringArray parts;
    parts.addTokens (host, ".", {});
    if (parts.size() == 4 && host.containsOnly ("0123456789."))
    {
        const int a = parts[0].getIntValue(), b = parts[1].getIntValue();
        return a == 127 || a == 10 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254);
    }
    return false;
}

bool isUrlAllowed (const juce::String& url, juce::String& error)
{
    const auto u = url.trim();
    if (u.isEmpty()) { error = "No server address configured."; return false; }
    if (u.startsWithIgnoreCase ("https://")) return true;
    if (u.startsWithIgnoreCase ("http://"))
    {
        if (isLocalHost (u)) return true;
        error = "Remote servers require HTTPS: refusing to send data over unencrypted HTTP to " + juce::URL (u).getDomain()
              + ". Use an https:// address; plain http is only allowed for this computer or the local network.";
        return false;
    }
    error = "Unsupported address \"" + u.substring (0, 80) + "\" (expected http:// or https://).";
    return false;
}

bool containsHebrew (const juce::String& text)
{
    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;
        if ((c >= 0x0590 && c <= 0x05FF) || (c >= 0xFB1D && c <= 0xFB4F)) return true;
    }
    return false;
}

// =================================================================================
// HTTP
// =================================================================================
static juce::String headerString (const juce::StringPairArray& headers)
{
    juce::String h;
    for (int i = 0; i < headers.size(); ++i)
        h << headers.getAllKeys()[i] << ": " << headers.getAllValues()[i] << "\r\n";
    return h;
}

HttpResponse httpRequest (const juce::String& url, const juce::StringPairArray& headers, const juce::String* postBody, int timeoutMs,
                          const std::atomic<bool>* cancel)
{
    HttpResponse r;
    juce::String policyError;
    if (! isUrlAllowed (url, policyError)) { r.error = policyError; return r; }
    if (cancel != nullptr && cancel->load()) { r.cancelled = true; r.error = "Cancelled."; return r; }

    juce::URL u (url);
    if (postBody != nullptr) u = u.withPOSTData (*postBody);
    juce::WebInputStream stream (u, postBody != nullptr);
    stream.withExtraHeaders (headerString (headers))
          .withConnectionTimeout (timeoutMs)
          .withNumRedirectsToFollow (0); // never follow redirects with credentials attached

    // A watcher aborts the blocking connect/read as soon as the caller cancels.
    std::atomic<bool> finished { false };
    std::thread watcher;
    if (cancel != nullptr)
        watcher = std::thread ([&]
        {
            while (! finished.load())
            {
                if (cancel->load()) { stream.cancel(); return; }
                std::this_thread::sleep_for (std::chrono::milliseconds (25));
            }
        });

    const bool connected = stream.connect (nullptr);
    r.status = stream.getStatusCode();
    r.headers = stream.getResponseHeaders();
    if (connected || r.status > 0)
    {
        juce::MemoryOutputStream body;
        constexpr juce::int64 maxBytes = 8 * 1024 * 1024; // bounded response size
        char buffer[16384];
        while (! stream.isExhausted())
        {
            const int n = stream.read (buffer, (int) sizeof (buffer));
            if (n <= 0) break;
            body.write (buffer, (size_t) n);
            if ((juce::int64) body.getDataSize() > maxBytes) { r.error = "Response too large."; break; }
            if (cancel != nullptr && cancel->load()) break;
        }
        r.body = body.toUTF8();
    }
    finished = true;
    if (watcher.joinable()) watcher.join();

    if (cancel != nullptr && cancel->load()) { r.cancelled = true; r.error = "Cancelled."; return r; }
    if (! connected && r.status == 0 && r.error.isEmpty())
        r.error = "Could not connect to " + u.getDomain() + " (server not running, wrong address, no network or timeout).";
    return r;
}

juce::var extractJsonObject (const juce::String& text)
{
    const auto s = text.toStdString();
    size_t start = s.find ('{');
    while (start != std::string::npos)
    {
        int depth = 0;
        bool inStr = false, esc = false;
        for (size_t i = start; i < s.size(); ++i)
        {
            const char c = s[i];
            if (inStr)
            {
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') inStr = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0)
            {
                juce::var v;
                if (juce::JSON::parse (juce::String::fromUTF8 (s.substr (start, i - start + 1).c_str()), v).wasOk() && v.isObject())
                    return v;
                break;
            }
        }
        start = s.find ('{', start + 1);
    }
    return {};
}

namespace
{
    bool waitOrCancel (int ms, const std::atomic<bool>& cancel)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
        while (juce::Time::getMillisecondCounter() < end)
        {
            if (cancel.load()) return false;
            juce::Thread::sleep (25);
        }
        return ! cancel.load();
    }

    bool isRetryable (const HttpResponse& r)
    {
        if (r.cancelled) return false;
        if (r.status == 0) return r.error.startsWith ("Could not connect"); // network error / timeout (not a policy error)
        if (r.status == 429)
            return ! r.body.contains ("enforced_spend_limit_reached") && ! r.body.contains ("insufficient_quota");
        return r.status == 408 || r.status == 409 || r.status >= 500; // includes 529 overloaded
    }

    juce::String apiErrorMessage (const HttpResponse& r)
    {
        juce::var v;
        if (r.body.isNotEmpty() && juce::JSON::parse (r.body, v).wasOk())
        {
            if (v["error"].isObject())
            {
                const auto type = v["error"]["type"].toString(), msg = v["error"]["message"].toString();
                return (type.isNotEmpty() ? type + ": " : juce::String()) + msg;
            }
            if (v["error"].isString()) return v["error"].toString();
        }
        if (r.error.isNotEmpty()) return r.error;
        return "HTTP " + juce::String (r.status) + (r.body.isNotEmpty() ? ": " + r.body.substring (0, 300) : juce::String());
    }

    juce::String friendlyError (const HttpResponse& r, const juce::String& service)
    {
        if (r.status == 401 || r.status == 403) return "Authentication failed (HTTP " + juce::String (r.status) + "): check the " + service + " API key in Settings > AI.";
        if (r.status == 404) return "Not found (HTTP 404) at " + service + ": check the server address and the model name. " + apiErrorMessage (r);
        if (r.status == 429) return "Rate limit or quota reached at " + service + " (HTTP 429). " + apiErrorMessage (r);
        return apiErrorMessage (r);
    }

    int retryDelayMs (const HttpResponse& r, int attempt)
    {
        const auto ra = r.headers.getValue ("retry-after", r.headers.getValue ("Retry-After", {}));
        if (ra.isNotEmpty() && ra.getIntValue() > 0)
            return juce::jlimit (500, 60000, ra.getIntValue() * 1000);
        return (int) (1000.0 * std::pow (2.0, attempt)) + juce::Random::getSystemRandom().nextInt (400);
    }

    /** POST with retries (network errors, 408/409/429/5xx), honouring retry-after and cancellation. */
    HttpResponse postWithRetries (const juce::String& url, const juce::StringPairArray& headers, const juce::String& body,
                                  const AISettings& s, const std::atomic<bool>& cancel, int& attempts)
    {
        HttpResponse r;
        for (int attempt = 0; attempt <= s.maxRetries; ++attempt)
        {
            attempts = attempt + 1;
            r = httpPostJson (url, headers, body, s.timeoutSeconds * 1000, &cancel);
            if (r.cancelled || r.status == 200 || ! isRetryable (r) || attempt == s.maxRetries) break;
            if (! waitOrCancel (retryDelayMs (r, attempt), cancel)) { r.cancelled = true; r.error = "Cancelled."; break; }
        }
        return r;
    }

    juce::var parseStructured (const juce::String& text)
    {
        juce::var parsed;
        if (juce::JSON::parse (text, parsed).failed() || ! parsed.isObject())
            parsed = extractJsonObject (text);
        return parsed.isObject() ? parsed : juce::var();
    }

    juce::DynamicObject* message (const juce::String& role, const juce::String& text)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("role", role);
        m->setProperty ("content", text);
        return m;
    }

    juce::String userTurn (const AIRequest& req)
    {
        return (req.context.isNotEmpty() ? req.context + "\n\n" : juce::String()) + "Request: " + req.userMessage;
    }

    juce::String schemaInstruction (const AIRequest& req)
    {
        return "\n\nRespond ONLY with one JSON object (no prose, no code fences) matching this JSON schema:\n"
               + juce::JSON::toString (req.responseSchema, true);
    }
}

AIResult AIProvider::diagnose (const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    AIResult r;
    const auto list = listModels (apiKey, cancel);
    r.httpStatus = list.httpStatus;
    r.latencyMs = list.latencyMs;
    if (! list.ok) { r.error = list.error; return r; }
    const auto selected = selectedModel();
    bool found = selected.isEmpty();
    for (const auto& m : list.models) found = found || m.id == selected;
    r.raw = juce::String ((int) list.models.size()) + " model(s) available";
    if (selected.isEmpty())
        r.raw << (list.models.empty() ? juce::String() : "; no model selected, the first (" + list.models.front().id + ") will be used");
    else
        r.raw << "; selected model \"" + selected + "\" " + (found ? "is available" : "was NOT found");
    r.modelUsed = selected;
    r.ok = found && ! list.models.empty();
    if (list.models.empty()) r.error = "The service answered but offers no models (install or load a model first).";
    else if (! found) r.error = r.raw + ". Choose another model (Refresh models).";
    return r;
}

// =================================================================================
// Anthropic
// =================================================================================
juce::StringPairArray AnthropicProvider::headers (const juce::String& apiKey) const
{
    juce::StringPairArray h;
    h.set ("Content-Type", "application/json");
    h.set ("x-api-key", apiKey);
    h.set ("anthropic-version", kApiVersion);
    if (settings.serverSideFallback)
        h.set ("anthropic-beta", "server-side-fallback-2026-07-01");
    return h;
}

juce::String AnthropicProvider::buildBody (const AIRequest& req) const
{
    auto* body = new juce::DynamicObject();
    body->setProperty ("model", settings.anthropicModel);
    body->setProperty ("max_tokens", 16000);

    // Large, stable system prompt (parameter registry) cached across requests.
    auto* sys = new juce::DynamicObject();
    sys->setProperty ("type", "text");
    sys->setProperty ("text", req.systemPrompt);
    auto* cc = new juce::DynamicObject();
    cc->setProperty ("type", "ephemeral");
    sys->setProperty ("cache_control", juce::var (cc));
    body->setProperty ("system", juce::Array<juce::var> { juce::var (sys) });

    // Conversation (roles must alternate and start with "user").
    juce::Array<juce::var> messages;
    juce::String pendingRole, pendingText;
    auto flush = [&]
    {
        if (pendingRole.isEmpty()) return;
        messages.add (juce::var (message (pendingRole, pendingText)));
        pendingRole.clear(); pendingText.clear();
    };
    auto push = [&] (const juce::String& role, const juce::String& text)
    {
        if (messages.isEmpty() && pendingRole.isEmpty() && role != "user") return;
        if (role == pendingRole) { pendingText << "\n\n" << text; return; }
        flush();
        pendingRole = role; pendingText = text;
    };
    for (const auto& h : req.history) push (h.role, h.text);
    push ("user", userTurn (req));
    flush();
    body->setProperty ("messages", messages);

    auto* oc = new juce::DynamicObject();
    oc->setProperty ("effort", settings.effort);
    if (! req.responseSchema.isVoid())
    {
        auto* fmt = new juce::DynamicObject();
        fmt->setProperty ("type", "json_schema");
        fmt->setProperty ("schema", req.responseSchema);
        oc->setProperty ("format", juce::var (fmt));
    }
    body->setProperty ("output_config", juce::var (oc));
    if (settings.serverSideFallback)
        body->setProperty ("fallbacks", "default");
    return juce::JSON::toString (juce::var (body), true);
}

AIResult AnthropicProvider::send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    if (containsHebrew (request.userMessage))
    {
        AIResult rejected;
        rejected.error = "AI sound design supports English only. Please enter your request in English.";
        return rejected;
    }
    AIResult result;
    if (apiKey.isEmpty()) { result.error = "No Anthropic API key configured (Settings > AI)."; return result; }
    const auto url = settings.anthropicBaseUrl.trimCharactersAtEnd ("/") + "/v1/messages";
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const auto r = postWithRetries (url, headers (apiKey), buildBody (request), settings, cancel, result.attempts);
    result.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    result.httpStatus = r.status;
    result.raw = r.body;
    if (r.cancelled) { result.cancelled = true; result.error = "Cancelled."; return result; }
    if (r.status != 200) { result.error = friendlyError (r, "Anthropic"); return result; }

    juce::var v;
    if (juce::JSON::parse (r.body, v).failed() || ! v.isObject()) { result.error = "The response was not valid JSON."; return result; }
    result.modelUsed = v["model"].toString();
    result.inputTokens = (int) v["usage"]["input_tokens"];
    result.outputTokens = (int) v["usage"]["output_tokens"];
    const auto stop = v["stop_reason"].toString();
    if (stop == "refusal") { result.refused = true; result.error = "The model declined this request."; return result; }
    if (stop == "max_tokens") { result.error = "The response was cut off (max_tokens reached)."; return result; }

    juce::String text;
    if (auto* content = v["content"].getArray())
        for (const auto& block : *content)
            if (block["type"].toString() == "text")
                text << block["text"].toString();
    result.raw = text;
    result.json = parseStructured (text);
    if (! result.json.isObject()) { result.error = "The model's reply did not contain the expected JSON object."; return result; }
    result.ok = true;
    return result;
}

ModelListResult AnthropicProvider::listModels (const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    ModelListResult res;
    if (apiKey.isEmpty()) { res.error = "No Anthropic API key stored."; return res; }
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    juce::String after;
    for (int page = 0; page < 10; ++page)
    {
        auto url = settings.anthropicBaseUrl.trimCharactersAtEnd ("/") + "/v1/models?limit=100";
        if (after.isNotEmpty()) url << "&after_id=" << juce::URL::addEscapeChars (after, true);
        const auto r = httpGet (url, headers (apiKey), 20000, &cancel);
        res.httpStatus = r.status;
        if (r.cancelled) { res.error = "Cancelled."; return res; }
        if (r.status != 200) { res.error = friendlyError (r, "Anthropic"); return res; }
        juce::var v;
        if (juce::JSON::parse (r.body, v).failed()) { res.error = "Model list was not valid JSON."; return res; }
        if (auto* data = v["data"].getArray())
            for (const auto& m : *data)
            {
                ModelInfo info;
                info.id = m["id"].toString();
                info.displayName = m["display_name"].toString();
                const auto so = m["capabilities"]["structured_outputs"]["supported"];
                info.structuredOutputs = so.isBool() ? ((bool) so ? 1 : 0) : -1;
                if (info.id.isNotEmpty()) res.models.push_back (info);
            }
        if (! (bool) v["has_more"]) break;
        after = v["last_id"].toString();
        if (after.isEmpty()) break;
    }
    res.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    res.ok = true;
    return res;
}

// =================================================================================
// OpenAI Chat Completions (OpenAI, LM Studio, other compatible servers)
// =================================================================================
juce::String ChatCompletionsProvider::name() const
{
    const auto m = settings.model();
    return (settings.provider == AISettings::OpenAI ? juce::String ("OpenAI")
            : settings.provider == AISettings::LMStudio ? juce::String ("LM Studio") : juce::String ("OpenAI-compatible"))
           + " (" + (m.isNotEmpty() ? m : juce::String ("auto")) + ")";
}

juce::String ChatCompletionsProvider::buildBody (const AIRequest& req, const juce::String& model, Format format) const
{
    auto* body = new juce::DynamicObject();
    body->setProperty ("model", model);
    juce::Array<juce::var> messages;
    messages.add (juce::var (message ("system", req.systemPrompt + (format == Format::JsonSchema ? juce::String() : schemaInstruction (req)))));
    for (const auto& h : req.history) messages.add (juce::var (message (h.role, h.text)));
    messages.add (juce::var (message ("user", userTurn (req))));
    body->setProperty ("messages", messages);
    if (format == Format::JsonSchema)
    {
        auto* js = new juce::DynamicObject();
        js->setProperty ("name", "vsx_patch_response");
        js->setProperty ("schema", req.responseSchema);
        js->setProperty ("strict", false); // the schema uses optional fields, which strict mode does not allow
        auto* rf = new juce::DynamicObject();
        rf->setProperty ("type", "json_schema");
        rf->setProperty ("json_schema", juce::var (js));
        body->setProperty ("response_format", juce::var (rf));
    }
    else if (format == Format::JsonObject)
    {
        auto* rf = new juce::DynamicObject();
        rf->setProperty ("type", "json_object");
        body->setProperty ("response_format", juce::var (rf));
    }
    body->setProperty ("stream", false);
    return juce::JSON::toString (juce::var (body), true);
}

ModelListResult ChatCompletionsProvider::listModels (const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    ModelListResult res;
    juce::StringPairArray h;
    if (apiKey.isNotEmpty()) h.set ("Authorization", "Bearer " + apiKey);
    else if (settings.provider == AISettings::OpenAI) { res.error = "No OpenAI API key stored."; return res; }
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const auto r = httpGet (settings.baseUrl().trimCharactersAtEnd ("/") + "/models", h, 20000, &cancel);
    res.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    res.httpStatus = r.status;
    if (r.cancelled) { res.error = "Cancelled."; return res; }
    if (r.status != 200) { res.error = friendlyError (r, AISettings::providerName (settings.provider)); return res; }
    juce::var v;
    if (juce::JSON::parse (r.body, v).failed()) { res.error = "Model list was not valid JSON."; return res; }
    if (auto* data = v["data"].getArray())
        for (const auto& m : *data)
            if (m["id"].toString().isNotEmpty())
                res.models.push_back ({ m["id"].toString(), m["id"].toString(), -1 });
    std::sort (res.models.begin(), res.models.end(), [] (const ModelInfo& a, const ModelInfo& b) { return a.id < b.id; });
    res.ok = true;
    return res;
}

AIResult ChatCompletionsProvider::send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    if (containsHebrew (request.userMessage))
    {
        AIResult rejected;
        rejected.error = "AI sound design supports English only. Please enter your request in English.";
        return rejected;
    }
    AIResult result;
    if (settings.provider == AISettings::OpenAI && apiKey.isEmpty()) { result.error = "No OpenAI API key configured (Settings > AI)."; return result; }
    auto model = settings.model();
    if (model.isEmpty())
    {
        if (settings.provider == AISettings::OpenAI) { result.error = "Choose an OpenAI model first (Settings > AI > Refresh models)."; return result; }
        const auto list = listModels (apiKey, cancel);
        if (! list.ok || list.models.empty())
        {
            result.error = list.ok ? "The server has no model loaded." : list.error;
            result.cancelled = cancel.load();
            return result;
        }
        model = list.models.front().id;
    }
    juce::StringPairArray h;
    h.set ("Content-Type", "application/json");
    if (apiKey.isNotEmpty()) h.set ("Authorization", "Bearer " + apiKey);
    const auto url = settings.baseUrl().trimCharactersAtEnd ("/") + "/chat/completions";
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    HttpResponse r;
    // Structured outputs first; servers without json_schema support fall back to JSON mode, then plain text.
    for (auto format : { Format::JsonSchema, Format::JsonObject, Format::None })
    {
        int attempts = 0;
        r = postWithRetries (url, h, buildBody (request, model, format), settings, cancel, attempts);
        result.attempts += attempts;
        const bool formatRejected = (r.status == 400 || r.status == 422)
                                 && (r.body.containsIgnoreCase ("response_format") || r.body.containsIgnoreCase ("json_schema"));
        if (! formatRejected) break;
    }
    result.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    result.httpStatus = r.status;
    result.raw = r.body;
    if (r.cancelled) { result.cancelled = true; result.error = "Cancelled."; return result; }
    if (r.status != 200) { result.error = friendlyError (r, AISettings::providerName (settings.provider)); return result; }
    juce::var v;
    if (juce::JSON::parse (r.body, v).failed() || ! v.isObject()) { result.error = "The response was not valid JSON."; return result; }
    result.modelUsed = v["model"].toString().isNotEmpty() ? v["model"].toString() : model;
    result.inputTokens = (int) v["usage"]["prompt_tokens"];
    result.outputTokens = (int) v["usage"]["completion_tokens"];
    const auto& msg = v["choices"][0]["message"];
    if (msg["refusal"].isString() && msg["refusal"].toString().isNotEmpty())
    {
        result.refused = true;
        result.error = "The model declined this request: " + msg["refusal"].toString().substring (0, 300);
        return result;
    }
    if (v["choices"][0]["finish_reason"].toString() == "length") { result.error = "The response was cut off (length limit)."; return result; }
    const auto content = msg["content"].toString();
    result.raw = content;
    result.json = parseStructured (content);
    if (! result.json.isObject()) { result.error = "The model's reply did not contain a JSON object."; return result; }
    result.ok = true;
    return result;
}

// =================================================================================
// Ollama native API
// =================================================================================
juce::String OllamaProvider::buildBody (const AIRequest& req, const juce::String& model) const
{
    auto* body = new juce::DynamicObject();
    body->setProperty ("model", model);
    juce::Array<juce::var> messages;
    // Ollama recommends also grounding the schema in the prompt.
    messages.add (juce::var (message ("system", req.systemPrompt + schemaInstruction (req))));
    for (const auto& h : req.history) messages.add (juce::var (message (h.role, h.text)));
    messages.add (juce::var (message ("user", userTurn (req))));
    body->setProperty ("messages", messages);
    body->setProperty ("stream", false);
    body->setProperty ("format", req.responseSchema);
    auto* options = new juce::DynamicObject();
    options->setProperty ("num_ctx", 32768); // the parameter reference does not fit the small default context
    body->setProperty ("options", juce::var (options));
    return juce::JSON::toString (juce::var (body), true);
}

ModelListResult OllamaProvider::listModels (const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    ModelListResult res;
    juce::StringPairArray h;
    if (apiKey.isNotEmpty()) h.set ("Authorization", "Bearer " + apiKey);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const auto r = httpGet (settings.ollamaBaseUrl.trimCharactersAtEnd ("/") + "/api/tags", h, 10000, &cancel);
    res.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    res.httpStatus = r.status;
    if (r.cancelled) { res.error = "Cancelled."; return res; }
    if (r.status != 200)
    {
        res.error = r.status == 0 ? "Ollama is not reachable at " + settings.ollamaBaseUrl + " (is it running? `ollama serve`)."
                                  : friendlyError (r, "Ollama");
        return res;
    }
    juce::var v;
    if (juce::JSON::parse (r.body, v).failed()) { res.error = "Model list was not valid JSON."; return res; }
    if (auto* models = v["models"].getArray())
        for (const auto& m : *models)
        {
            const auto id = m["name"].toString().isNotEmpty() ? m["name"].toString() : m["model"].toString();
            if (id.isNotEmpty()) res.models.push_back ({ id, id, -1 });
        }
    res.ok = true;
    return res;
}

AIResult OllamaProvider::send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    if (containsHebrew (request.userMessage))
    {
        AIResult rejected;
        rejected.error = "AI sound design supports English only. Please enter your request in English.";
        return rejected;
    }
    AIResult result;
    auto model = settings.ollamaModel;
    if (model.isEmpty())
    {
        const auto list = listModels (apiKey, cancel);
        if (! list.ok || list.models.empty())
        {
            result.error = list.ok ? "No models are installed in Ollama (run: ollama pull <model>)." : list.error;
            result.cancelled = cancel.load();
            return result;
        }
        model = list.models.front().id;
    }
    juce::StringPairArray h;
    h.set ("Content-Type", "application/json");
    if (apiKey.isNotEmpty()) h.set ("Authorization", "Bearer " + apiKey);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const auto r = postWithRetries (settings.ollamaBaseUrl.trimCharactersAtEnd ("/") + "/api/chat", h, buildBody (request, model), settings, cancel, result.attempts);
    result.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    result.httpStatus = r.status;
    result.raw = r.body;
    if (r.cancelled) { result.cancelled = true; result.error = "Cancelled."; return result; }
    if (r.status != 200)
    {
        result.error = r.status == 404 ? "Ollama model \"" + model + "\" not found (run: ollama pull " + model + ")." : friendlyError (r, "Ollama");
        return result;
    }
    juce::var v;
    if (juce::JSON::parse (r.body, v).failed() || ! v.isObject()) { result.error = "The response was not valid JSON."; return result; }
    result.modelUsed = v["model"].toString().isNotEmpty() ? v["model"].toString() : model;
    result.inputTokens = (int) v["prompt_eval_count"];
    result.outputTokens = (int) v["eval_count"];
    if (v["done_reason"].toString() == "length") { result.error = "The response was cut off (context or length limit)."; return result; }
    const auto content = v["message"]["content"].toString();
    result.raw = content;
    result.json = parseStructured (content);
    if (! result.json.isObject()) { result.error = "The model's reply did not contain a JSON object."; return result; }
    result.ok = true;
    return result;
}

// =================================================================================
std::unique_ptr<AIProvider> createProvider (const AISettings& s)
{
    switch (s.provider)
    {
        case AISettings::Anthropic:        return std::make_unique<AnthropicProvider> (s);
        case AISettings::Ollama:           return std::make_unique<OllamaProvider> (s);
        case AISettings::OpenAI:
        case AISettings::LMStudio:
        case AISettings::OpenAICompatible: return std::make_unique<ChatCompletionsProvider> (s);
        default:                           return nullptr;
    }
}

} // namespace vsx
