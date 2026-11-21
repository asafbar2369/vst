#include "AISettingsPanel.h"

namespace vsx
{
static juce::String defaultUrlFor (int provider)
{
    const AISettings d;
    switch (provider)
    {
        case AISettings::Anthropic:        return d.anthropicBaseUrl;
        case AISettings::OpenAI:           return d.openAiCloudBaseUrl;
        case AISettings::Ollama:           return d.ollamaBaseUrl;
        case AISettings::LMStudio:         return d.lmStudioBaseUrl;
        case AISettings::OpenAICompatible: return d.openAiBaseUrl;
        default:                           return {};
    }
}

static void setUrlFor (AISettings& s, const juce::String& url)
{
    switch (s.provider)
    {
        case AISettings::Anthropic:        s.anthropicBaseUrl = url; break;
        case AISettings::OpenAI:           s.openAiCloudBaseUrl = url; break;
        case AISettings::Ollama:           s.ollamaBaseUrl = url; break;
        case AISettings::LMStudio:         s.lmStudioBaseUrl = url; break;
        case AISettings::OpenAICompatible: s.openAiBaseUrl = url; break;
        default: break;
    }
}

juce::Label* AISettingsPanel::addLabel (const juce::String& t)
{
    auto* l = labels.add (new juce::Label ({}, t));
    l->setFont (Theme::font (12.0f, true));
    l->setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (l);
    return l;
}

AISettingsPanel::AISettingsPanel (ViceSynthProcessor& p) : proc (p), ai (p.getAI())
{
    for (int i = 0; i < AISettings::NumProviders; ++i) provider.addItem (AISettings::providerName (i), i + 1);
    effort.addItemList ({ "low", "medium", "high", "xhigh", "max" }, 1);
    model.setEditableText (true);
    model.setTextWhenNothingSelected ("(type a model id or press REFRESH MODELS)");
    lProvider = addLabel ("Mode / provider"); lUrl = addLabel ("Server address"); lModel = addLabel ("Model");
    lKey = addLabel ("API key"); lEffort = addLabel ("Claude effort"); lTimeout = addLabel ("Timeout (s)"); lMatch = addLabel ("Match budget"); lAgent = addLabel ("Agent steps");

    apiKey.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    apiKey.setTextToShowWhenEmpty ("Paste key, then SAVE KEY", Theme::textDim);
    for (auto* s : { &timeout, &matchIterations, &agentSteps })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
        s->setColour (juce::Slider::trackColourId, Theme::cyberCyan);
    }
    timeout.setRange (10, 600, 5);
    matchIterations.setRange (5, 400, 5);
    agentSteps.setRange (0, 4, 1);
    agentSteps.setTooltip ("AI providers: after the first answer, the patch is rendered and analysed here and the measurements are "
                           "returned to the model, which may correct the patch (up to this many rounds). 0 = off. Only validated "
                           "parameter operations are applied; nothing the model writes is executed.");
    detectLocal.setComponentID ("vsx_detect_local_ai");
    detectLocal.setTooltip ("Look for Ollama and LM Studio on this computer, list their models and select one (no API key, nothing leaves "
                            "your computer). If none is running, setup steps are shown.");
    detectLocal.setColour (juce::TextButton::buttonColourId, Theme::cyberCyan.withAlpha (0.3f));
    localReport.setMultiLine (true, true);
    localReport.setReadOnly (true);
    localReport.setScrollbarsShown (true);
    localReport.setFont (Theme::font (11.5f));
    localReport.setColour (juce::TextEditor::backgroundColourId, Theme::display);
    localReport.setTextToShowWhenEmpty ("Local AI setup: press DETECT LOCAL AI (Ollama / LM Studio).", Theme::textDim);
    matchIterations.setTooltip ("Number of candidate renders used by reference matching: more = closer match, longer processing "
                                "(roughly 60-100 ms per candidate on a modern CPU).");
    shareRef.setTooltip ("Off by default. When on, the measured features of a loaded reference (pitch, envelope, spectrum, brightness, width...) "
                         "are included in requests to CLOUD providers. Audio files are never uploaded by VICE SYNTH X. "
                         "Local and offline engines always use the analysis.");

