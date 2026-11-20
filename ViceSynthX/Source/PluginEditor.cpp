#include "PluginEditor.h"
#include <cmath>

namespace vsx
{
void ViceSynthEditor::Root::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack);
    // very subtle vignette so cards read as floating panels
    juce::ColourGradient glow (Theme::electricPurple.withAlpha (0.05f), (float) getWidth() * 0.5f, (float) getHeight() * 0.45f,
                               Theme::deepBlack.withAlpha (0.0f), 0.0f, 0.0f, true);
    g.setGradientFill (glow);
    g.fillAll();
}

// =================================================================================
ViceSynthEditor::ViceSynthEditor (ViceSynthProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (root);
    root.setSize (kDesignW, kDesignH);

    header = std::make_unique<MainHeader> (p);
    header->onTab = [this] (int v) { showView (v); };
    header->onSettings = [this] { showSettings(); };
    header->onHelp = [this] { showHelp(); };
    header->onScale = [this] (float s) { setScale (s); };
    browser = std::make_unique<BrowserPanel> (p, browserModel);
    hero = std::make_unique<PresetHero> (p);
    hero->compact = true;
    library = std::make_unique<LibraryView> (p, browserModel);
    studio = std::make_unique<StudioView> (p);
    design = std::make_unique<SoundDesignView> (p);
    design->onOpenWavetableEditor = [this] (int osc) { openWavetableEditor (osc); };
    design->onPageChanged = [this] (int page) { ui().setProperty ("designPage", page); };
    stage = std::make_unique<StageView> (p);
    aiPanel = std::make_unique<AIDesignerPanel> (p);
    aiPanel->onShowHistory = [this] { showHistory(); };
    aiPanel->onShowSettings = [this] { showSettings(); };
    macros = std::make_unique<MacroStrip> (p);
    performance = std::make_unique<PerformancePanel> (p);

    for (juce::Component* c : { (juce::Component*) header.get(), (juce::Component*) browser.get(), (juce::Component*) aiPanel.get(),
                                (juce::Component*) macros.get(), (juce::Component*) performance.get() })
        root.addAndMakeVisible (c);
    for (juce::Component* c : { (juce::Component*) hero.get(), (juce::Component*) library.get(), (juce::Component*) studio.get(),
                                (juce::Component*) design.get(), (juce::Component*) stage.get() })
        root.addChildComponent (c);

    settingsPage = std::make_unique<SettingsPage> (p);
    settingsPage->onScale = [this] (float s) { setScale (s); };
    settingsPage->onOpenGL = [this] (bool on) { setOpenGL (on); };
    history = std::make_unique<AIChatPanel> (p);
    help = std::make_unique<HelpView>();
    root.addChildComponent (overlay);

    browserModel.onChanged = [this]
    {
        browser->refresh();
        library->refresh();
    };
    browserModel.update (proc.getPresetManager());

    // wavetable / sample loads update the oscillator panels
    juce::Component::SafePointer<ViceSynthEditor> safe (this);
    proc.getPatch().onResourceChanged = [safe] (int osc)
    {
        if (safe != nullptr) safe->design->resourceChanged (osc);
    };

    float scale = 0.85f;
    int view = Studio, designPage = SoundDesignView::Matrix;
    bool gl = false;
    if (proc.uiState.isObject())
    {
        scale = (float) proc.uiState.getProperty ("scale", 0.85);
        view = juce::jlimit (0, 3, (int) proc.uiState.getProperty ("view", (int) Studio));
        designPage = (int) proc.uiState.getProperty ("designPage", designPage);
        gl = (bool) proc.uiState.getProperty ("openGL", false);
    }
    setResizable (true, true);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setScale (scale);
    layoutRoot();
    design->showPage (designPage);
    showView (view);
    if (gl) setOpenGL (true);
    startTimerHz (30);
}

