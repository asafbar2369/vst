#include "../Source/AI/AIController.h"
#include "../Source/Presets/PresetManager.h"
#include <juce_core/juce_core.h>
#include <map>
#include <thread>

/*
    AI provider, protocol and workflow tests.

    IMPORTANT: the protocol tests below talk to a LOCAL MOCK HTTP SERVER started by the test
    on 127.0.0.1. They verify request formats, response parsing, error handling, fallbacks,
    timeouts and cancellation. They are NOT live connectivity tests against Anthropic, OpenAI,
    Ollama or LM Studio, and must never be reported as such.
*/
namespace vsx
{
namespace
{
    /** Minimal HTTP/1.1 server answering each request with a handler result (Connection: close). */
    class MockHttpServer : private juce::Thread
    {
    public:
        struct Request { juce::String method, path, body; std::map<juce::String, juce::String> headers; };
        struct Response { int status = 200; juce::String body; int delayMs = 0; };
        using Handler = std::function<Response (const Request&)>;

        explicit MockHttpServer (Handler h) : juce::Thread ("VSX mock HTTP"), handler (std::move (h))
        {
            for (int p = 39100; p < 39200 && port == 0; ++p)
                if (socket.createListener (p, "127.0.0.1")) port = p;
            if (port != 0) startThread();
        }
        ~MockHttpServer() override
        {
            signalThreadShouldExit();
            socket.close();
            stopThread (4000);
        }
        int getPort() const { return port; }
        juce::String url (const juce::String& path = {}) const { return "http://127.0.0.1:" + juce::String (port) + path; }
        std::vector<Request> requests() { const juce::ScopedLock sl (lock); return received; }

    private:
        void run() override
        {
            while (! threadShouldExit())
            {
                std::unique_ptr<juce::StreamingSocket> c (socket.waitForNextConnection());
                if (c == nullptr) continue;
                Request req;
                if (! readRequest (*c, req)) continue;
                { const juce::ScopedLock sl (lock); received.push_back (req); }
                auto res = handler (req);
                for (int waited = 0; waited < res.delayMs && ! threadShouldExit(); waited += 20) juce::Thread::sleep (20);
                const auto bodyUtf8 = res.body.toStdString();
                juce::String head;
                head << "HTTP/1.1 " << res.status << " X\r\nContent-Type: application/json\r\nContent-Length: " << (int) bodyUtf8.size()
                     << "\r\nConnection: close\r\n\r\n";
                const auto all = head.toStdString() + bodyUtf8;
                c->write (all.data(), (int) all.size());
                juce::Thread::sleep (5);
                c->close();
            }
        }

        static bool readRequest (juce::StreamingSocket& c, Request& req)
        {
            std::string data;
            char buf[4096];
            size_t headerEnd = std::string::npos;
            const auto start = juce::Time::getMillisecondCounter();
            while (headerEnd == std::string::npos)
            {
                if (juce::Time::getMillisecondCounter() - start > 5000) return false;
                if (c.waitUntilReady (true, 200) <= 0) continue;
                const int n = c.read (buf, sizeof (buf), false);
                if (n <= 0) return false;
                data.append (buf, (size_t) n);
                headerEnd = data.find ("\r\n\r\n");
            }
            const auto headerText = juce::String (data.substr (0, headerEnd));
            auto lines = juce::StringArray::fromLines (headerText);
            const auto first = juce::StringArray::fromTokens (lines[0], " ", "");
            req.method = first[0];
            req.path = first[1];
            int contentLength = 0;
            for (int i = 1; i < lines.size(); ++i)
            {
                const auto key = lines[i].upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
                const auto value = lines[i].fromFirstOccurrenceOf (":", false, false).trim();
                req.headers[key] = value;
                if (key == "content-length") contentLength = value.getIntValue();
            }
            std::string body = data.substr (headerEnd + 4);
            while ((int) body.size() < contentLength)
            {
                if (juce::Time::getMillisecondCounter() - start > 5000) return false;
                if (c.waitUntilReady (true, 200) <= 0) continue;
                const int n = c.read (buf, sizeof (buf), false);
                if (n <= 0) break;
                body.append (buf, (size_t) n);
            }
            req.body = juce::String::fromUTF8 (body.data(), (int) body.size());
            return true;
        }

