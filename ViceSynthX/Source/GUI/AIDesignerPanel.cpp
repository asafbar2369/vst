#include "AIDesignerPanel.h"

namespace vsx
{
AIDesignerPanel::AIDesignerPanel (ViceSynthProcessor& p) : proc (p), ai (p.getAI())
{
    suggestions = { "A dreamy 80s pad with shimmering motion", "A deep, driving bass for a synthwave track", "A bright, punchy pluck with analog character",
                    "A cinematic, futuristic arpeggio with movement", "Create a warm early-1990s digital pad", "A powerful Moog-style analog bass",
                    "A Roland D-50-inspired atmospheric texture using original synthesis", "A vintage FM electric piano",
                    "Make this sound warmer, wider and less harsh", "Add analog warmth without losing clarity",
                    "Make the attack softer and increase the stereo movement", "Generate five variations of this preset",
                    "A squelchy acid bassline", "A glassy FM bell with a long tail", "A lush ensemble string pad",
                    "A vintage pop synth brass stab inspired by 1980s productions",
                    juce::String::fromUTF8 ("\xd7\xa6\xd7\x95\xd7\xa8 \xd7\xa4\xd7\x93 \xd7\x90\xd7\xa0\xd7\x9c\xd7\x95\xd7\x92\xd7\x99 \xd7\x97\xd7\x9d \xd7\xa9\xd7\x9c \xd7\xa9\xd7\xa0\xd7\x95\xd7\xaa \xd7\x94-80") };

    for (int i = 0; i < AISettings::NumProviders; ++i) providerBox.addItem (AISettings::providerName (i), i + 1);
    providerBox.setTooltip ("AI engine. OFFLINE: built-in rule-based designer (no network). LOCAL: Ollama / LM Studio on this computer or network. "
                            "CLOUD: Anthropic Claude / OpenAI (API key in Settings). Models and keys: main header Settings.");
    providerBox.onChange = [this]
    {
        auto s = ai.getSettings();
        s.provider = juce::jlimit (0, AISettings::NumProviders - 1, providerBox.getSelectedId() - 1);
        ai.setSettings (s);
        timerCallback();
        repaint();
    };
    addAndMakeVisible (providerBox);

    prompt.setMultiLine (true, true);
    prompt.setReturnKeyStartsNewLine (false);
    prompt.setInputRestrictions (500);
    prompt.setFont (Theme::font (14.5f));
    prompt.setTextToShowWhenEmpty ("Describe the sound you want (English)...", Theme::textDim);
    prompt.setColour (juce::TextEditor::backgroundColourId, Theme::display);
    prompt.onReturnKey = [this] { generate(); };
    prompt.onTextChange = [this]
    {
        counter.setText (juce::String (prompt.getText().length()) + " / 500", juce::dontSendNotification);
        updatePromptDirection();
    };
    addAndMakeVisible (prompt);
    counter.setText ("0 / 500", juce::dontSendNotification);
    counter.setFont (Theme::font (11.0f));
    counter.setColour (juce::Label::textColourId, Theme::textDim);
    counter.setJustificationType (juce::Justification::centredRight);
    counter.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (counter);

    generateB.setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
    generateB.setToggleState (true, juce::dontSendNotification);
    generateB.onClick = [this] { generate(); };
    generateB.setTooltip ("Generate a playable preset from the prompt (Enter)");
    addAndMakeVisible (generateB);

    for (int i = 0; i < kVisibleSuggestions; ++i)
    {
        auto* b = suggestionButtons.add (new juce::TextButton());
        b->onClick = [this, b] { prompt.setText (b->getButtonText(), true); prompt.grabKeyboardFocus(); };
        b->setTooltip ("Use this prompt (edit it if you like, then press GENERATE SOUND)");
        addAndMakeVisible (b);
    }
    refreshB.onClick = [this] { suggestionOffset = (suggestionOffset + kVisibleSuggestions) % suggestions.size(); showSuggestions(); };
    addAndMakeVisible (refreshB);
    showSuggestions();

    prevVar.onClick = [this] { stepVariation (-1); };
    nextVar.onClick = [this] { stepVariation (1); };
    loadVarB.onClick = [this]
    {
        if (juce::isPositiveAndBelow (variationIndex, (int) variations.size())) ai.loadPreset (variations[(size_t) variationIndex]);
    };
    loadVarB.setTooltip ("Load the selected result / variation (undoable)");
    varLabel.setJustificationType (juce::Justification::centred);
    varLabel.setFont (Theme::font (13.0f, true));
    for (auto* c : std::initializer_list<juce::Component*> { &prevVar, &nextVar, &varLabel, &loadVarB }) addAndMakeVisible (c);

    response.setMultiLine (true, true);
    response.setReadOnly (true);
    response.setCaretVisible (false);
    response.setScrollbarsShown (true);
    response.setScrollBarThickness (6);
    response.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    response.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    response.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (response);
    status.setFont (Theme::font (11.0f, true));
    status.setColour (juce::Label::textColourId, Theme::cyberCyan);
    status.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (status);
    progress.setColour (juce::ProgressBar::foregroundColourId, Theme::neonPink);
    progress.setColour (juce::ProgressBar::backgroundColourId, Theme::display);
    addChildComponent (progress);

    compareB.setClickingTogglesState (false);
    compareB.setColour (juce::TextButton::buttonOnColourId, Theme::sunsetOrange);
    compareB.setTooltip ("A/B: switch between the sound before and after the last AI change (no undo steps are created)");
    compareB.onClick = [this] { ai.toggleCompare(); timerCallback(); };
    addAndMakeVisible (compareB);
    for (auto* b : { &stopB, &previewB, &saveB, &refB, &matchB, &historyB }) { b->showBackground = true; addAndMakeVisible (b); }
    stopB.onClick = [this] { ai.cancel(); };
    previewB.onClick = [this] { proc.audition (60, 100, 1.6); };
    saveB.onClick = [this] { saveWithName(); };
    refB.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load reference audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) ai.loadReference (fc.getResult());
        });
    };
    matchB.onClick = [this] { ai.matchReference(); };
    historyB.onClick = [this] { if (onShowHistory) onShowHistory(); };

    juce::Component::SafePointer<AIDesignerPanel> safe (this);
    ai.addUpdateListener (this, [safe] { if (safe != nullptr) safe->conversationChanged(); });
    conversationChanged();
    startTimerHz (10);
}

