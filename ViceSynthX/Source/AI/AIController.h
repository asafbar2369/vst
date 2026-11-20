#pragma once

#include "AIProvider.h"
#include "OfflineDesigner.h"
#include "SecureStorage.h"
#include "../Analysis/AudioFeatures.h"
#include <deque>

namespace vsx
{
class PresetManager;
class OfflineRenderer;

/** Interface the AI layer uses to read/modify the instrument (implemented by the processor). */
struct PatchHost
{
    virtual ~PatchHost() = default;
    virtual PatchSpec getCurrentPatch() = 0;
    virtual void applyPatch (const PatchSpec& spec, const juce::String& undoLabel) = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
    virtual bool canUndo() const = 0;
    virtual bool canRedo() const = 0;
    virtual void audition (int note, int velocity, double seconds) = 0;
    virtual PresetManager& getPresetManager() = 0;
};

struct ChatEntry
{
    enum Kind { User, Assistant, Info, Error };
    Kind kind = Info;
    juce::String text;
    juce::String engine;
    std::vector<PatchSpec> presets;   // generated preset (first) and variations
    juce::Time time = juce::Time::getCurrentTime();
};

/**
    Conversational AI sound designer. All network requests, audio analysis and candidate
    rendering run on a dedicated worker thread; results are applied on the message thread
    as single undoable transactions. Never touches the audio callback.
*/
class AIController : private juce::Thread, private juce::AsyncUpdater
{
public:
    explicit AIController (PatchHost& host);
    ~AIController() override;

    void submit (const juce::String& prompt);
    void cancel();
    bool isBusy() const { return busy.load(); }
    juce::String getStatus() const;
    float getProgress() const { return progress.load(); }

    const std::deque<ChatEntry>& getConversation() const { return conversation; }
    void clearConversation();
    void loadPreset (const PatchSpec& spec);
    juce::File savePreset (const PatchSpec& spec, juce::String& error);
    bool undo();
    bool redo();

    // reference audio
    void loadReference (const juce::File& file);
    void matchReference (int iterations = 40);
    bool hasReference() const { return reference.valid; }
    const AudioFeatures& getReference() const { return reference; }
    juce::String getReferenceName() const { return referenceName; }

    // settings / credentials
    AISettings getSettings() const { return settings; }
    void setSettings (const AISettings& s);
    void setApiKey (const juce::String& key);
    bool hasApiKey() const;
    juce::String maskedKey() const;
    void testConnection();
    SecureStorage& getStorage() { return storage; }

    std::function<void()> onUpdate; // message thread (legacy single callback)
    void addUpdateListener (void* owner, std::function<void()> fn) { removeUpdateListener (owner); listeners.push_back ({ owner, std::move (fn) }); }
    void removeUpdateListener (void* owner)
    {
        listeners.erase (std::remove_if (listeners.begin(), listeners.end(), [owner] (const auto& l) { return l.first == owner; }), listeners.end());
    }
    void notifyUpdate()
    {
        if (onUpdate) onUpdate();
        auto copy = listeners;
        for (auto& l : copy) if (l.second) l.second();
    }

    juce::var toVar() const;
    void fromVar (const juce::var& v);

    /** Synchronous design (used by tests and the offline path). */
    DesignResult designNow (const juce::String& prompt, const PatchSpec& current);

private:
    struct Job
    {
        enum Type { Prompt, Analyse, Match, Test } type = Prompt;
        juce::String text;
        juce::File file;
        PatchSpec current;
        std::vector<AIMessage> history;
        AISettings settings;
        juce::String apiKey;
        int iterations = 40;
    };
    struct Result
    {
        Job::Type type = Job::Prompt;
        bool ok = false;
        juce::String message;
        DesignResult design;
        AudioFeatures features;
        juce::String referenceName;
        bool applyPatch = false;
        juce::String undoLabel;
    };

    void run() override;
    void handleAsyncUpdate() override;
    void enqueue (Job job);
    void post (Result r);
    void setStatus (const juce::String& s);
    Result runPrompt (const Job& job);
    OfflineRenderer& renderer();

    PatchHost& host;
    SecureStorage storage;
    AISettings settings;
    OfflineDesigner offline;
    std::deque<ChatEntry> conversation;

    juce::CriticalSection lock;
    std::deque<Job> jobs;
    std::deque<Result> results;
    juce::WaitableEvent wake;
    std::atomic<bool> busy { false }, cancelFlag { false };
    std::atomic<float> progress { 0.0f };
    juce::String status { "Ready" };
    AudioFeatures reference;
    juce::String referenceName;
    std::unique_ptr<OfflineRenderer> offlineRenderer; // worker thread only
    juce::String cachedSystemPrompt;
    std::vector<std::pair<void*, std::function<void()>>> listeners;
};

} // namespace vsx