        Handler handler;
        juce::StreamingSocket socket;
        int port = 0;
        juce::CriticalSection lock;
        std::vector<Request> received;
    };

    juce::String validDesignJson (const juce::String& reply = "Darker and warmer.")
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("reply", reply);
        o->setProperty ("action", "modify_patch");
        auto* op = new juce::DynamicObject();
        op->setProperty ("op", "set");
        op->setProperty ("param", "flt1_cutoff");
        op->setProperty ("value", 900.0);
        o->setProperty ("operations", juce::Array<juce::var> { juce::var (op) });
        return juce::JSON::toString (juce::var (o), true);
    }

    juce::String jsonString (const juce::String& s) { return juce::JSON::toString (juce::var (s)); }

    /** In-memory host used to exercise the controller without the audio processor. */
    struct FakeHost : PatchHost
    {
        PatchSpec current = PatchOps::initPatch();
        std::vector<PatchSpec> undoStack, redoStack;
        PresetManager presets;
        PatchSpec getCurrentPatch() override { return current; }
        void applyPatch (const PatchSpec& s, const juce::String&) override { undoStack.push_back (current); redoStack.clear(); current = s; }
        void previewPatch (const PatchSpec& s) override { current = s; }
        bool undo() override { if (undoStack.empty()) return false; redoStack.push_back (current); current = undoStack.back(); undoStack.pop_back(); return true; }
        bool redo() override { if (redoStack.empty()) return false; undoStack.push_back (current); current = redoStack.back(); redoStack.pop_back(); return true; }
        bool canUndo() const override { return ! undoStack.empty(); }
        bool canRedo() const override { return ! redoStack.empty(); }
        PresetManager& getPresetManager() override { return presets; }
    };

    bool waitIdle (AIController& ai, int timeoutMs)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (juce::Time::getMillisecondCounter() < end)
        {
            ai.deliverPendingResults();
            if (! ai.isBusy()) return true;
            juce::Thread::sleep (10);
        }
        return false;
    }
}

class AIProviderTests final : public juce::UnitTest
{
public:
    AIProviderTests() : juce::UnitTest ("AI providers and workflow", "VICE SYNTH X") {}

    void runTest() override
    {
        // never touch the real user's AI settings
        const auto tempSettings = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vsx-test-ai-settings");
        tempSettings.deleteRecursively();
        tempSettings.createDirectory();
        AISettings::setSettingsFolderOverride (tempSettings);

        testEnglishOnly();
        testUrlPolicy();
        testRequestBodies();
        testAnthropicProtocol();
        testChatCompletionsProtocol();
        testOllamaProtocol();
        testCancellation();
        testControllerWorkflow();
        testLocalAgent();

        AISettings::setSettingsFolderOverride ({});
        tempSettings.deleteRecursively();
    }