ViceSynthEditor::~ViceSynthEditor()
{
    stopTimer();
    proc.getPatch().onResourceChanged = nullptr;
    browserModel.onChanged = nullptr;
    if (openGL.isAttached()) openGL.detach();
    overlay.hide();
    wavetableEditor.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::DynamicObject& ViceSynthEditor::ui()
{
    if (! proc.uiState.isObject()) proc.uiState = juce::var (new juce::DynamicObject());
    return *proc.uiState.getDynamicObject();
}

void ViceSynthEditor::setScale (float s)
{
    if (! std::isfinite (s)) s = 0.85f;
    s = juce::jlimit (0.5f, 2.0f, s);
    setSize (juce::roundToInt (kDesignW * s), juce::roundToInt (kDesignH * s));
}

void ViceSynthEditor::setOpenGL (bool enabled)
{
    if (enabled && ! openGL.isAttached()) openGL.attachTo (*this);
    if (! enabled && openGL.isAttached()) openGL.detach();
    settingsPage->openGLEnabled = enabled;
    ui().setProperty ("openGL", enabled);
}

void ViceSynthEditor::showView (int v)
{
    currentView = juce::jlimit (0, 3, v);
    hero->setVisible (currentView == Library || currentView == Studio);
    library->setVisible (currentView == Library);
    studio->setVisible (currentView == Studio);
    design->setVisible (currentView == SoundDesign);
    stage->setVisible (currentView == Stage);
    header->setActiveTab (currentView);
    if (currentView == Library) library->grabKeyboardFocus();
    ui().setProperty ("view", currentView);
}

void ViceSynthEditor::showSettings()
{
    overlay.show ("SETTINGS", settingsPage.get(), { 1180, 760 });
}

void ViceSynthEditor::showHelp()
{
    overlay.show ("QUICK HELP", help.get(), { 1000, 640 });
}

void ViceSynthEditor::showHistory()
{
    overlay.show ("AI CONVERSATION HISTORY", history.get(), { 760, 760 });
}

void ViceSynthEditor::openWavetableEditor (int osc)
{
    wavetableEditor = std::make_unique<WavetableEditor> (proc, osc);
    wavetableEditor->onClose = [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ViceSynthEditor> (this)]
        {
            if (safe != nullptr) safe->wavetableEditor.reset();
        });
    };
    root.addAndMakeVisible (*wavetableEditor);
    wavetableEditor->setBounds (root.getLocalBounds().reduced (40, 30));
}

void ViceSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack);
}

void ViceSynthEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    root.setTransform (juce::AffineTransform::scale (scale));
    if (header != nullptr) header->setScaleDisplay (scale);
    ui().setProperty ("scale", scale);
}

void ViceSynthEditor::layoutRoot()
{
    auto r = root.getLocalBounds();
    header->setBounds (r.removeFromTop (76));
    r = r.reduced (10, 0).withTrimmedTop (8).withTrimmedBottom (10);
    performance->setBounds (r.removeFromBottom (136));
    r.removeFromBottom (8);
    macros->setBounds (r.removeFromBottom (104));
    r.removeFromBottom (8);
    browser->setBounds (r.removeFromLeft (290));
    r.removeFromLeft (8);
    aiPanel->setBounds (r.removeFromRight (318));
    r.removeFromRight (8);
    // centre column
    design->setBounds (r);
    stage->setBounds (r);
    auto withHero = r;
    hero->setBounds (withHero.removeFromTop (150));
    withHero.removeFromTop (8);
    library->setBounds (withHero);
    studio->setBounds (withHero);
    overlay.setBounds (root.getLocalBounds());
    if (wavetableEditor != nullptr) wavetableEditor->setBounds (root.getLocalBounds().reduced (40, 30));
}

bool ViceSynthEditor::visibleInEditor (const juce::Component* c) const
{
    for (; c != nullptr && c != this; c = c->getParentComponent())
        if (! c->isVisible()) return false;
    return c == this;
}

void ViceSynthEditor::refresh (bool requireOnScreen)
{
    ++frame;
    // knobs: modulation rings and "changed by AI" markers (cleared once the value moves away)
    auto& ai = proc.getAI();
    if (ai.getAIChangeSerial() != aiSerialSeen)
    {
        aiSerialSeen = ai.getAIChangeSerial();
        aiChanged.clear();
        for (const auto& c : ai.getAIChanges()) aiChanged[c.first] = c.second;
    }
    const auto& reg = ParamRegistry::get();
    for (auto* k : ParamKnob::registry())
        if (requireOnScreen ? k->isShowing() : visibleInEditor (k))
        {
            k->refreshModulation();
            bool hl = false;
            if (auto it = aiChanged.find (k->getParamIndex()); it != aiChanged.end())
            {
                const auto& d = reg[it->first];
                hl = std::abs (proc.getParam (it->first) - it->second) <= 1.0e-4f * std::max (1.0f, d.maxValue - d.minValue);
            }
            k->setAIHighlight (hl);
        }

    switch (currentView)
    {
        case Studio: studio->refresh(); break;
        case SoundDesign: design->refresh(); break;
        case Stage: if (frame % 2 == 0) stage->refresh(); break;
        default: break;
    }
    if (frame % 3 == 0)
    {
        header->refresh();
        if (hero->isVisible()) hero->refresh();
    }
    const auto& name = proc.getCurrentMeta().name;
    if (name != lastPresetName)
    {
        lastPresetName = name;
        browser->refresh();
        library->refresh();
    }
}

} // namespace vsx