    for (auto* l : { &modeInfo, &keyStatus, &security, &modelsStatus, &testResult })
    {
        l->setFont (Theme::font (11.0f));
        l->setColour (juce::Label::textColourId, Theme::textDim);
        l->setJustificationType (juce::Justification::topLeft);
        l->setMinimumHorizontalScale (1.0f);
    }
    keyStatus.setColour (juce::Label::textColourId, Theme::cyberCyan);
    security.setText (SecureStorage::isStrongEncryptionAvailable()
                          ? "Keys are encrypted with Windows DPAPI for your user account; never stored in presets, projects or logs."
                          : "This platform build only obfuscates stored keys (owner-only file permissions). The Windows build uses DPAPI encryption.",
                      juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &provider, &model, &effort, &baseUrl, &apiKey, &refreshModels, &saveKey, &removeKey,
                                                            &test, &resetUrl, &serverFallback, &offlineFallback, &autoPreview, &verify, &shareRef,
                                                            &timeout, &matchIterations, &agentSteps, &detectLocal, &localReport,
                                                            &modeInfo, &keyStatus, &security, &modelsStatus, &testResult })
        addAndMakeVisible (c);

    auto changed = [this] { if (! loading) storeSettings(); };
    provider.onChange = [this]
    {
        if (loading) return;
        auto s = ai.getSettings();
        s.provider = juce::jlimit (0, AISettings::NumProviders - 1, provider.getSelectedId() - 1);
        ai.setSettings (s);
        loadFromSettings();
    };
    effort.onChange = changed;
    model.onChange = changed;
    baseUrl.onFocusLost = changed;
    baseUrl.onReturnKey = changed;
    for (auto* tb : { &serverFallback, &offlineFallback, &autoPreview, &verify, &shareRef }) tb->onClick = changed;
    timeout.onValueChange = changed;
    matchIterations.onValueChange = changed;
    agentSteps.onValueChange = changed;
    detectLocal.onClick = [this] { storeSettings(); ai.detectLocalAI(); };
    resetUrl.onClick = [this] { baseUrl.setText (defaultUrlFor (ai.getSettings().provider), false); storeSettings(); };
    refreshModels.onClick = [this] { storeSettings(); ai.refreshModels(); };
    saveKey.onClick = [this]
    {
        storeSettings();
        if (apiKey.getText().trim().isNotEmpty()) ai.setApiKey (apiKey.getText());
        apiKey.clear();
        timerCallback();
    };
    removeKey.onClick = [this] { ai.getStorage().removeKey (ai.getSettings().providerId()); timerCallback(); };
    test.onClick = [this] { storeSettings(); ai.testConnection(); };
    loadFromSettings();
    startTimerHz (4);
}

AISettingsPanel::~AISettingsPanel() { stopTimer(); }