AIDesignerPanel::~AIDesignerPanel()
{
    stopTimer();
    ai.removeUpdateListener (this);
}

void AIDesignerPanel::showSuggestions()
{
    for (int i = 0; i < suggestionButtons.size(); ++i)
        suggestionButtons[i]->setButtonText (suggestions[(suggestionOffset + i) % suggestions.size()]);
}

void AIDesignerPanel::updatePromptDirection()
{
    const auto j = juce::Justification::topLeft;
    if (prompt.getJustificationType() != j) prompt.setJustification (j);
}

void AIDesignerPanel::generate()
{
    const auto t = prompt.getText().trim();
    if (t.isEmpty() || ai.isBusy()) return;
    ai.submit (t);
}

void AIDesignerPanel::stepVariation (int d)
{
    if (variations.empty()) return;
    variationIndex = (variationIndex + d + (int) variations.size()) % (int) variations.size();
    ai.loadPreset (variations[(size_t) variationIndex]);
    timerCallback();
}

void AIDesignerPanel::saveWithName()
{
    auto* w = new juce::AlertWindow ("Save sound", "Name for the preset (saved to the AI Generated folder):", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.getCurrentMeta().name.isNotEmpty() ? proc.getCurrentMeta().name : juce::String ("AI Sound"));
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<AIDesignerPanel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim()).substring (0, 60);
        if (name.isEmpty()) name = "AI Sound";
        auto spec = safe->proc.getCurrentPatch();
        spec.meta.name = name;
        spec.meta.source = "ai";
        juce::String err;
        if (safe->ai.savePreset (spec, err).existsAsFile())
            safe->proc.setCurrentMeta (spec.meta); // same sound, new name
    }), true);
}

