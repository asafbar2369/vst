#include "AIController.h"
#include "../Analysis/SoundMatcher.h"
#include "../Presets/PresetManager.h"

namespace vsx
{
AIController::AIController (PatchHost& h) : juce::Thread ("VSX AI Worker"), host (h)
{
    settings = AISettings::load();
    conversation.push_back ({ ChatEntry::Info,
                              "Hi! Describe a sound and I will build a playable preset - e.g. \"Create a warm 1990s digital pad\" or "
                              "\"Make this darker and wider\". Works offline with the built-in designer; connect Claude or another "
                              "provider in Settings for richer results.", "", {} });
}

AIController::~AIController()
{
    cancelFlag = true;
    signalThreadShouldExit();
    wake.signal();
    stopThread (20000);
    cancelPendingUpdate();
}

juce::String AIController::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void AIController::setStatus (const juce::String& s)
{
    {
        const juce::ScopedLock sl (lock);
        status = s;
    }
    triggerAsyncUpdate();
}

void AIController::setSettings (const AISettings& s)
{
    if (s.provider != settings.provider || s.baseUrl() != settings.baseUrl())
    {
        models.clear();
        modelsStatus.clear();
        modelsProvider = -1;
        lastConnection.clear();
    }
    settings = s;
    settings.save();
}

juce::String AIController::engineLabel() const
{
    if (settings.provider == AISettings::Offline) return "OFFLINE - Local rule-based designer";
    auto label = settings.modeLabel() + " - " + AISettings::providerName (settings.provider);
    const auto m = settings.model();
    label << " (" << (m.isNotEmpty() ? m : juce::String ("auto model")) << ")";
    if (settings.requiresApiKey() && ! hasApiKey())
        label << (settings.fallbackToOffline ? " - no key, offline fallback" : " - no API key");
    return label;
}

bool AIController::isSupportedAudioFile (const juce::File& f)
{
    return f.existsAsFile() && f.hasFileExtension ("wav;wave;aif;aiff;flac");
}

void AIController::setApiKey (const juce::String& key)
{
    storage.storeKey (settings.providerId(), key.trim());
}

bool AIController::hasApiKey() const { return storage.hasKey (settings.providerId()); }
juce::String AIController::maskedKey() const { return SecureStorage::mask (storage.loadKey (settings.providerId())); }

void AIController::enqueue (Job job)
{
    {
        const juce::ScopedLock sl (lock);
        jobs.push_back (std::move (job));
    }
    if (! isThreadRunning()) startThread (juce::Thread::Priority::low);
    wake.signal();
}

bool AIController::startJob (Job job, const juce::String& statusText)
{
    // One active operation at a time. A cancellation flag is only reset when no
    // earlier request can still be using it on the worker thread.
    if (busy.load()) return false;
    cancelFlag = false;
    busy = true;
    progress = 0.0f;
    setStatus (statusText);
    enqueue (std::move (job));
    notifyUpdate();
    return true;
}

void AIController::post (Result r)
{
    {
        const juce::ScopedLock sl (lock);
        results.push_back (std::move (r));
    }
    triggerAsyncUpdate();
}

void AIController::submit (const juce::String& promptIn)
{
    if (busy.load()) return;
    const auto prompt = promptIn.trim().substring (0, 2000);
    if (prompt.isEmpty()) return;
    if (containsHebrew (prompt))
    {
        conversation.push_back ({ ChatEntry::Error, "AI sound design supports English only. Please enter your request in English.", "", {} });
        notifyUpdate();
        return;
    }

    Job j;
    j.type = Job::Prompt;
    j.text = prompt;
    j.current = host.getCurrentPatch();
    j.referenceSnapshot = reference;
    j.referenceNameSnapshot = referenceName;
    j.settings = settings;
    j.apiKey = settings.provider != AISettings::Offline ? storage.loadKey (settings.providerId()) : juce::String();
    // conversation context: last exchanges (text only, never credentials)
    int count = 0;
    for (auto it = conversation.rbegin(); it != conversation.rend() && count < 12; ++it)
        if ((it->kind == ChatEntry::User || it->kind == ChatEntry::Assistant) && ! containsHebrew (it->text))
        {
            j.history.insert (j.history.begin(), { it->kind == ChatEntry::User ? "user" : "assistant", it->text });
            ++count;
        }
    conversation.push_back ({ ChatEntry::User, prompt, "", {} });
    startJob (std::move (j), settings.provider == AISettings::Offline ? "Designing (offline)..."
                                                                       : "Contacting " + AISettings::providerName (settings.provider) + "...");
}

void AIController::cancel()
{
    if (! busy.load()) return;
    // Leave a queued job in place so the worker publishes a completion even
    // if cancellation happens before it has picked up that job.
    cancelFlag = true;
    setStatus ("Cancelling...");
}

void AIController::clearConversation()
{
    conversation.clear();
    conversation.push_back ({ ChatEntry::Info, "Conversation cleared.", "", {} });
    notifyUpdate();
}

void AIController::loadPreset (const PatchSpec& spec)
{
    host.applyPatch (spec, "Load " + spec.meta.name);
    if (settings.autoPreview) host.audition (60, 100, 1.5);
}

juce::File AIController::savePreset (const PatchSpec& spec, juce::String& error)
{
    auto f = host.getPresetManager().save (spec, true, error);
    conversation.push_back ({ f.existsAsFile() ? ChatEntry::Info : ChatEntry::Error,
                              f.existsAsFile() ? "Saved \"" + spec.meta.name + "\" to AI Generated presets." : "Save failed: " + error, "", {} });
    notifyUpdate();
    return f;
}

bool AIController::undo()
{
    const bool ok = host.undo();
    conversation.push_back ({ ChatEntry::Info, ok ? "Undid the last change." : "Nothing to undo.", "", {} });
    notifyUpdate();
    return ok;
}

bool AIController::redo()
{
    const bool ok = host.redo();
    conversation.push_back ({ ChatEntry::Info, ok ? "Redid the change." : "Nothing to redo.", "", {} });
    notifyUpdate();
    return ok;
}

void AIController::loadReference (const juce::File& file)
{
    if (busy.load()) return;
    if (! isSupportedAudioFile (file))
    {
        conversation.push_back ({ ChatEntry::Error, "Unsupported file \"" + file.getFileName() + "\" (use WAV, AIFF or FLAC).", "", {} });
        notifyUpdate();
        return;
    }
    Job j;
    j.type = Job::Analyse;
    j.file = file;
    startJob (std::move (j), "Analysing " + file.getFileName() + "...");
}

void AIController::matchReference (int iterations)
{
    if (busy.load()) return;
    if (! reference.valid)
    {
        conversation.push_back ({ ChatEntry::Error, "Load a reference audio file first (drop a WAV / AIFF on the AI panel).", "", {} });
        notifyUpdate();
        return;
    }
    Job j;
    j.type = Job::Match;
    j.iterations = juce::jlimit (1, 1000, iterations > 0 ? iterations : settings.matchIterations);
    j.current = host.getCurrentPatch();
    j.referenceSnapshot = reference;
    j.referenceNameSnapshot = referenceName;
    j.settings = settings;
    startJob (std::move (j), "Matching reference...");
}

void AIController::testConnection()
{
    Job j;
    j.type = Job::Test;
    j.settings = settings;
    j.apiKey = storage.loadKey (settings.providerId());
    startJob (std::move (j), "Testing connection to " + AISettings::providerName (settings.provider) + "...");
}

void AIController::refreshModels()
{
    if (settings.provider == AISettings::Offline)
    {
        models.clear();
        modelsStatus = "The offline designer needs no model.";
        notifyUpdate();
        return;
    }
    Job j;
    j.type = Job::ListModels;
    j.settings = settings;
    j.apiKey = storage.loadKey (settings.providerId());
    startJob (std::move (j), "Listing models...");
}

bool AIController::canCompare()
{
    if (! compareValid) return false;
    // only valid while the instrument still holds one of the two compared states
    const auto now = host.getCurrentPatch();
    const auto& expected = showingBefore ? compareBefore : compareAfter;
    for (size_t i = 0; i < now.values.size() && i < expected.values.size(); ++i)
        if (std::abs (now.values[i] - expected.values[i]) > 1.0e-5f) { compareValid = false; showingBefore = false; return false; }
    return true;
}

void AIController::toggleCompare()
{
    if (! canCompare()) return;
    showingBefore = ! showingBefore;
    host.previewPatch (showingBefore ? compareBefore : compareAfter);
    if (settings.autoPreview) host.audition (60, 100, 1.2);
    notifyUpdate();
}

OfflineRenderer& AIController::renderer()
{
    if (offlineRenderer == nullptr)
        offlineRenderer = std::make_unique<OfflineRenderer> (44100.0);
    return *offlineRenderer;
}

DesignResult AIController::designNow (const juce::String& prompt, const PatchSpec& current)
{
    return offline.design (prompt, current, reference.valid ? &reference : nullptr);
}

AIController::Result AIController::runPrompt (const Job& job)
{
    Result res;
    res.type = Job::Prompt;
    res.basePatch = job.current;
    DesignResult dr;
    juce::String notice;

    auto provider = createProvider (job.settings);
    if (provider != nullptr && job.settings.requiresApiKey() && job.apiKey.isEmpty())
    {
        if (! job.settings.fallbackToOffline)
        {
            res.message = "No API key stored for " + AISettings::providerName (job.settings.provider) + " (Settings > AI).";
            return res;
        }
        notice = "No API key stored for " + AISettings::providerName (job.settings.provider) + ": used the built-in offline designer.";
        provider.reset();
    }

    if (provider != nullptr)
    {
        if (cachedSystemPrompt.isEmpty()) cachedSystemPrompt = PatchOps::systemPrompt();
        // Reference analysis (numbers only, never audio) goes to cloud services only with explicit consent.
        juce::String refSummary;
        if (job.referenceSnapshot.valid)
        {
            if (! job.settings.isCloud() || job.settings.shareReferenceAnalysis) refSummary = job.referenceSnapshot.describe();
            else notice = "The reference analysis was NOT sent to the cloud service (enable \"Share reference analysis\" in Settings > AI). "
                          "Audio files are never uploaded.";
        }
        AIRequest req;
        req.systemPrompt = cachedSystemPrompt;
        req.context = PatchOps::contextFor (job.current, refSummary);
        req.history = job.history;
        req.userMessage = job.text;
        req.responseSchema = PatchOps::responseSchema();
        const auto r = provider->send (req, job.apiKey, cancelFlag);
        if (cancelFlag.load() || r.cancelled)
        {
            res.message = "Request cancelled.";
            return res;
        }
        if (r.ok)
        {
            dr = PatchOps::fromResponse (r.json, job.current, job.text);
            dr.engine = job.settings.modeLabel() + " - " + AISettings::providerName (job.settings.provider)
                      + (r.modelUsed.isNotEmpty() ? " (" + r.modelUsed + ")" : juce::String())
                      + " - " + juce::String (r.latencyMs / 1000.0, 1) + " s";
            if (r.inputTokens + r.outputTokens > 0)
                dr.engine << " - " << r.inputTokens << " in / " << r.outputTokens << " out tokens";
        }
        else if (job.settings.fallbackToOffline)
        {
            notice = AISettings::providerName (job.settings.provider) + " failed (" + r.error + "). Used the built-in offline designer instead.";
            dr = offline.design (job.text, job.current, job.referenceSnapshot.valid ? &job.referenceSnapshot : nullptr);
        }
        else
        {
            res.message = AISettings::providerName (job.settings.provider) + " request failed: " + r.error;
            return res;
        }
    }
    else
        dr = offline.design (job.text, job.current, job.referenceSnapshot.valid ? &job.referenceSnapshot : nullptr);

    // closed loop: render before/after, measure, refine towards the stated perceptual goals
    if (dr.hasPatch && job.settings.optimiseAfterGenerate && ! dr.descriptors.isEmpty() && ! cancelFlag.load())
    {
        setStatus ("Rendering and measuring the result...");
        juce::String log;
        dr.patch = SoundMatcher::refineDescriptors (job.current, dr.patch, dr.descriptors, renderer(), log);
        if (log.isNotEmpty()) res.measurement = log.trim();
    }
    else if (dr.hasPatch && ! dr.descriptors.isEmpty())
        res.measurement = "Not measured (render-and-measure verification is off in Settings > AI). Perceptual goals are estimates.";
    if (cancelFlag.load())
    {
        res.message = "Request cancelled.";
        return res;
    }
    if (notice.isNotEmpty()) dr.reply = notice + "\n\n" + dr.reply;
    if (! dr.report.errors.isEmpty())
        dr.reply << "\n\nRejected " << dr.report.errors.size() << " invalid operation(s): " << dr.report.errors.joinIntoString ("; ").substring (0, 400);

    res.ok = true;
    res.design = std::move (dr);
    res.applyPatch = res.design.hasPatch;
    res.undoLabel = "AI: " + job.text.substring (0, 40);
    return res;
}

void AIController::run()
{
    while (! threadShouldExit())
    {
        Job job;
        bool have = false;
        {
            const juce::ScopedLock sl (lock);
            if (! jobs.empty()) { job = std::move (jobs.front()); jobs.pop_front(); have = true; }
        }
        if (! have)
        {
            wake.wait (500);
            continue;
        }

        Result res;
        res.type = job.type;
        if (cancelFlag.load() || threadShouldExit())
        {
            res.message = "Request cancelled.";
        }
        else switch (job.type)
        {
            case Job::Prompt: res = runPrompt (job); break;
            case Job::Analyse:
            {
                juce::AudioFormatManager fm;
                fm.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (job.file));
                if (reader == nullptr) { res.message = "Cannot read " + job.file.getFileName(); break; }
                const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
                juce::AudioBuffer<float> buf ((int) std::min<unsigned int> (2, reader->numChannels), len);
                reader->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);
                res.features = AudioFeatures::analyse (buf, reader->sampleRate);
                res.referenceName = job.file.getFileName();
                res.ok = res.features.valid;
                res.message = res.ok ? "Analysed \"" + job.file.getFileName() + "\":\n" + res.features.describe()
                                     : "The file is silent or too short to analyse.";
                break;
            }
            case Job::Match:
            {
                int note = 60;
                auto start = SoundMatcher::initialGuess (job.referenceSnapshot, note);
                auto m = SoundMatcher::optimise (job.referenceSnapshot, start, note, job.iterations, renderer(), [this] (float p, float s)
                {
                    progress = p;
                    setStatus ("Matching... " + juce::String ((int) (p * 100.0f)) + "% (similarity " + juce::String ((int) (s * 100.0f)) + "%)");
                    return ! cancelFlag.load() && ! threadShouldExit();
                });
                res.ok = ! m.cancelled;
                res.basePatch = job.current;
                res.design.hasPatch = res.ok;
                res.measurement = "Similarity " + juce::String (m.initialSimilarity, 3) + " (initial analysis-based patch) -> "
                                + juce::String (m.finalSimilarity, 3) + " (best of " + juce::String (m.renders) + " rendered candidates). "
                                  "Internal feature similarity (spectrum, harmonics, envelope, brightness, width, modulation; 0..1), "
                                  "not a validated perceptual score.";
                m.best.meta.name = "Match: " + job.referenceNameSnapshot.upToLastOccurrenceOf (".", false, false);
                m.best.meta.category = "Digital";
                m.best.meta.source = "ai";
                res.design.patch = m.best;
                res.design.engine = "Sound-matching engine";
                res.design.reply = "Built a patch from the reference analysis and refined it by rendering candidates. " + m.report + " "
                                 + SoundMatcher::limitationsFor (job.referenceSnapshot);
                res.applyPatch = res.ok;
                res.undoLabel = "Match reference";
                if (! res.ok) res.message = "Matching cancelled.";
                break;
            }
            case Job::Test:
            {
                auto provider = createProvider (job.settings);
                res.provider = job.settings.provider;
                if (provider == nullptr) { res.ok = true; res.message = "Offline designer selected: no connection needed."; break; }
                const auto r = provider->diagnose (job.apiKey, cancelFlag);
                res.ok = r.ok;
                res.message = r.ok ? "Connected to " + AISettings::providerName (job.settings.provider) + " (HTTP " + juce::String (r.httpStatus) + ", "
                                       + juce::String ((int) r.latencyMs) + " ms): " + r.raw + "."
                                   : "Connection test failed: " + (r.error.isNotEmpty() ? r.error : r.raw);
                break;
            }
            case Job::ListModels:
            {
                auto provider = createProvider (job.settings);
                res.provider = job.settings.provider;
                if (provider == nullptr) { res.ok = true; break; }
                const auto list = provider->listModels (job.apiKey, cancelFlag);
                res.ok = list.ok;
                res.models = list.models;
                res.message = list.ok ? juce::String ((int) list.models.size()) + " model(s) found at " + AISettings::providerName (job.settings.provider)
                                            + " (" + juce::String ((int) list.latencyMs) + " ms)."
                                      : "Could not list models: " + list.error;
                break;
            }
        }
        progress = 0.0f;
        post (std::move (res));
    }
}

