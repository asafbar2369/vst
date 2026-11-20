#include "AIProvider.h"

namespace vsx
{
// ---------------------------------------------------------------------------------
juce::var AISettings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("provider", provider);
    o->setProperty ("anthropicBaseUrl", anthropicBaseUrl);
    o->setProperty ("anthropicModel", anthropicModel);
    o->setProperty ("effort", effort);
    o->setProperty ("serverSideFallback", serverSideFallback);
    o->setProperty ("openAiBaseUrl", openAiBaseUrl);
    o->setProperty ("openAiModel", openAiModel);
    o->setProperty ("timeoutSeconds", timeoutSeconds);
    o->setProperty ("maxRetries", maxRetries);
    o->setProperty ("fallbackToOffline", fallbackToOffline);
    o->setProperty ("autoPreview", autoPreview);
    o->setProperty ("optimiseAfterGenerate", optimiseAfterGenerate);
    return juce::var (o);
}

AISettings AISettings::fromVar (const juce::var& v)
{
    AISettings s;
    if (! v.isObject()) return s;
    s.provider = juce::jlimit (0, 2, (int) v.getProperty ("provider", s.provider));
    s.anthropicBaseUrl = v.getProperty ("anthropicBaseUrl", s.anthropicBaseUrl).toString();
    s.anthropicModel = v.getProperty ("anthropicModel", s.anthropicModel).toString();
    s.effort = v.getProperty ("effort", s.effort).toString();
    s.serverSideFallback = (bool) v.getProperty ("serverSideFallback", s.serverSideFallback);
    s.openAiBaseUrl = v.getProperty ("openAiBaseUrl", s.openAiBaseUrl).toString();
    s.openAiModel = v.getProperty ("openAiModel", s.openAiModel).toString();
    s.timeoutSeconds = juce::jlimit (5, 900, (int) v.getProperty ("timeoutSeconds", s.timeoutSeconds));
    s.maxRetries = juce::jlimit (0, 5, (int) v.getProperty ("maxRetries", s.maxRetries));
    s.fallbackToOffline = (bool) v.getProperty ("fallbackToOffline", s.fallbackToOffline);
    s.autoPreview = (bool) v.getProperty ("autoPreview", s.autoPreview);
    s.optimiseAfterGenerate = (bool) v.getProperty ("optimiseAfterGenerate", s.optimiseAfterGenerate);
    return s;
}

juce::File AISettings::settingsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("VICE SYNTH X");
    dir.createDirectory();
    return dir.getChildFile ("ai_settings.json");
}

void AISettings::save() const
{
    settingsFile().replaceWithText (juce::JSON::toString (toVar()));
}

AISettings AISettings::load()
{
    return fromVar (juce::JSON::parse (settingsFile().loadFileAsString()));
}

// ---------------------------------------------------------------------------------
static juce::String headerString (const juce::StringPairArray& headers)
{
    juce::String h;
    for (int i = 0; i < headers.size(); ++i)
        h << headers.getAllKeys()[i] << ": " << headers.getAllValues()[i] << "\r\n";
    return h;
}

HttpResponse httpPostJson (const juce::String& url, const juce::StringPairArray& headers, const juce::String& body, int timeoutMs)
{
    HttpResponse r;
    juce::URL u (url);
    u = u.withPOSTData (body);
    int status = 0;
    juce::StringPairArray responseHeaders;
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders (headerString (headers))
                    .withConnectionTimeoutMs (timeoutMs)
                    .withStatusCode (&status)
                    .withResponseHeaders (&responseHeaders)
                    .withHttpRequestCmd ("POST")
                    .withNumRedirectsToFollow (2);
    auto stream = u.createInputStream (opts);
    r.status = status;
    r.headers = responseHeaders;
    if (stream == nullptr)
    {
        r.error = status > 0 ? "HTTP " + juce::String (status) : "Could not connect to " + u.getDomain() + " (network unavailable, wrong address or timeout).";
        return r;
    }
    r.body = stream->readEntireStreamAsString();
    return r;
}

