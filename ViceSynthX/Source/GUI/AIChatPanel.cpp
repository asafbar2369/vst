#include "AIChatPanel.h"

namespace vsx
{
class AIChatPanel::Bubble : public juce::Component
{
public:
    Bubble (AIChatPanel& o, const ChatEntry& e) : owner (o), entry (e)
    {
        for (size_t i = 0; i < entry.presets.size(); ++i)
        {
            auto* load = buttons.add (new juce::TextButton (i == 0 && entry.presets.size() > 1 ? "Load main" : (entry.presets.size() > 1 ? "Load V" + juce::String ((int) i) : "Load")));
            const auto spec = entry.presets[i];
            load->onClick = [this, spec] { owner.ai.loadPreset (spec); };
            load->setTooltip ("Load \"" + spec.meta.name + "\"");
            addAndMakeVisible (load);
            auto* save = buttons.add (new juce::TextButton ("Save"));
            save->onClick = [this, spec] { juce::String err; owner.ai.savePreset (spec, err); };
            save->setTooltip ("Save \"" + spec.meta.name + "\" to the AI Generated library");
            addAndMakeVisible (save);
        }
    }

    int heightFor (int width)
    {
        layoutText (width);
        const int rows = (int) (entry.presets.size() + 1) / 2;
        return (int) std::ceil (textLayout.getHeight()) + 18 + (entry.engine.isNotEmpty() ? 12 : 0) + rows * 24;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        r.removeFromTop ((int) std::ceil (textLayout.getHeight()) + 4 + (entry.engine.isNotEmpty() ? 12 : 0));
        const int colW = r.getWidth() / 2;
        for (int i = 0; i < buttons.size(); i += 2)
        {
            const int row = (i / 2) / 2, col = (i / 2) % 2;
            auto cell = juce::Rectangle<int> (r.getX() + col * colW, r.getY() + row * 24, colW - 4, 22);
            buttons[i]->setBounds (cell.removeFromLeft (cell.getWidth() * 62 / 100));
            buttons[i + 1]->setBounds (cell.withTrimmedLeft (2));
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f, 2.0f);
        const bool user = entry.kind == ChatEntry::User;
        const auto c = user ? Theme::neonPink : entry.kind == ChatEntry::Error ? Theme::sunsetOrange
                     : entry.kind == ChatEntry::Info ? Theme::electricPurple : Theme::cyberCyan;
        g.setColour (c.withAlpha (user ? 0.22f : 0.12f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (c.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        layoutText (getWidth());
        textLayout.draw (g, r.reduced (8.0f, 6.0f).withHeight (textLayout.getHeight()));
        if (entry.engine.isNotEmpty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (9.5f));
            g.drawText (entry.engine, r.reduced (8.0f, 6.0f).withTrimmedTop (textLayout.getHeight() + 1.0f).withHeight (11.0f), juce::Justification::left);
        }
    }

private:
    void layoutText (int width)
    {
        if (width == laidOutWidth) return;
        laidOutWidth = width;
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        const juce::String prefix = entry.kind == ChatEntry::User ? "You: " : entry.kind == ChatEntry::Error ? "! " : "";
        s.append (prefix, Theme::font (12.5f, true), Theme::neonPink);
        s.append (entry.text, Theme::font (12.5f), Theme::text);
        textLayout.createLayout (s, (float) std::max (40, width - 20));
    }

    AIChatPanel& owner;
    ChatEntry entry;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::TextLayout textLayout;
    int laidOutWidth = -1;
};

void AIChatPanel::ChatList::layout (int width)
{
    int y = 0;
    for (auto* c : getChildren())
        if (auto* b = dynamic_cast<Bubble*> (c))
        {
            const int h = b->heightFor (width);
            b->setBounds (0, y, width, h);
            y += h + 6;
        }
    setSize (width, std::max (y, 10));
}

AIChatPanel::AIChatPanel (ViceSynthProcessor& p) : Section ("AI SOUND DESIGNER", Theme::cyberCyan), proc (p), ai (p.getAI())
{
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    input.setMultiLine (true, true);
    input.setReturnKeyStartsNewLine (false);
    input.setTextToShowWhenEmpty ("Describe a sound... e.g. \"Create a warm 1990s digital pad\" or \"Make it darker and wider\"", Theme::textDim);
    input.setFont (Theme::font (13.0f));
    input.onReturnKey = [this] { send(); };
    addAndMakeVisible (input);

    for (auto* b : { &sendButton, &stopButton, &undoButton, &redoButton, &previewButton, &saveButton, &refButton, &matchButton, &clearButton })
        addAndMakeVisible (b);
    sendButton.setColour (juce::TextButton::buttonColourId, Theme::neonPink.withAlpha (0.6f));
    sendButton.onClick = [this] { send(); };
    stopButton.onClick = [this] { ai.cancel(); };
    undoButton.onClick = [this] { ai.undo(); };
    redoButton.onClick = [this] { ai.redo(); };
    previewButton.onClick = [this] { proc.audition (60, 100, 1.6); };
    saveButton.onClick = [this] { juce::String err; auto spec = proc.getCurrentPatch(); spec.meta.source = "ai"; ai.savePreset (spec, err); };
    refButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load reference audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) ai.loadReference (fc.getResult());
        });
    };
    matchButton.onClick = [this] { ai.matchReference(); };
    clearButton.onClick = [this] { ai.clearConversation(); };
    sendButton.setTooltip ("Send (Enter). Shift+Enter for a new line.");
    undoButton.setTooltip ("Undo the last AI change (snapshot-based).");
    refButton.setTooltip ("Load an audio file to analyse and match.");
    matchButton.setTooltip ("Render candidate patches and optimise them towards the reference.");

    status.setFont (Theme::font (11.5f, true));
    status.setColour (juce::Label::textColourId, Theme::cyberCyan);
    addAndMakeVisible (status);
    progress.setColour (juce::ProgressBar::foregroundColourId, Theme::neonPink);
    progress.setColour (juce::ProgressBar::backgroundColourId, Theme::deepBlack);
    addAndMakeVisible (progress);

    juce::Component::SafePointer<AIChatPanel> safe (this);
    ai.addUpdateListener (this, [safe] { if (safe != nullptr) safe->rebuild(); });
    rebuild();
    startTimerHz (10);
}

