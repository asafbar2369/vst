#pragma once

#include "Widgets.h"

namespace vsx
{
/**
    AI connection settings: operating mode / provider, server address, model selection with
    automatic discovery (Anthropic /v1/models, OpenAI-compatible /models, Ollama /api/tags),
    API key management (OS-encrypted storage, never in presets), privacy consent for sending
    reference-audio analysis to cloud services, verification and matching budget, and a real
    connection test.
*/
class AISettingsPanel : public juce::Component, private juce::Timer
{
public:
    explicit AISettingsPanel (ViceSynthProcessor& p);
    ~AISettingsPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void loadFromSettings();
    void storeSettings();
    void rebuildModelList();
    juce::Label* addLabel (const juce::String& text);

    ViceSynthProcessor& proc;
    AIController& ai;
    juce::ComboBox provider, model, effort;
    juce::TextEditor baseUrl, apiKey;
    juce::TextButton refreshModels { "REFRESH MODELS" }, saveKey { "SAVE KEY" }, removeKey { "REMOVE KEY" }, test { "TEST CONNECTION" },
        resetUrl { "DEFAULT" };
    juce::ToggleButton serverFallback { "Claude: server-side refusal fallback" }, offlineFallback { "Use the offline designer if the service fails" },
        autoPreview { "Auto-preview generated sounds" }, verify { "Verify results by rendering & measuring (closed loop)" },
        shareRef { "Share reference-audio ANALYSIS with cloud services (numbers only, never audio)" };
    juce::Slider timeout, matchIterations;
    juce::Label modeInfo, keyStatus, security, modelsStatus, testResult;
    juce::OwnedArray<juce::Label> labels;
    juce::Label *lProvider = nullptr, *lUrl = nullptr, *lModel = nullptr, *lKey = nullptr, *lEffort = nullptr, *lTimeout = nullptr, *lMatch = nullptr;
    bool loading = false;
    juce::String lastModelsStatus;
};

} // namespace vsx