    void testEnglishOnly()
    {
        beginTest ("Hebrew AI is disabled without modifying the patch or sending a request");
        const auto hebrew = juce::String::charToString ((juce::juce_wchar) 0x05D0);
        const auto initial = PatchOps::initPatch();
        OfflineDesigner designer;
        const auto rejected = designer.design (hebrew, initial);
        expect (! rejected.hasPatch);
        expect (rejected.question.contains ("English only"));
        expect (! containsHebrew (rejected.reply));
        const auto english = designer.design ("Create a warm analog pad", initial);
        expect (english.hasPatch, english.reply);
        expect (! containsHebrew (english.reply));
        expect (containsHebrew (juce::String::charToString ((juce::juce_wchar) 0xFB2A)));

        FakeHost host;
        AIController controller (host);
        const auto before = host.getCurrentPatch().toJson();
        controller.submit (hebrew);
        expect (! controller.isBusy());
        expectEquals (host.getCurrentPatch().toJson(), before);
        expect (controller.getConversation().back().text.contains ("English only"));

        std::atomic<int> calls { 0 };
        MockHttpServer server ([&calls] (const MockHttpServer::Request&) -> MockHttpServer::Response
        {
            ++calls; return { 500, "{}" };
        });
        AIRequest request; request.userMessage = hebrew;
        std::atomic<bool> cancel { false };
        AISettings settings;
        settings.anthropicBaseUrl = server.url();
        settings.openAiBaseUrl = server.url();
        settings.ollamaBaseUrl = server.url();
        const auto check = [this] (const AIResult& result)
        {
            expect (! result.ok && result.error.contains ("English only"));
        };
        check (AnthropicProvider (settings).send (request, {}, cancel));
        check (ChatCompletionsProvider (settings).send (request, {}, cancel));
        check (OllamaProvider (settings).send (request, {}, cancel));
        expectEquals (calls.load(), 0);

        const auto response = juce::JSON::parse (validDesignJson (hebrew));
        const auto result = PatchOps::fromResponse (response, initial, "Make this warmer");
        expect (! result.hasPatch && ! result.report.errors.isEmpty());
        expectEquals (result.patch.toJson(), initial.toJson());
        expect (PatchOps::systemPrompt().contains ("English only"));
    }

    void testUrlPolicy()
    {
        beginTest ("Remote providers require HTTPS; plain HTTP only for local hosts");
        juce::String err;
        expect (isUrlAllowed ("https://api.anthropic.com", err));
        expect (isUrlAllowed ("http://localhost:11434", err));
        expect (isUrlAllowed ("http://127.0.0.1:1234/v1", err));
        expect (isUrlAllowed ("http://192.168.1.20:1234/v1", err));
        expect (! isUrlAllowed ("http://example.com/v1", err) && err.contains ("HTTPS"));
        expect (! isUrlAllowed ("ftp://example.com", err));
        expect (! isUrlAllowed ("", err));
        AISettings s;
        s.provider = AISettings::Ollama;
        expect (s.isLocal() && s.modeLabel() == "LOCAL");
        s.provider = AISettings::Anthropic;
        expect (s.isCloud() && s.modeLabel() == "CLOUD" && s.requiresApiKey());
        s.provider = AISettings::Offline;
        expect (s.modeLabel() == "OFFLINE");
        const auto round = AISettings::fromVar (s.toVar());
        expectEquals (round.provider, (int) AISettings::Offline);
    }

    void testRequestBodies()
    {
        beginTest ("Request bodies follow each provider's documented format");
        AIRequest req;
        req.systemPrompt = "SYSTEM";
        req.userMessage = "Create a warm pad";
        req.responseSchema = PatchOps::responseSchema();
        AISettings s;
        s.provider = AISettings::Anthropic;
        const auto a = juce::JSON::parse (AnthropicProvider (s).buildBody (req));
        expectEquals (a["model"].toString(), s.anthropicModel);
        expectEquals (a["output_config"]["format"]["type"].toString(), juce::String ("json_schema"));
        expect (! a.hasProperty ("temperature") && ! a.hasProperty ("top_p") && ! a.hasProperty ("top_k"), "sampling params are rejected by API 2026-10-21");
        expect (a["messages"][0]["content"].toString().contains (req.userMessage));

        s.provider = AISettings::LMStudio;
        const auto c = juce::JSON::parse (ChatCompletionsProvider (s).buildBody (req, "m", ChatCompletionsProvider::Format::JsonSchema));
        expectEquals (c["response_format"]["type"].toString(), juce::String ("json_schema"));
        expectEquals (c["response_format"]["json_schema"]["name"].toString(), juce::String ("vsx_patch_response"));
        expect (c["response_format"]["json_schema"]["schema"].isObject());
        expect (! (bool) c["stream"]);
        const auto c2 = juce::JSON::parse (ChatCompletionsProvider (s).buildBody (req, "m", ChatCompletionsProvider::Format::None));
        expect (! c2.hasProperty ("response_format") && c2["messages"][0]["content"].toString().contains ("JSON schema"));

        s.provider = AISettings::Ollama;
        const auto o = juce::JSON::parse (OllamaProvider (s).buildBody (req, "llama"));
        expect (o["format"].isObject() && ! (bool) o["stream"] && (int) o["options"]["num_ctx"] >= 16384);
    }