HttpResponse httpGet (const juce::String& url, const juce::StringPairArray& headers, int timeoutMs)
{
    HttpResponse r;
    int status = 0;
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders (headerString (headers))
                    .withConnectionTimeoutMs (timeoutMs)
                    .withStatusCode (&status);
    auto stream = juce::URL (url).createInputStream (opts);
    r.status = status;
    if (stream == nullptr)
    {
        r.error = "Could not connect to " + juce::URL (url).getDomain();
        return r;
    }
    r.body = stream->readEntireStreamAsString();
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
                if (juce::JSON::parse (juce::String (s.substr (start, i - start + 1)), v).wasOk() && v.isObject())
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
        if (r.status == 0) return true;                     // network error / timeout
        if (r.status == 429)
            return ! r.body.contains ("enforced_spend_limit_reached");
        return r.status == 408 || r.status == 409 || r.status >= 500; // includes 529 overloaded
    }

    juce::String apiErrorMessage (const HttpResponse& r)
    {
        juce::var v;
        if (juce::JSON::parse (r.body, v).wasOk())
        {
            if (v["error"].isObject())
                return v["error"]["type"].toString() + ": " + v["error"]["message"].toString();
            if (v["error"].isString()) return v["error"].toString();
        }
        if (r.error.isNotEmpty()) return r.error;
        return "HTTP " + juce::String (r.status) + (r.body.isNotEmpty() ? ": " + r.body.substring (0, 300) : juce::String());
    }

    int retryDelayMs (const HttpResponse& r, int attempt)
    {
        const auto ra = r.headers.getValue ("retry-after", r.headers.getValue ("Retry-After", {}));
        if (ra.isNotEmpty() && ra.getIntValue() > 0)
            return juce::jlimit (500, 60000, ra.getIntValue() * 1000);
        return (int) (1000.0 * std::pow (2.0, attempt)) + juce::Random::getSystemRandom().nextInt (400);
    }
}

// ---------------------------------------------------------------------------------
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
        auto* m = new juce::DynamicObject();
        m->setProperty ("role", pendingRole);
        m->setProperty ("content", pendingText);
        messages.add (juce::var (m));
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
    push ("user", (req.context.isNotEmpty() ? req.context + "\n\n" : juce::String()) + "Request: " + req.userMessage);
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

AIResult AnthropicProvider::post (const juce::String& body, const juce::String& apiKey, const std::atomic<bool>& cancel, bool structured)
{
    AIResult result;
    juce::StringPairArray headers;
    headers.set ("Content-Type", "application/json");
    headers.set ("x-api-key", apiKey);
    headers.set ("anthropic-version", "2026-10-21");
    if (settings.serverSideFallback)
        headers.set ("anthropic-beta", "server-side-fallback-2026-07-01");

    const auto url = settings.anthropicBaseUrl.trimCharactersAtEnd ("/") + "/v1/messages";
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    HttpResponse r;
    for (int attempt = 0; attempt <= settings.maxRetries; ++attempt)
    {
        result.attempts = attempt + 1;
        if (cancel.load()) { result.error = "Cancelled."; return result; }
        r = httpPostJson (url, headers, body, settings.timeoutSeconds * 1000);
        if (r.status == 200 || ! isRetryable (r) || attempt == settings.maxRetries) break;
        if (! waitOrCancel (retryDelayMs (r, attempt), cancel)) { result.error = "Cancelled."; return result; }
    }
    result.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    result.httpStatus = r.status;
    result.raw = r.body;
    if (r.status != 200)
    {
        result.error = apiErrorMessage (r);
        if (r.status == 401) result.error = "Authentication failed (401): check the Anthropic API key in Settings.";
        return result;
    }

    juce::var v;
    if (juce::JSON::parse (r.body, v).failed() || ! v.isObject())
    {
        result.error = "The response was not valid JSON.";
        return result;
    }
    result.modelUsed = v["model"].toString();
    result.inputTokens = (int) v["usage"]["input_tokens"];
    result.outputTokens = (int) v["usage"]["output_tokens"];
    const auto stop = v["stop_reason"].toString();
    if (stop == "refusal")
    {
        result.refused = true;
        result.error = "The model declined this request.";
        return result;
    }
    if (stop == "max_tokens")
    {
        result.error = "The response was cut off (max_tokens reached).";
        return result;
    }

    juce::String text;
    if (auto* content = v["content"].getArray())
        for (const auto& block : *content)
            if (block["type"].toString() == "text")
                text << block["text"].toString();
    result.raw = text;
    if (! structured)
    {
        result.ok = true;
        return result;
    }
    juce::var parsed;
    if (juce::JSON::parse (text, parsed).failed() || ! parsed.isObject())
        parsed = extractJsonObject (text);
    if (! parsed.isObject())
    {
        result.error = "The model's reply did not contain the expected JSON object.";
        return result;
    }
    result.json = parsed;
    result.ok = true;
    return result;
}

AIResult AnthropicProvider::send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    if (apiKey.isEmpty())
    {
        AIResult r;
        r.error = "No Anthropic API key configured (Settings > AI).";
        return r;
    }
    return post (buildBody (request), apiKey, cancel, true);
}