void AISettingsPanel::loadFromSettings()
{
    loading = true;
    const auto s = ai.getSettings();
    provider.setSelectedId (s.provider + 1, juce::dontSendNotification);
    baseUrl.setText (s.baseUrl(), false);
    effort.setText (s.effort, juce::dontSendNotification);
    serverFallback.setToggleState (s.serverSideFallback, juce::dontSendNotification);
    offlineFallback.setToggleState (s.fallbackToOffline, juce::dontSendNotification);
    autoPreview.setToggleState (s.autoPreview, juce::dontSendNotification);
    verify.setToggleState (s.optimiseAfterGenerate, juce::dontSendNotification);
    shareRef.setToggleState (s.shareReferenceAnalysis, juce::dontSendNotification);
    timeout.setValue (s.timeoutSeconds, juce::dontSendNotification);
    matchIterations.setValue (s.matchIterations, juce::dontSendNotification);
    agentSteps.setValue (s.agentSteps, juce::dontSendNotification);
    rebuildModelList();
    const bool offline = s.provider == AISettings::Offline;
    const bool claude = s.provider == AISettings::Anthropic;
    for (juce::Component* c : { (juce::Component*) &baseUrl, (juce::Component*) &model, (juce::Component*) &refreshModels, (juce::Component*) &apiKey,
                                (juce::Component*) &saveKey, (juce::Component*) &removeKey, (juce::Component*) &resetUrl, (juce::Component*) &test,
                                (juce::Component*) lUrl, (juce::Component*) lModel, (juce::Component*) lKey })
        c->setEnabled (! offline);
    effort.setEnabled (claude);
    serverFallback.setEnabled (claude);
    lEffort->setEnabled (claude);
    juce::String info;
    switch (s.provider)
    {
        case AISettings::Offline: info = "OFFLINE: the built-in RULE-BASED designer (fixed English vocabulary, not a language model). No network, account or model needed."; break;
        case AISettings::Anthropic: info = "CLOUD: Anthropic Messages API with structured JSON output. Requires an API key (billed by Anthropic)."; break;
        case AISettings::OpenAI: info = "CLOUD: OpenAI Chat Completions with JSON-schema structured output. Requires an API key (billed by OpenAI)."; break;
        case AISettings::Ollama: info = "LOCAL: Ollama native API (structured output via the format schema). Install Ollama and pull a model; no key needed."; break;
        case AISettings::LMStudio: info = "LOCAL: LM Studio server (OpenAI-compatible, json_schema output). Start the server in LM Studio and load a model."; break;
        default: info = "Any OpenAI-compatible server (/v1/chat/completions, /v1/models). Plain http only for this computer or the local network."; break;
    }
    modeInfo.setText (info, juce::dontSendNotification);
    loading = false;
    timerCallback();
}

void AISettingsPanel::rebuildModelList()
{
    const auto s = ai.getSettings();
    const auto current = s.model();
    model.clear (juce::dontSendNotification);
    int id = 1;
    if (ai.getModelsProvider() == s.provider)
        for (const auto& m : ai.getModels())
            model.addItem (m.id + (m.displayName.isNotEmpty() && m.displayName != m.id ? "  -  " + m.displayName : juce::String())
                                + (m.structuredOutputs == 0 ? "  (no structured output)" : juce::String()), id++);
    model.setText (current, juce::dontSendNotification);
}

void AISettingsPanel::storeSettings()
{
    auto s = ai.getSettings();
    s.provider = juce::jlimit (0, AISettings::NumProviders - 1, provider.getSelectedId() - 1);
    const auto url = baseUrl.getText().trim();
    setUrlFor (s, url.isNotEmpty() ? url : defaultUrlFor (s.provider));
    s.setModel (model.getText().upToFirstOccurrenceOf ("  ", false, false).trim());
    if (s.provider == AISettings::Anthropic && s.anthropicModel.isEmpty()) s.anthropicModel = AISettings().anthropicModel;
    s.effort = effort.getText().isNotEmpty() ? effort.getText() : juce::String ("medium");
    s.serverSideFallback = serverFallback.getToggleState();
    s.fallbackToOffline = offlineFallback.getToggleState();
    s.autoPreview = autoPreview.getToggleState();
    s.optimiseAfterGenerate = verify.getToggleState();
    s.shareReferenceAnalysis = shareRef.getToggleState();
    s.timeoutSeconds = (int) timeout.getValue();
    s.matchIterations = (int) matchIterations.getValue();
    s.agentSteps = (int) agentSteps.getValue();
    ai.setSettings (s);
}