    void testAnthropicProtocol()
    {
        beginTest ("Anthropic protocol (local mock server): success, models, auth error, refusal");
        MockHttpServer server ([] (const MockHttpServer::Request& r) -> MockHttpServer::Response
        {
            if (r.headers.count ("x-api-key") == 0 || r.headers.at ("x-api-key") != "good-key")
                return { 401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})" };
            if (r.path.startsWith ("/v1/models"))
                return { 200, R"({"data":[{"id":"claude-opus-5-5","display_name":"Claude Opus 5.5","capabilities":{"structured_outputs":{"supported":true}}}],"has_more":false})" };
            if (r.body.contains ("please refuse"))
                return { 200, R"({"model":"claude-opus-5-5","stop_reason":"refusal","content":[],"usage":{"input_tokens":5,"output_tokens":0}})" };
            return { 200, juce::String (R"({"model":"claude-opus-5-5","stop_reason":"end_turn","usage":{"input_tokens":10,"output_tokens":20},"content":[{"type":"text","text":)")
                            + jsonString (validDesignJson()) + "}]}" };
        });
        expect (server.getPort() != 0, "mock server could not start");
        AISettings s;
        s.provider = AISettings::Anthropic;
        s.anthropicBaseUrl = server.url();
        s.maxRetries = 0;
        AnthropicProvider p (s);
        std::atomic<bool> cancel { false };
        AIRequest req;
        req.systemPrompt = "S";
        req.userMessage = "darker";
        req.responseSchema = PatchOps::responseSchema();
        auto r = p.send (req, "good-key", cancel);
        expect (r.ok, r.error);
        expectEquals (r.json["action"].toString(), juce::String ("modify_patch"));
        expectEquals (r.outputTokens, 20);
        const auto reqs = server.requests();
        expect (! reqs.empty() && reqs.back().headers.count ("anthropic-version") && reqs.back().headers.at ("anthropic-version") == "2026-10-21");

        const auto list = p.listModels ("good-key", cancel);
        expect (list.ok && list.models.size() == 1 && list.models[0].structuredOutputs == 1);
        const auto diag = p.diagnose ("good-key", cancel);
        expect (diag.ok, diag.error);