AIResult AnthropicProvider::diagnose (const juce::String& apiKey)
{
    if (apiKey.isEmpty())
    {
        AIResult r;
        r.error = "No API key stored.";
        return r;
    }
    auto* body = new juce::DynamicObject();
    body->setProperty ("model", settings.anthropicModel);
    body->setProperty ("max_tokens", 64);
    auto* m = new juce::DynamicObject();
    m->setProperty ("role", "user");
    m->setProperty ("content", "Reply with the single word: ready");
    body->setProperty ("messages", juce::Array<juce::var> { juce::var (m) });
    auto* oc = new juce::DynamicObject();
    oc->setProperty ("effort", "low");
    body->setProperty ("output_config", juce::var (oc));
    std::atomic<bool> noCancel { false };
    return post (juce::JSON::toString (juce::var (body), true), apiKey, noCancel, false);
}

// ---------------------------------------------------------------------------------
juce::String OpenAICompatibleProvider::buildBody (const AIRequest& req) const
{
    auto* body = new juce::DynamicObject();
    body->setProperty ("model", settings.openAiModel);
    juce::Array<juce::var> messages;
    auto add = [&] (const juce::String& role, const juce::String& text)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("role", role);
        m->setProperty ("content", text);
        messages.add (juce::var (m));
    };
    add ("system", req.systemPrompt + "\n\nRespond ONLY with one JSON object matching this JSON schema:\n"
                       + juce::JSON::toString (req.responseSchema, true));
    for (const auto& h : req.history) add (h.role, h.text);
    add ("user", (req.context.isNotEmpty() ? req.context + "\n\n" : juce::String()) + "Request: " + req.userMessage);
    body->setProperty ("messages", messages);
    auto* rf = new juce::DynamicObject();
    rf->setProperty ("type", "json_object");
    body->setProperty ("response_format", juce::var (rf));
    body->setProperty ("stream", false);
    return juce::JSON::toString (juce::var (body), true);
}

AIResult OpenAICompatibleProvider::send (const AIRequest& request, const juce::String& apiKey, const std::atomic<bool>& cancel)
{
    AIResult result;
    juce::StringPairArray headers;
    headers.set ("Content-Type", "application/json");
    if (apiKey.isNotEmpty()) headers.set ("Authorization", "Bearer " + apiKey);
    const auto url = settings.openAiBaseUrl.trimCharactersAtEnd ("/") + "/chat/completions";
    auto body = buildBody (request);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    HttpResponse r;
    bool removedFormat = false;
    for (int attempt = 0; attempt <= settings.maxRetries; ++attempt)
    {
        result.attempts = attempt + 1;
        if (cancel.load()) { result.error = "Cancelled."; return result; }
        r = httpPostJson (url, headers, body, settings.timeoutSeconds * 1000);
        if (r.status == 400 && ! removedFormat && r.body.containsIgnoreCase ("response_format"))
        {
            // some servers do not support JSON mode: retry once without it
            juce::var v = juce::JSON::parse (body);
            if (auto* o = v.getDynamicObject()) o->removeProperty ("response_format");
            body = juce::JSON::toString (v, true);
            removedFormat = true;
            --attempt;
            continue;
        }
        if (r.status == 200 || ! isRetryable (r) || attempt == settings.maxRetries) break;
        if (! waitOrCancel (retryDelayMs (r, attempt), cancel)) { result.error = "Cancelled."; return result; }
    }
    result.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    result.httpStatus = r.status;
    result.raw = r.body;
    if (r.status != 200)
    {
        result.error = apiErrorMessage (r);
        return result;
    }
    juce::var v;
    if (juce::JSON::parse (r.body, v).failed())
    {
        result.error = "The response was not valid JSON.";
        return result;
    }
    result.modelUsed = v["model"].toString();
    const auto content = v["choices"][0]["message"]["content"].toString();
    result.raw = content;
    result.json = extractJsonObject (content);
    if (! result.json.isObject())
    {
        result.error = "The model's reply did not contain a JSON object.";
        return result;
    }
    result.ok = true;
    return result;
}

AIResult OpenAICompatibleProvider::diagnose (const juce::String& apiKey)
{
    AIResult result;
    juce::StringPairArray headers;
    if (apiKey.isNotEmpty()) headers.set ("Authorization", "Bearer " + apiKey);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const auto r = httpGet (settings.openAiBaseUrl.trimCharactersAtEnd ("/") + "/models", headers, 15000);
    result.latencyMs = juce::Time::getMillisecondCounterHiRes() - t0;
    result.httpStatus = r.status;
    result.raw = r.body;
    result.ok = r.status == 200;
    if (! result.ok) result.error = r.error.isNotEmpty() ? r.error : "HTTP " + juce::String (r.status);
    return result;
}

std::unique_ptr<AIProvider> createProvider (const AISettings& s)
{
    if (s.provider == AISettings::Anthropic) return std::make_unique<AnthropicProvider> (s);
    if (s.provider == AISettings::OpenAICompatible) return std::make_unique<OpenAICompatibleProvider> (s);
    return nullptr;
}

} // namespace vsx