AIChatPanel::~AIChatPanel()
{
    ai.removeUpdateListener (this);
}

void AIChatPanel::submitText (const juce::String& text) { ai.submit (text); }

void AIChatPanel::send()
{
    const auto t = input.getText().trim();
    if (t.isEmpty()) return;
    input.clear();
    ai.submit (t);
}

void AIChatPanel::rebuild()
{
    const auto& conv = ai.getConversation();
    list.removeAllChildren();
    bubbles.clear();
    for (const auto& e : conv)
    {
        auto* b = bubbles.add (new Bubble (*this, e));
        list.addAndMakeVisible (b);
    }
    list.layout (std::max (100, viewport.getWidth() - 10));
    if (conv.size() != lastCount)
        viewport.setViewPosition (0, std::max (0, list.getHeight() - viewport.getHeight()));
    lastCount = conv.size();
    timerCallback();
}

void AIChatPanel::timerCallback()
{
    const bool busy = ai.isBusy();
    auto s = ai.getSettings();
    const juce::String provider = s.provider == AISettings::Anthropic ? "Claude (" + s.anthropicModel + ")"
                                : s.provider == AISettings::OpenAICompatible ? "OpenAI-compatible (" + s.openAiModel + ")" : "Offline designer";
    status.setText ((busy ? ai.getStatus() : "Ready") + juce::String ("  |  ") + provider
                        + (ai.hasReference() ? "  |  ref: " + ai.getReferenceName() : juce::String()),
                    juce::dontSendNotification);
    progressValue = busy ? (ai.getProgress() > 0.0f ? (double) ai.getProgress() : -1.0) : 0.0;
    progress.setVisible (busy);
    sendButton.setEnabled (! busy);
    stopButton.setEnabled (busy);
    matchButton.setEnabled (! busy && ai.hasReference());
    undoButton.setEnabled (proc.canUndo());
    redoButton.setEnabled (proc.canRedo());
}

void AIChatPanel::resized()
{
    auto r = content();
    auto bottom = r.removeFromBottom (150);
    status.setBounds (bottom.removeFromTop (18));
    progress.setBounds (bottom.removeFromTop (6).reduced (0, 1));
    bottom.removeFromTop (4);
    auto row2 = bottom.removeFromBottom (24);
    auto row1 = bottom.removeFromBottom (24);
    bottom.removeFromBottom (4);
    auto inputArea = bottom;
    sendButton.setBounds (inputArea.removeFromRight (64).reduced (2, 0));
    input.setBounds (inputArea);
    layoutRow (row1, { &stopButton, &undoButton, &redoButton, &previewButton, &saveButton }, 3);
    layoutRow (row2.withTrimmedTop (2), { &refButton, &matchButton, &clearButton }, 3);
    viewport.setBounds (r.withTrimmedBottom (4));
    list.layout (std::max (100, viewport.getWidth() - 10));
}

void AIChatPanel::paint (juce::Graphics& g)
{
    Section::paint (g);
}

} // namespace vsx