        r = p.send (req, "bad-key", cancel);
        expect (! r.ok && r.httpStatus == 401 && r.error.contains ("Authentication"), r.error);
        req.userMessage = "please refuse";
        r = p.send (req, "good-key", cancel);
        expect (! r.ok && r.refused);
        expect (! p.send (req, {}, cancel).ok, "missing key must fail without a request");
    }

    void testChatCompletionsProtocol()
    {
        beginTest ("OpenAI-compatible protocol (local mock server): schema fallback, invalid JSON, missing model");
        std::atomic<int> attempts { 0 };
        MockHttpServer server ([&attempts] (const MockHttpServer::Request& r) -> MockHttpServer::Response
        {
            if (r.path == "/v1/models") return { 200, R"({"object":"list","data":[{"id":"local-model"}]})" };
            if (r.path != "/v1/chat/completions") return { 404, R"({"error":"not found"})" };
            ++attempts;
            const auto body = juce::JSON::parse (r.body);
            if (body["model"].toString() == "missing") return { 404, R"({"error":{"message":"model 'missing' not found"}})" };
            if (body["response_format"]["type"].toString() == "json_schema")
                return { 400, R"({"error":{"message":"response_format json_schema is not supported"}})" };
            if (body["model"].toString() == "garbage")
                return { 200, R"({"choices":[{"message":{"content":"I think a pad would be nice"},"finish_reason":"stop"}]})" };
            return { 200, juce::String (R"({"model":"local-model","choices":[{"message":{"role":"assistant","content":)") + jsonString (validDesignJson())
                            + R"(},"finish_reason":"stop"}],"usage":{"prompt_tokens":7,"completion_tokens":9}})" };
        });
        AISettings s;
        s.provider = AISettings::OpenAICompatible;
        s.openAiBaseUrl = server.url ("/v1");
        s.maxRetries = 0;
        std::atomic<bool> cancel { false };
        AIRequest req;
        req.systemPrompt = "S";
        req.userMessage = "warmer";
        req.responseSchema = PatchOps::responseSchema();
        // empty model -> discovered from /v1/models; json_schema rejected -> json_object accepted
        auto r = ChatCompletionsProvider (s).send (req, {}, cancel);
        expect (r.ok, r.error);
        expectEquals (r.modelUsed, juce::String ("local-model"));
        expectEquals (attempts.load(), 2);
        s.openAiModel = "garbage";
        r = ChatCompletionsProvider (s).send (req, {}, cancel);
        expect (! r.ok && r.error.contains ("JSON"), r.error);
        s.openAiModel = "missing";
        r = ChatCompletionsProvider (s).send (req, {}, cancel);
        expect (! r.ok && r.httpStatus == 404, r.error);
        s.provider = AISettings::OpenAI;
        s.openAiCloudModel = {};
        r = ChatCompletionsProvider (s).send (req, "k", cancel);
        expect (! r.ok && r.error.contains ("model"), "OpenAI requires an explicitly chosen model");
    }

    void testOllamaProtocol()
    {
        beginTest ("Ollama native protocol (local mock server): tags, chat with format schema, missing model");
        MockHttpServer server ([] (const MockHttpServer::Request& r) -> MockHttpServer::Response
        {
            if (r.path == "/api/tags") return { 200, R"({"models":[{"name":"llama3.2:latest","model":"llama3.2:latest"}]})" };
            if (r.path == "/api/chat")
            {
                const auto body = juce::JSON::parse (r.body);
                if (body["model"].toString() == "nope") return { 404, R"({"error":"model \"nope\" not found, try pulling it first"})" };
                if (! body["format"].isObject() || (bool) body["stream"]) return { 400, R"({"error":"bad request"})" };
                return { 200, juce::String (R"({"model":"llama3.2:latest","done":true,"done_reason":"stop","message":{"role":"assistant","content":)")
                                + jsonString (validDesignJson()) + R"(},"prompt_eval_count":11,"eval_count":12})" };
            }
            return { 404, "{}" };
        });
        AISettings s;
        s.provider = AISettings::Ollama;
        s.ollamaBaseUrl = server.url();
        s.maxRetries = 0;
        std::atomic<bool> cancel { false };
        AIRequest req;
        req.systemPrompt = "S";
        req.userMessage = "x";
        req.responseSchema = PatchOps::responseSchema();
        auto r = OllamaProvider (s).send (req, {}, cancel);
        expect (r.ok, r.error);
        expectEquals (r.modelUsed, juce::String ("llama3.2:latest"));
        s.ollamaModel = "nope";
        r = OllamaProvider (s).send (req, {}, cancel);
        expect (! r.ok && r.error.contains ("ollama pull"), r.error);
        s.ollamaBaseUrl = "http://127.0.0.1:9"; // nothing listens on the discard port
        const auto list = OllamaProvider (s).listModels ({}, cancel);
        expect (! list.ok && list.error.contains ("not reachable"), list.error);
    }

    void testCancellation()
    {
        beginTest ("Cancellation aborts an in-flight request quickly");
        MockHttpServer server ([] (const MockHttpServer::Request&) -> MockHttpServer::Response
        {
            return { 200, R"({"models":[]})", 4000 }; // slow server
        });
        AISettings s;
        s.provider = AISettings::Ollama;
        s.ollamaBaseUrl = server.url();
        s.ollamaModel = "slow";
        s.maxRetries = 0;
        std::atomic<bool> cancel { false };
        AIRequest req;
        req.systemPrompt = "S";
        req.userMessage = "x";
        req.responseSchema = PatchOps::responseSchema();
        auto canceller = std::thread ([&cancel] { juce::Thread::sleep (300); cancel = true; });
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto r = OllamaProvider (s).send (req, {}, cancel);
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
        canceller.join();
        expect (! r.ok && r.cancelled, r.error);
        expect (elapsed < 2500.0, "cancel took " + juce::String (elapsed) + " ms");
    }

    static juce::String ollamaReply (const juce::String& designJson)
    {
        return juce::String (R"({"model":"m","done":true,"done_reason":"stop","message":{"role":"assistant","content":)")
               + jsonString (designJson) + "}}";
    }

    static juce::String designJson (const juce::String& action, float cutoff, const juce::String& reply)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("reply", reply);
        o->setProperty ("action", action);
        juce::Array<juce::var> ops;
        if (cutoff > 0.0f)
        {
            auto* op = new juce::DynamicObject();
            op->setProperty ("op", "set");
            op->setProperty ("param", "flt1_cutoff");
            op->setProperty ("value", cutoff);
            ops.add (juce::var (op));
        }
        o->setProperty ("operations", ops);
        return juce::JSON::toString (juce::var (o), true);
    }

    void testLocalAgent()
    {
        beginTest ("Plain, prefixed and snake_case wavetable / sample names are accepted (found by a real local model)");
        {
            for (const juce::String name : { "Basic Shapes", "factory:Basic Shapes", "basic_shapes", "GLASS" })
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("reply", "ok");
                o->setProperty ("action", "modify_patch");
                auto* op = new juce::DynamicObject();
                op->setProperty ("op", "wavetable");
                op->setProperty ("osc", "A");
                op->setProperty ("name", name);
                o->setProperty ("operations", juce::Array<juce::var> { juce::var (op) });
                const auto r = PatchOps::fromResponse (juce::var (o), PatchOps::initPatch(), "test");
                expect (r.hasPatch && r.report.errors.isEmpty(), name + ": " + r.report.errors.joinIntoString ("; "));
            }
        }

        beginTest ("Local agent: the model receives the render / analysis tool result and stops when satisfied");
        {
            std::atomic<int> chats { 0 };
            MockHttpServer server ([&chats] (const MockHttpServer::Request& r) -> MockHttpServer::Response
            {
                if (r.path == "/api/tags") return { 200, R"({"models":[{"name":"m"}]})" };
                const int n = ++chats;
                if (n == 1) return { 200, ollamaReply (designJson ("modify_patch", 900.0f, "Darker.")) };
                return { 200, ollamaReply (designJson ("answer_only", 0.0f, "The measurements match: centroid is low. DONE.")) };
            });
            FakeHost host;
            AIController ai (host);
            auto s = ai.getSettings();
            s.provider = AISettings::Ollama;
            s.ollamaBaseUrl = server.url();
            s.ollamaModel = "m";
            s.optimiseAfterGenerate = false;
            s.fallbackToOffline = false;
            s.agentSteps = 2;
            ai.setSettings (s);
            ai.submit ("make it darker");
            expect (waitIdle (ai, 60000));
            expectEquals (chats.load(), 2, "one answer + one review round (the model said it is done)");
            const auto reqs = server.requests();
            juce::String review;
            for (const auto& q : reqs) if (q.path == "/api/chat" && q.body.contains ("TOOL RESULT")) review = q.body;
            expect (review.contains ("render_and_analyse") && review.contains ("spectral centroid") && review.contains ("make it darker"),
                    "the review request carries the measured analysis and the original request");
            expect (review.contains ("Darker."), "the model's first answer is in the history");
            expectWithinAbsoluteError (host.current.get ("flt1_cutoff"), 900.0f, 0.5f);
            const auto& e = ai.getConversation().back();
            expect (e.measurement.contains ("AI agent") && e.measurement.contains ("Step 1: no change"), e.measurement);
        }

        beginTest ("Local agent: corrective operations from a review are validated and applied; never executed");
        {
            std::atomic<int> chats { 0 };
            MockHttpServer server ([&chats] (const MockHttpServer::Request& r) -> MockHttpServer::Response
            {
                if (r.path == "/api/tags") return { 200, R"({"models":[{"name":"m"}]})" };
                const int n = ++chats;
                if (n == 1) return { 200, ollamaReply (designJson ("new_patch", 900.0f, "First try.")) };
                if (n == 2) return { 200, ollamaReply (designJson ("new_patch", 2500.0f, "Too dark; opened the filter.")) }; // review must not restart
                return { 200, ollamaReply (designJson ("answer_only", 0.0f, "Good now.")) };
            });
            FakeHost host;
            host.current.set ("master_volume", -7.0f);
            AIController ai (host);
            auto s = ai.getSettings();
            s.provider = AISettings::Ollama;
            s.ollamaBaseUrl = server.url();
            s.ollamaModel = "m";
            s.optimiseAfterGenerate = false;
            s.fallbackToOffline = false;
            s.agentSteps = 3;
            ai.setSettings (s);
            const auto undoDepth = host.undoStack.size();
            ai.submit ("a mellow pad");
            expect (waitIdle (ai, 60000));
            expectEquals (chats.load(), 3);
            expectWithinAbsoluteError (host.current.get ("flt1_cutoff"), 2500.0f, 0.5f);
            expectEquals ((int) host.undoStack.size(), (int) undoDepth + 1, "the whole agent run is one undo step");
        }

        beginTest ("Local AI setup wizard: detects a running server and selects its model; explains setup otherwise");
        {
            MockHttpServer server ([] (const MockHttpServer::Request& r) -> MockHttpServer::Response
            {
                if (r.path == "/api/tags") return { 200, R"({"models":[{"name":"qwen-test:7b"},{"name":"other:1b"}]})" };
                return { 404, "{}" };
            });
            FakeHost host;
            AIController ai (host);
            auto s = ai.getSettings();
            s.provider = AISettings::Offline;
            s.ollamaBaseUrl = server.url();
            s.lmStudioBaseUrl = "http://127.0.0.1:9/v1";
            ai.setSettings (s);
            ai.detectLocalAI();
            expect (waitIdle (ai, 30000));
            expectEquals (ai.getSettings().provider, (int) AISettings::Ollama);
            expectEquals (ai.getSettings().model(), juce::String ("qwen-test:7b"));
            expect (ai.getLocalSetupReport().contains ("RUNNING") && ai.getLocalSetupReport().contains ("LM Studio (local): not found"),
                    ai.getLocalSetupReport());

            auto s2 = ai.getSettings();
            s2.provider = AISettings::Offline;
            s2.ollamaBaseUrl = "http://127.0.0.1:9";
            ai.setSettings (s2);
            ai.detectLocalAI();
            expect (waitIdle (ai, 30000));
            expectEquals (ai.getSettings().provider, (int) AISettings::Offline, "nothing found: stays on the labelled offline designer");
            expect (ai.getLocalSetupReport().contains ("ollama pull") && ai.getLocalSetupReport().contains ("rule-based"));
        }
    }

    void testControllerWorkflow()
    {
        beginTest ("Controller: offline generation, undo/redo, A/B compare, change tracking");
        FakeHost host;
        AIController ai (host);
        auto s = ai.getSettings();
        s.provider = AISettings::Offline;
        s.optimiseAfterGenerate = true;
        ai.setSettings (s);
        const auto before = host.current;
        ai.submit ("Make it darker and wider");
        expect (waitIdle (ai, 60000), "offline design did not finish");
        expect (host.canUndo(), "AI result must be applied as one undoable step");
        expect (! ai.getAIChanges().empty(), "changed parameters must be tracked");
        const auto& last = ai.getConversation().back();
        expect (last.kind == ChatEntry::Assistant && ! last.notApplied);
        expect (last.measurement.isNotEmpty(), "closed-loop measurement expected for darker/wider");
        const auto after = host.current;
        expect (ai.canCompare());
        ai.toggleCompare();
        expect (host.current.values == before.values && ai.isComparingBefore());
        ai.toggleCompare();
        expect (host.current.values == after.values);
        expect (ai.undo() && host.current.values == before.values);
        expect (ai.redo() && host.current.values == after.values);

        beginTest ("Controller: stale results are never applied over edits made during the request");
        MockHttpServer server ([] (const MockHttpServer::Request& r) -> MockHttpServer::Response
        {
            if (r.path == "/api/tags") return { 200, R"({"models":[{"name":"m"}]})" };
            return { 200, juce::String (R"({"model":"m","done":true,"done_reason":"stop","message":{"role":"assistant","content":)")
                            + jsonString (validDesignJson()) + "}}", 600 };
        });
        s.provider = AISettings::Ollama;
        s.ollamaBaseUrl = server.url();
        s.ollamaModel = "m";
        s.optimiseAfterGenerate = false;
        s.fallbackToOffline = false;
        ai.setSettings (s);
        const auto undoDepth = host.undoStack.size();
        ai.submit ("darker please");
        juce::Thread::sleep (100);
        host.current.set ("master_volume", -20.0f); // user edit while the request runs
        expect (waitIdle (ai, 20000));
        const auto& e = ai.getConversation().back();
        expect (e.notApplied, "edited patch must not be overwritten");
        expectEquals ((int) host.undoStack.size(), (int) undoDepth);
        expect (! e.presets.empty(), "the result stays available as a card");

        beginTest ("Controller: cancelled requests change nothing; one request at a time");
        auto slowServer = std::make_unique<MockHttpServer> ([] (const MockHttpServer::Request& r) -> MockHttpServer::Response
        {
            if (r.path == "/api/tags") return { 200, R"({"models":[{"name":"m"}]})" };
            return { 200, juce::String (R"({"done":true,"message":{"content":)") + jsonString (validDesignJson()) + "}}", 3000 };
        });
        s.ollamaBaseUrl = slowServer->url();
        ai.setSettings (s);
        const auto snapshot = host.current;
        const auto convSize = ai.getConversation().size();
        ai.submit ("first");
        ai.submit ("second (must be ignored while busy)");
        juce::Thread::sleep (200);
        ai.cancel();
        expect (waitIdle (ai, 3000), "cancel must finish promptly");
        expect (host.current.values == snapshot.values);
        int users = 0;
        for (size_t i = convSize; i < ai.getConversation().size(); ++i) users += ai.getConversation()[i].kind == ChatEntry::User ? 1 : 0;
        expectEquals (users, 1);

        beginTest ("Controller: missing key falls back to offline (when allowed) and says so");
        s.provider = AISettings::Anthropic;
        s.fallbackToOffline = true;
        ai.setSettings (s);
        if (! ai.hasApiKey())
        {
            ai.submit ("a warm analog pad");
            expect (waitIdle (ai, 60000));
            const auto& fb = ai.getConversation().back();
            expect (fb.text.contains ("offline designer"), fb.text);
        }
    }
};

static AIProviderTests aiProviderTests;
} // namespace vsx