bool AIDesignerPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (AIController::isSupportedAudioFile (juce::File (f))) return true;
    return false;
}

void AIDesignerPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    for (const auto& f : files)
        if (AIController::isSupportedAudioFile (juce::File (f))) { ai.loadReference (juce::File (f)); break; }
}

void AIDesignerPanel::conversationChanged()
{
    const auto& conv = ai.getConversation();
    for (auto it = conv.rbegin(); it != conv.rend(); ++it)
    {
        if (it->kind == ChatEntry::User) continue;
        response.clear();
        response.setJustification (juce::Justification::topLeft);
        response.setFont (Theme::font (12.5f));
        response.setColour (juce::TextEditor::textColourId, it->kind == ChatEntry::Error ? Theme::sunsetOrange : Theme::text);
        response.insertTextAtCaret (it->text);
        if (it->measurement.isNotEmpty())
        {
            response.setColour (juce::TextEditor::textColourId, Theme::cyberCyan);
            response.setFont (Theme::font (11.5f, true));
            response.insertTextAtCaret ("\n\nMEASURED: " + it->measurement);
        }
        if (it->engine.isNotEmpty())
        {
            response.setColour (juce::TextEditor::textColourId, Theme::textDim);
            response.setFont (Theme::font (10.5f));
            response.insertTextAtCaret ("\n" + it->engine);
        }
        response.moveCaretToTop (false);
        if (conv.size() != lastConversationSize && ! it->presets.empty())
        {
            variations = it->presets;
            variationIndex = 0;
        }
        break;
    }
    lastConversationSize = conv.size();
    timerCallback();
}

void AIDesignerPanel::timerCallback()
{
    const bool busy = ai.isBusy();
    const auto s = ai.getSettings();
    if (providerBox.getSelectedId() != s.provider + 1) providerBox.setSelectedId (s.provider + 1, juce::dontSendNotification);
    status.setText (busy ? ai.getStatus() : ai.engineLabel() + (ai.hasReference() ? "  |  ref: " + ai.getReferenceName() : juce::String()),
                    juce::dontSendNotification);
    progressValue = busy ? (ai.getProgress() > 0.0f ? (double) ai.getProgress() : -1.0) : 0.0;
    progress.setVisible (busy);
    generateB.setEnabled (! busy);
    stopB.setEnabled (busy);
    providerBox.setEnabled (! busy);
    matchB.setEnabled (! busy && ai.hasReference());
    refB.setEnabled (! busy);
    const bool cmp = ! busy && ai.canCompare();
    compareB.setEnabled (cmp);
    compareB.setToggleState (cmp && ai.isComparingBefore(), juce::dontSendNotification);
    compareB.setButtonText (cmp && ai.isComparingBefore() ? "A (before)" : "A/B");
    prevVar.setEnabled (variations.size() > 1);
    nextVar.setEnabled (variations.size() > 1);
    loadVarB.setEnabled (! variations.empty() && ! busy);
    varLabel.setText (variations.empty() ? "- / -" : juce::String (variationIndex + 1) + " / " + juce::String ((int) variations.size()), juce::dontSendNotification);
    repaint (badgeArea);
}

void AIDesignerPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::card (g, r, 10.0f);
    Icons::draw (g, Icons::Type::Sparkle, juce::Rectangle<float> (16.0f, 14.0f, 36.0f, 36.0f), Theme::neonPink, 1.8f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (18.0f, true));
    g.drawText ("AI SOUND DESIGNER", juce::Rectangle<float> (60.0f, 12.0f, r.getWidth() - 100.0f, 24.0f), juce::Justification::left);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true).withExtraKerningFactor (0.14f));
    g.drawText (juce::String::fromUTF8 ("GENERATE  \xe2\x80\xa2  EXPLORE  \xe2\x80\xa2  REFINE"), juce::Rectangle<float> (60.0f, 36.0f, r.getWidth() - 100.0f, 14.0f), juce::Justification::left);

    // mode badge
    const auto s = ai.getSettings();
    const auto mode = s.modeLabel();
    const auto c = s.provider == AISettings::Offline ? Theme::sunsetOrange : s.isCloud() ? Theme::electricPurple : Theme::cyberCyan;
    auto b = badgeArea.toFloat();
    g.setColour (c.withAlpha (0.18f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (c);
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);
    g.setFont (Theme::font (10.5f, true));
    g.drawText (mode, b, juce::Justification::centred);
    // connection indicator (last real test for this provider)
    const auto last = ai.getLastConnectionResult();
    if (s.provider != AISettings::Offline && last.isNotEmpty())
    {
        g.setColour (ai.getLastConnectionOk() ? juce::Colour (0xff3ddc84) : Theme::sunsetOrange);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ b.getRight() - 6.0f, b.getY() + 5.0f }));
    }

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (12.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("SUGGESTED PROMPTS", juce::Rectangle<int> (16, refreshB.getY(), 220, refreshB.getHeight()), juce::Justification::centredLeft);
    auto vb = juce::Rectangle<float> (12.0f, (float) varLabel.getY() - 3.0f, r.getWidth() - 24.0f, (float) varLabel.getHeight() + 6.0f);
    Theme::displayWell (g, vb, 6.0f);
    Icons::draw (g, Icons::Type::Box, vb.withWidth (32.0f).reduced (8.0f), Theme::text, 1.4f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("AI RESULTS", vb.withTrimmedLeft (34.0f), juce::Justification::centredLeft);
    Theme::displayWell (g, response.getBounds().toFloat().expanded (4.0f), 6.0f);
}

void AIDesignerPanel::paintOverChildren (juce::Graphics& g)
{
    if (! dragOver) return;
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (Theme::deepBlack.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Theme::cyberCyan);
    g.drawRoundedRectangle (r, 10.0f, 2.0f);
    g.setFont (Theme::font (16.0f, true));
    g.drawFittedText ("Drop reference audio\n(WAV / AIFF / FLAC)\n\nAnalysed locally - never uploaded", r.toNearestInt(), juce::Justification::centred, 4);
}

void AIDesignerPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto head = r.removeFromTop (50);
    r.removeFromTop (4);
    auto prow = r.removeFromTop (26);
    badgeArea = prow.removeFromLeft (64);
    prow.removeFromLeft (6);
    providerBox.setBounds (prow);
    r.removeFromTop (8);
    auto pr = r.removeFromTop (92);
    prompt.setBounds (pr);
    counter.setBounds (pr.removeFromBottom (18).removeFromRight (80).translated (-6, -2));
    r.removeFromTop (8);
    generateB.setBounds (r.removeFromTop (38));
    r.removeFromTop (6);
    status.setBounds (r.removeFromTop (16));
    progress.setBounds (r.removeFromTop (5));
    r.removeFromTop (6);

    auto tools = r.removeFromBottom (30);
    const int tw = tools.getWidth() / 7;
    for (juce::Component* b : { (juce::Component*) &stopB, (juce::Component*) &previewB, (juce::Component*) &compareB, (juce::Component*) &saveB,
                                (juce::Component*) &refB, (juce::Component*) &matchB, (juce::Component*) &historyB })
        b->setBounds (tools.removeFromLeft (tw).reduced (2, 0));
    r.removeFromBottom (10);
    auto vrow = r.removeFromBottom (28).reduced (8, 0);
    loadVarB.setBounds (vrow.removeFromRight (52).reduced (0, 2));
    vrow.removeFromRight (4);
    nextVar.setBounds (vrow.removeFromRight (26));
    varLabel.setBounds (vrow.removeFromRight (48));
    prevVar.setBounds (vrow.removeFromRight (26));
    r.removeFromBottom (10);
    auto sugg = r.removeFromBottom (kVisibleSuggestions * 32 + 26);
    auto shead = sugg.removeFromTop (26);
    refreshB.setBounds (shead.removeFromRight (26));
    for (auto* b : suggestionButtons) b->setBounds (sugg.removeFromTop (32).reduced (0, 2));
    r.removeFromBottom (8);
    response.setBounds (r.reduced (4));
}

} // namespace vsx
