#include "AIDesignerPanel.h"

namespace vsx
{
AIDesignerPanel::AIDesignerPanel (ViceSynthProcessor& p) : proc (p), ai (p.getAI())
{
    suggestions = { "A dreamy 80s pad with shimmering motion", "A deep, driving bass for a synthwave track", "A bright, punchy pluck with analog character",
                    "A cinematic, futuristic arpeggio with movement", "Create a warm 1990s digital pad", "A powerful Moog-style analog bass",
                    "A vintage JD-800-inspired atmospheric sound", "A cinematic evolving texture", "Make this sound darker and wider",
                    "Add analog warmth without losing clarity", "Make the attack softer and increase the stereo movement", "Create five variations of this sound",
                    "A squelchy acid bassline", "A glassy FM bell with a long tail", "A lush ensemble string pad", "Make this preset suitable for an early 1990s pop production" };

    prompt.setMultiLine (true, true);
    prompt.setReturnKeyStartsNewLine (false);
    prompt.setInputRestrictions (500);
    prompt.setFont (Theme::font (14.5f));
    prompt.setTextToShowWhenEmpty ("Describe the sound you want...", Theme::textDim);
    prompt.setColour (juce::TextEditor::backgroundColourId, Theme::display);
    prompt.onReturnKey = [this] { generate(); };
    prompt.onTextChange = [this] { counter.setText (juce::String (prompt.getText().length()) + " / 500", juce::dontSendNotification); };
    addAndMakeVisible (prompt);
    counter.setText ("0 / 500", juce::dontSendNotification);
    counter.setFont (Theme::font (11.0f));
    counter.setColour (juce::Label::textColourId, Theme::textDim);
    counter.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (counter);

    generateB.setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
    generateB.setToggleState (true, juce::dontSendNotification);
    generateB.onClick = [this] { generate(); };
    generateB.setTooltip ("Generate a playable preset from the prompt (Enter)");
    addAndMakeVisible (generateB);

    for (int i = 0; i < 4; ++i)
    {
        auto* b = suggestionButtons.add (new juce::TextButton());
        b->onClick = [this, b] { prompt.setText (b->getButtonText(), true); prompt.grabKeyboardFocus(); };
        b->setTooltip ("Use this prompt (then press GENERATE SOUND)");
        addAndMakeVisible (b);
    }
    refreshB.onClick = [this] { suggestionOffset = (suggestionOffset + 4) % suggestions.size(); showSuggestions(); };
    addAndMakeVisible (refreshB);
    showSuggestions();

    prevVar.onClick = [this] { stepVariation (-1); };
    nextVar.onClick = [this] { stepVariation (1); };
    varLabel.setJustificationType (juce::Justification::centred);
    varLabel.setFont (Theme::font (13.0f, true));
    for (auto* c : std::initializer_list<juce::Component*> { &prevVar, &nextVar, &varLabel }) addAndMakeVisible (c);

    response.setJustificationType (juce::Justification::topLeft);
    response.setFont (Theme::font (12.5f));
    response.setColour (juce::Label::textColourId, Theme::text);
    response.setMinimumHorizontalScale (1.0f);
    responseView.setViewedComponent (&response, false);
    responseView.setScrollBarsShown (true, false);
    responseView.setScrollBarThickness (5);
    addAndMakeVisible (responseView);
    status.setFont (Theme::font (11.0f, true));
    status.setColour (juce::Label::textColourId, Theme::cyberCyan);
    addAndMakeVisible (status);
    progress.setColour (juce::ProgressBar::foregroundColourId, Theme::neonPink);
    progress.setColour (juce::ProgressBar::backgroundColourId, Theme::display);
    addChildComponent (progress);

    for (auto* b : { &stopB, &previewB, &saveB, &refB, &matchB, &historyB }) { b->showBackground = true; addAndMakeVisible (b); }
    stopB.onClick = [this] { ai.cancel(); };
    previewB.onClick = [this] { proc.audition (60, 100, 1.6); };
    saveB.onClick = [this] { juce::String err; auto s = proc.getCurrentPatch(); s.meta.source = "ai"; ai.savePreset (s, err); };
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
    ai.removeUpdateListener (this);
}

void AIDesignerPanel::showSuggestions()
{
    for (int i = 0; i < suggestionButtons.size(); ++i)
        suggestionButtons[i]->setButtonText (suggestions[(suggestionOffset + i) % suggestions.size()]);
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

void AIDesignerPanel::conversationChanged()
{
    const auto& conv = ai.getConversation();
    // newest assistant / error message and the presets it produced
    for (auto it = conv.rbegin(); it != conv.rend(); ++it)
    {
        if (it->kind == ChatEntry::User) continue;
        juce::String text = it->text;
        if (it->engine.isNotEmpty()) text = it->text + "\n\n[" + it->engine + "]";
        response.setText (text, juce::dontSendNotification);
        response.setColour (juce::Label::textColourId, it->kind == ChatEntry::Error ? Theme::sunsetOrange : Theme::text);
        if (conv.size() != lastConversationSize && ! it->presets.empty())
        {
            variations = it->presets;
            variationIndex = 0;
        }
        break;
    }
    lastConversationSize = conv.size();
    const int w = std::max (60, responseView.getWidth() - 8);
    juce::AttributedString as;
    as.append (response.getText(), Theme::font (12.5f));
    as.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout tl;
    tl.createLayout (as, (float) w - 6.0f);
    response.setSize (w, (int) tl.getHeight() + 12);
    timerCallback();
}

void AIDesignerPanel::timerCallback()
{
    const bool busy = ai.isBusy();
    const auto s = ai.getSettings();
    const juce::String engine = s.provider == AISettings::Anthropic ? "Claude " + s.anthropicModel + (ai.hasApiKey() ? juce::String() : juce::String (" (no key: offline fallback)"))
                              : s.provider == AISettings::OpenAICompatible ? "LLM: " + s.openAiModel
                                                                           : juce::String ("Local rule-based designer (offline)");
    status.setText (busy ? ai.getStatus() : engine + (ai.hasReference() ? "  |  ref: " + ai.getReferenceName() : juce::String()), juce::dontSendNotification);
    progressValue = busy ? (ai.getProgress() > 0.0f ? (double) ai.getProgress() : -1.0) : 0.0;
    progress.setVisible (busy);
    generateB.setEnabled (! busy);
    stopB.setEnabled (busy);
    matchB.setEnabled (! busy && ai.hasReference());
    prevVar.setEnabled (variations.size() > 1);
    nextVar.setEnabled (variations.size() > 1);
    varLabel.setText (variations.empty() ? "- / -" : juce::String (variationIndex + 1) + " / " + juce::String ((int) variations.size()), juce::dontSendNotification);
}

void AIDesignerPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::card (g, r, 10.0f);
    Icons::draw (g, Icons::Type::Sparkle, juce::Rectangle<float> (16.0f, 16.0f, 40.0f, 40.0f), Theme::neonPink, 1.8f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (18.0f, true));
    g.drawText ("AI SOUND DESIGNER", juce::Rectangle<float> (66.0f, 14.0f, r.getWidth() - 70.0f, 26.0f), juce::Justification::left);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true).withExtraKerningFactor (0.14f));
    g.drawText (juce::String::fromUTF8 ("GENERATE  \xe2\x80\xa2  EXPLORE  \xe2\x80\xa2  REFINE"), juce::Rectangle<float> (66.0f, 40.0f, r.getWidth() - 70.0f, 16.0f), juce::Justification::left);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (12.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("SUGGESTED PROMPTS", juce::Rectangle<int> (16, refreshB.getY(), 220, refreshB.getHeight()), juce::Justification::centredLeft);
    auto vb = juce::Rectangle<float> (12.0f, (float) varLabel.getY() - 4.0f, r.getWidth() - 24.0f, (float) varLabel.getHeight() + 8.0f);
    Theme::displayWell (g, vb, 6.0f);
    Icons::draw (g, Icons::Type::Box, vb.withWidth (36.0f).reduced (9.0f), Theme::text, 1.4f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.5f, true));
    g.drawText ("AI VARIATIONS", vb.withTrimmedLeft (40.0f), juce::Justification::centredLeft);
    Theme::displayWell (g, responseView.getBounds().toFloat().expanded (4.0f), 6.0f);
}

void AIDesignerPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (56);
    auto pr = r.removeFromTop (118);
    prompt.setBounds (pr);
    counter.setBounds (pr.removeFromBottom (18).removeFromRight (80).translated (-6, -2));
    r.removeFromTop (10);
    generateB.setBounds (r.removeFromTop (42));
    r.removeFromTop (8);
    status.setBounds (r.removeFromTop (16));
    progress.setBounds (r.removeFromTop (5));
    r.removeFromTop (6);
    auto tools = r.removeFromBottom (30);
    const int tw = tools.getWidth() / 6;
    for (auto* b : { &stopB, &previewB, &saveB, &refB, &matchB, &historyB }) b->setBounds (tools.removeFromLeft (tw).reduced (3, 0));
    r.removeFromBottom (10);
    auto vrow = r.removeFromBottom (32).reduced (8, 0);
    nextVar.setBounds (vrow.removeFromRight (28));
    varLabel.setBounds (vrow.removeFromRight (56));
    prevVar.setBounds (vrow.removeFromRight (28));
    varLabel.setBounds (varLabel.getBounds());
    r.removeFromBottom (12);
    auto sugg = r.removeFromBottom (4 * 34 + 30);
    auto head = sugg.removeFromTop (26);
    refreshB.setBounds (head.removeFromRight (26));
    for (auto* b : suggestionButtons) b->setBounds (sugg.removeFromTop (34).reduced (0, 2));
    r.removeFromBottom (8);
    responseView.setBounds (r.reduced (4));
    conversationChanged();
}

} // namespace vsx