void AISettingsPanel::timerCallback()
{
    const auto s = ai.getSettings();
    const bool busy = ai.isBusy();
    keyStatus.setText (s.provider == AISettings::Offline ? "No key required."
                       : ai.hasApiKey() ? "Key stored: " + ai.maskedKey()
                       : s.requiresApiKey() ? "No key stored (required)." : "No key stored (optional for local servers).",
                       juce::dontSendNotification);
    if (ai.getModelsStatus() != lastModelsStatus)
    {
        lastModelsStatus = ai.getModelsStatus();
        rebuildModelList();
        if (ai.getModelsProvider() == s.provider && s.model().isEmpty() && ! ai.getModels().empty() && s.provider != AISettings::OpenAI)
            model.setText ({}, juce::dontSendNotification);
    }
    modelsStatus.setText (ai.getModelsProvider() == s.provider ? ai.getModelsStatus() : juce::String(), juce::dontSendNotification);
    testResult.setText (busy ? ai.getStatus() : ai.getLastConnectionResult(), juce::dontSendNotification);
    if (ai.getLocalSetupReport() != lastLocalReport)
    {
        lastLocalReport = ai.getLocalSetupReport();
        localReport.setText (lastLocalReport, false);
        loadFromSettings(); // detection may have selected a provider and model
    }
    detectLocal.setEnabled (! busy);
    testResult.setColour (juce::Label::textColourId, ai.getLastConnectionOk() ? juce::Colour (0xff3ddc84) : Theme::sunsetOrange);
    for (juce::Component* c : { (juce::Component*) &refreshModels, (juce::Component*) &test, (juce::Component*) &provider })
        c->setEnabled (! busy && (c == &provider || s.provider != AISettings::Offline));
}

void AISettingsPanel::paint (juce::Graphics&) {}

void AISettingsPanel::resized()
{
    auto a = getLocalBounds();
    auto row = [&a] (int h = 26) { auto rr = a.removeFromTop (h); a.removeFromTop (5); return rr; };
    auto labelled = [] (juce::Rectangle<int> rr, juce::Label* l, juce::Component& c)
    {
        l->setBounds (rr.removeFromLeft (112));
        c.setBounds (rr);
    };
    labelled (row(), lProvider, provider);
    modeInfo.setBounds (row (30).withTrimmedLeft (112));
    {
        auto rr = row();
        resetUrl.setBounds (rr.removeFromRight (70));
        rr.removeFromRight (6);
        labelled (rr, lUrl, baseUrl);
    }
    {
        auto rr = row();
        refreshModels.setBounds (rr.removeFromRight (130));
        rr.removeFromRight (6);
        labelled (rr, lModel, model);
    }
    modelsStatus.setBounds (row (16).withTrimmedLeft (112));
    labelled (row(), lKey, apiKey);
    {
        auto rr = row().withTrimmedLeft (112);
        saveKey.setBounds (rr.removeFromLeft (96));
        rr.removeFromLeft (6);
        removeKey.setBounds (rr.removeFromLeft (110));
        rr.removeFromLeft (8);
        keyStatus.setBounds (rr);
    }
    security.setBounds (row (28).withTrimmedLeft (112));
    {
        auto rr = row();
        labelled (rr.removeFromLeft (rr.getWidth() / 2), lEffort, effort);
        rr.removeFromLeft (10);
        serverFallback.setBounds (rr);
    }
    shareRef.setBounds (row (22));
    offlineFallback.setBounds (row (22));
    verify.setBounds (row (22));
    autoPreview.setBounds (row (22));
    labelled (row(), lTimeout, timeout);
    labelled (row(), lMatch, matchIterations);
    labelled (row(), lAgent, agentSteps);
    {
        auto rr = row (30);
        test.setBounds (rr.removeFromLeft (170));
        rr.removeFromLeft (8);
        detectLocal.setBounds (rr.removeFromLeft (170));
    }
    testResult.setBounds (row (34));
    localReport.setBounds (a);
}

} // namespace vsx