void AIController::applyResultPatch (const Result& r)
{
    const auto before = host.getCurrentPatch();
    host.applyPatch (r.design.patch, r.undoLabel);
    compareBefore = before;
    compareAfter = host.getCurrentPatch();
    compareValid = true;
    showingBefore = false;
    aiChanges.clear();
    for (size_t i = 0; i < compareAfter.values.size() && i < before.values.size(); ++i)
        if (std::abs (compareAfter.values[i] - before.values[i]) > 1.0e-6f)
            aiChanges.push_back ({ (int) i, compareAfter.values[i] });
    ++aiChangeSerial;
    if (settings.autoPreview) host.audition (60, 100, 1.6);
}

void AIController::handleAsyncUpdate()
{
    std::deque<Result> pending;
    {
        const juce::ScopedLock sl (lock);
        pending.swap (results);
    }
    for (auto& r : pending)
    {
        // Cancellation can arrive after a worker completed but before the
        // message thread handled its result. Never apply such a patch.
        if (cancelFlag.load())
        {
            conversation.push_back ({ ChatEntry::Info, "Request cancelled. No changes applied.", "", {} });
            continue;
        }
        switch (r.type)
        {
            case Job::Prompt:
            case Job::Match:
            {
                if (! r.ok)
                {
                    conversation.push_back ({ ChatEntry::Error, r.message, "", {} });
                    break;
                }
                ChatEntry e;
                e.kind = ChatEntry::Assistant;
                e.text = r.design.question.isNotEmpty() && ! r.design.hasPatch ? r.design.question : r.design.reply;
                e.engine = r.design.engine;
                e.measurement = r.measurement;
                if (r.design.hasPatch) e.presets.push_back (r.design.patch);
                for (auto& v : r.design.variations) e.presets.push_back (v);
                if (r.applyPatch)
                {
                    // stale-result protection: never overwrite edits made while the request was running
                    const auto now = host.getCurrentPatch();
                    bool changed = false;
                    for (size_t i = 0; i < now.values.size() && i < r.basePatch.values.size() && ! changed; ++i)
                        changed = std::abs (now.values[i] - r.basePatch.values[i]) > 1.0e-5f;
                    if (changed)
                    {
                        e.notApplied = true;
                        e.text << "\n\nThe sound was edited while this request was running, so the result was NOT applied automatically. "
                                  "Use LOAD on the result to apply it.";
                    }
                    else
                        applyResultPatch (r);
                }
                conversation.push_back (e);
                break;
            }
            case Job::Analyse:
                if (r.ok) { reference = r.features; referenceName = r.referenceName; }
                conversation.push_back ({ r.ok ? ChatEntry::Info : ChatEntry::Error, r.message, "", {} });
                break;
            case Job::Test:
                lastConnection = r.message;
                lastConnectionOk = r.ok;
                conversation.push_back ({ r.ok ? ChatEntry::Info : ChatEntry::Error, r.message, "", {} });
                break;
            case Job::ListModels:
                if (r.ok) models = r.models;
                modelsStatus = r.message;
                modelsProvider = r.provider;
                break;
        }
    }
    while (conversation.size() > 200) conversation.pop_front();

    bool idle;
    {
        const juce::ScopedLock sl (lock);
        idle = jobs.empty();
    }
    if (! pending.empty() && idle)
    {
        busy = false;
        progress = 0.0f;
        const juce::ScopedLock sl (lock);
        status = "Ready";
    }
    notifyUpdate();
}

juce::var AIController::toVar() const
{
    juce::Array<juce::var> arr;
    const int start = std::max (0, (int) conversation.size() - 40);
    for (int i = start; i < (int) conversation.size(); ++i)
    {
        const auto& e = conversation[(size_t) i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("kind", (int) e.kind);
        o->setProperty ("text", e.text);
        o->setProperty ("engine", e.engine);
        arr.add (juce::var (o));
    }
    return arr;
}

void AIController::fromVar (const juce::var& v)
{
    if (auto* arr = v.getArray(); arr != nullptr && ! arr->isEmpty())
    {
        conversation.clear();
        for (const auto& e : *arr)
            conversation.push_back ({ (ChatEntry::Kind) juce::jlimit (0, 3, (int) e["kind"]), e["text"].toString(), e["engine"].toString(), {} });
        notifyUpdate();
    }
}

} // namespace vsx
