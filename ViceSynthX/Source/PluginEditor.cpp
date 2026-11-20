#include "PluginEditor.h"
#include "AI/PatchOps.h"

namespace vsx
{
using namespace pid;

// =================================================================================
class ViceSynthEditor::Header : public juce::Component
{
public:
    explicit Header (ViceSynthProcessor& p, ViceSynthEditor& e) : proc (p), editor (e), quality (p, G (global::Quality)), meter (p)
    {
        addAndMakeVisible (art);
        art.setInterceptsMouseClicks (false, false);
        presetName.setJustificationType (juce::Justification::centred);
        presetName.setFont (Theme::displayFont (20.0f));
        presetName.setColour (juce::Label::textColourId, juce::Colours::white);
        presetName.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (presetName);
        presetInfo.setJustificationType (juce::Justification::centred);
        presetInfo.setFont (Theme::font (11.5f));
        presetInfo.setColour (juce::Label::textColourId, Theme::cyberCyan);
        presetInfo.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (presetInfo);
        for (auto* b : { &prev, &next, &save, &fav, &menu, &undo, &redo }) addAndMakeVisible (b);
        prev.onClick = [this] { proc.stepPreset (-1); };
        next.onClick = [this] { proc.stepPreset (1); };
        save.onClick = [this] { if (onSave) onSave(); };
        fav.onClick = [this]
        {
            auto& pm = proc.getPresetManager();
            const int i = pm.indexOfName (proc.getCurrentMeta().name);
            if (i >= 0) { const auto e2 = pm.getEntries()[(size_t) i]; pm.setFavorite (e2, ! e2.favorite); }
        };
        undo.onClick = [this] { proc.undo(); };
        redo.onClick = [this] { proc.redo(); };
        menu.onClick = [this] { showMenu(); };
        prev.setTooltip ("Previous preset"); next.setTooltip ("Next preset"); save.setTooltip ("Save preset");
        fav.setTooltip ("Toggle favourite"); undo.setTooltip ("Undo (preset loads and AI changes)"); redo.setTooltip ("Redo");
        master = std::make_unique<ParamKnob> (p, G (global::MasterVolume), "MASTER", Theme::neonPink);
        addAndMakeVisible (*master);
        addAndMakeVisible (quality);
        addAndMakeVisible (meter);
        cpu.setFont (Theme::font (11.0f, true));
        cpu.setColour (juce::Label::textColourId, Theme::textDim);
        addAndMakeVisible (cpu);
    }

    void paint (juce::Graphics&) override {}
    void paintOverChildren (juce::Graphics& g) override
    {
        MiamiArtwork::drawLogo (g, juce::Rectangle<float> (150.0f, 16.0f, 360.0f, 56.0f));
        auto pb = presetBox.toFloat();
        g.setColour (Theme::deepBlack.withAlpha (0.55f));
        g.fillRoundedRectangle (pb, 10.0f);
        g.setColour (Theme::neonPink.withAlpha (0.7f));
        g.drawRoundedRectangle (pb, 10.0f, 1.2f);
    }

    void resized() override
    {
        art.setBounds (getLocalBounds());
        auto r = getLocalBounds();
        presetBox = juce::Rectangle<int> (0, 0, 470, 62).withCentre ({ r.getCentreX() + 30, r.getCentreY() });
        auto pb = presetBox.reduced (8, 6);
        prev.setBounds (pb.removeFromLeft (30).withSizeKeepingCentre (30, 30));
        next.setBounds (pb.removeFromRight (30).withSizeKeepingCentre (30, 30));
        presetName.setBounds (pb.removeFromTop (30));
        presetInfo.setBounds (pb);
        auto tools = juce::Rectangle<int> (presetBox.getRight() + 10, presetBox.getY() + 14, 300, 34);
        for (auto* b : { &save, &fav, &undo, &redo, &menu }) { b->setBounds (tools.removeFromLeft (56).reduced (2, 0)); }
        auto right = r.removeFromRight (360).reduced (10, 8);
        meter.setBounds (right.removeFromRight (16).reduced (0, 6));
        right.removeFromRight (6);
        master->setBounds (right.removeFromRight (70));
        auto col = right.removeFromRight (150);
        quality.setBounds (col.removeFromTop (col.getHeight() / 2).reduced (0, 8));
        cpu.setBounds (col);
    }

    void refresh()
    {
        const auto& m = proc.getCurrentMeta();
        if (presetName.getText() != m.name) presetName.setText (m.name, juce::dontSendNotification);
        const int i = proc.getPresetManager().indexOfName (m.name);
        const bool isFav = i >= 0 && proc.getPresetManager().getEntries()[(size_t) i].favorite;
        fav.setButtonText (isFav ? juce::String::fromUTF8 ("\xe2\x98\x85") : juce::String::fromUTF8 ("\xe2\x98\x86"));
        presetInfo.setText (m.category + (m.source.isNotEmpty() ? "  |  " + m.source.toUpperCase() : juce::String())
                                + (m.author.isNotEmpty() ? "  |  " + m.author : juce::String()), juce::dontSendNotification);
        cpu.setText ("CPU " + juce::String ((int) (proc.cpuLoad.load() * 100.0f)) + "%   voices " + juce::String (proc.getEngine().activeVoiceCount.load()),
                     juce::dontSendNotification);
        undo.setEnabled (proc.canUndo());
        redo.setEnabled (proc.canRedo());
        meter.repaint();
    }

    std::function<void()> onSave;

private:
    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Initialise patch");
        m.addItem (2, "Load preset file...");
        m.addItem (3, "Save preset file as...");
        m.addSeparator();
        m.addItem (4, "Undo  " + proc.getUndoLabel(), proc.canUndo());
        m.addItem (5, "Redo", proc.canRedo());
        m.addItem (6, "Restore last session (crash recovery)");
        m.addSeparator();
        m.addItem (7, "Settings");
        m.addItem (8, "About VICE SYNTH X");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menu), [this] (int r)
        {
            if (r == 1) { auto s = PatchOps::initPatch(); s.meta.name = "Init"; proc.applyPatch (s, "Init"); }
            else if (r == 2 || r == 3)
            {
                chooser = std::make_unique<juce::FileChooser> (r == 2 ? "Load preset" : "Save preset", PresetManager::userFolder(), "*" + PresetFormat::kExtension);
                const int flags = r == 2 ? juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                         : juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
                chooser->launchAsync (flags, [this, r] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f == juce::File()) return;
                    juce::String err;
                    if (r == 2) { PatchSpec s; if (proc.getPresetManager().loadFile (f, s, err)) proc.applyPatch (s, "Load " + s.meta.name); }
                    else { auto s = proc.getCurrentPatch(); s.meta.name = f.getFileNameWithoutExtension(); PresetManager::writeAtomically (f.withFileExtension (PresetFormat::kExtension), s.toJson(), err); }
                });
            }
            else if (r == 4) proc.undo();
            else if (r == 5) proc.redo();
            else if (r == 6) { PatchSpec s; if (proc.getPresetManager().readRecovery (s)) proc.applyPatch (s, "Restore session"); }
            else if (r == 7) editor.showTab (8);
            else if (r == 8)
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "VICE SYNTH X",
                    juce::String ("Version ") + VSX_VERSION_STRING + "\nHybrid wavetable / analog-modelled synthesizer with an AI sound designer.\n"
                    "All artwork, wavetables, samples and presets are original content.");
        });
    }

    ViceSynthProcessor& proc;
    ViceSynthEditor& editor;
    MiamiHeader art;
    juce::Label presetName, presetInfo, cpu;
    juce::TextButton prev { juce::String::fromUTF8 ("\xe2\x97\x80") }, next { juce::String::fromUTF8 ("\xe2\x96\xb6") }, save { "SAVE" },
        fav { juce::String::fromUTF8 ("\xe2\x98\x86") }, menu { "MENU" }, undo { "UNDO" }, redo { "REDO" };
    std::unique_ptr<ParamKnob> master;
    ParamCombo quality;
    LevelMeter meter;
    juce::Rectangle<int> presetBox;
    std::unique_ptr<juce::FileChooser> chooser;
};

void ViceSynthEditor::Root::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (Theme::darkPurple, 0.0f, 0.0f, Theme::deepBlack, 0.0f, (float) getHeight(), false);
    bg.addColour (0.4, Theme::deepBlack.interpolatedWith (Theme::electricPurple, 0.08f));
    g.setGradientFill (bg);
    g.fillAll();
    // subtle perspective grid at the bottom (synthwave floor)
    g.setColour (Theme::electricPurple.withAlpha (0.06f));
    const float horizon = (float) getHeight() * 0.55f;
    for (int i = -20; i <= 20; ++i)
        g.drawLine ((float) getWidth() * 0.5f, horizon, (float) getWidth() * 0.5f + (float) i * 120.0f, (float) getHeight(), 1.0f);
}

// =================================================================================
ViceSynthEditor::ViceSynthEditor (ViceSynthProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (root);
    root.setSize (kDesignW, kDesignH);

    header = std::make_unique<Header> (p, *this);
    root.addAndMakeVisible (*header);

    const juce::StringArray tabs { "SYNTH", "FX", "MATRIX", "LFO", "ENV", "ARP", "SEQ", "MIXER", "SETTINGS" };
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto* b = tabButtons.add (new juce::TextButton (tabs[i]));
        b->setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
        b->onClick = [this, i] { showTab (i); };
        root.addAndMakeVisible (b);
    }

    browser = std::make_unique<PresetBrowser> (p);
    chat = std::make_unique<AIChatPanel> (p);
    bottom = std::make_unique<BottomBar> (p);
    root.addAndMakeVisible (*browser);
    root.addAndMakeVisible (*chat);
    root.addAndMakeVisible (*bottom);
    header->onSave = [this] { browser->saveAs(); };

    for (int o = 0; o < kNumOscs; ++o)
    {
        oscPanels[(size_t) o] = std::make_unique<OscPanel> (p, o);
        oscPanels[(size_t) o]->onOpenWavetableEditor = [this] (int osc) { openWavetableEditor (osc); };
        synthPage.addAndMakeVisible (*oscPanels[(size_t) o]);
    }
    subNoise = std::make_unique<SubNoisePanel> (p);
    synthPage.addAndMakeVisible (*subNoise);
    for (int f = 0; f < kNumFilters; ++f)
    {
        filterPanels[(size_t) f] = std::make_unique<FilterPanel> (p, f);
        synthPage.addAndMakeVisible (*filterPanels[(size_t) f]);
    }
    modSection = std::make_unique<ModulationSection> (p);
    synthPage.addAndMakeVisible (*modSection);

    fxPage = std::make_unique<FxPage> (p);
    matrixPage = std::make_unique<MatrixPage> (p);
    lfoPage = std::make_unique<LfoPage> (p);
    envPage = std::make_unique<EnvPage> (p);
    arpPage = std::make_unique<ArpPage> (p);
    seqPage = std::make_unique<SeqPage> (p);
    mixerPage = std::make_unique<MixerPage> (p);
    settingsPage = std::make_unique<SettingsPage> (p);
    settingsPage->onScale = [this] (float s) { setScale (s); };
    settingsPage->onOpenGL = [this] (bool on) { setOpenGL (on); };
    pages = { &synthPage, fxPage.get(), matrixPage.get(), lfoPage.get(), envPage.get(), arpPage.get(), seqPage.get(), mixerPage.get(), settingsPage.get() };
    for (auto* pg : pages) root.addChildComponent (pg);

    // resource loads (wavetables/samples) update the oscillator panels
    juce::Component::SafePointer<ViceSynthEditor> safe (this);
    proc.getPatch().onResourceChanged = [safe] (int osc)
    {
        if (safe != nullptr && juce::isPositiveAndBelow (osc, kNumOscs)) safe->oscPanels[(size_t) osc]->resourceChanged();
    };

    float scale = 0.8f;
    int tab = 0;
    bool gl = false;
    if (proc.uiState.isObject())
    {
        scale = juce::jlimit (0.5f, 2.0f, (float) proc.uiState.getProperty ("scale", 0.8));
        tab = (int) proc.uiState.getProperty ("tab", 0);
        gl = (bool) proc.uiState.getProperty ("openGL", false);
    }
    setResizable (true, true);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize ((int) (kDesignW * scale), (int) (kDesignH * scale));
    layoutRoot();
    showTab (tab);
    if (gl) setOpenGL (true);
    startTimerHz (30);
}

ViceSynthEditor::~ViceSynthEditor()
{
    stopTimer();
    proc.getPatch().onResourceChanged = nullptr;
    if (openGL.isAttached()) openGL.detach();
    wavetableEditor.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ViceSynthEditor::setScale (float s)
{
    setSize ((int) (kDesignW * s), (int) (kDesignH * s));
}

void ViceSynthEditor::setOpenGL (bool enabled)
{
    if (enabled && ! openGL.isAttached()) openGL.attachTo (*this);
    if (! enabled && openGL.isAttached()) openGL.detach();
    settingsPage->openGLEnabled = enabled;
    if (! proc.uiState.isObject()) proc.uiState = juce::var (new juce::DynamicObject());
    proc.uiState.getDynamicObject()->setProperty ("openGL", enabled);
}

void ViceSynthEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, (int) pages.size() - 1, index);
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == currentTab);
        tabButtons[i]->setToggleState (i == currentTab, juce::dontSendNotification);
    }
    if (! proc.uiState.isObject()) proc.uiState = juce::var (new juce::DynamicObject());
    proc.uiState.getDynamicObject()->setProperty ("tab", currentTab);
}

void ViceSynthEditor::openWavetableEditor (int osc)
{
    wavetableEditor = std::make_unique<WavetableEditor> (proc, osc);
    wavetableEditor->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ViceSynthEditor> (this)] { if (safe != nullptr) safe->wavetableEditor.reset(); }); };
    root.addAndMakeVisible (*wavetableEditor);
    wavetableEditor->setBounds (root.getLocalBounds());
}

void ViceSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack);
}

void ViceSynthEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    root.setTransform (juce::AffineTransform::scale (scale));
    if (! proc.uiState.isObject()) proc.uiState = juce::var (new juce::DynamicObject());
    proc.uiState.getDynamicObject()->setProperty ("scale", scale);
}

void ViceSynthEditor::layoutRoot()
{
    auto r = root.getLocalBounds();
    header->setBounds (r.removeFromTop (92));
    auto tabRow = r.removeFromTop (34).reduced (262, 4);
    tabRow.removeFromRight (370);
    const int tw = tabRow.getWidth() / tabButtons.size();
    for (auto* b : tabButtons) b->setBounds (tabRow.removeFromLeft (tw).reduced (3, 0));
    bottom->setBounds (r.removeFromBottom (150).reduced (6, 4));
    browser->setBounds (r.removeFromLeft (256).reduced (6, 4));
    chat->setBounds (r.removeFromRight (380).reduced (6, 4));
    auto centre = r.reduced (2, 4);
    for (auto* pg : pages) pg->setBounds (centre);

    auto s = synthPage.getLocalBounds();
    auto oscRow = s.removeFromTop (354);
    const int ow = oscRow.getWidth() / 3;
    for (int o = 0; o < kNumOscs; ++o) oscPanels[(size_t) o]->setBounds (oscRow.removeFromLeft (ow).reduced (3));
    subNoise->setBounds (s.removeFromTop (84).reduced (3));
    auto filterRow = s.removeFromTop (180);
    filterPanels[0]->setBounds (filterRow.removeFromLeft (filterRow.getWidth() / 2).reduced (3));
    filterPanels[1]->setBounds (filterRow.reduced (3));
    modSection->setBounds (s.reduced (3));
    if (wavetableEditor != nullptr) wavetableEditor->setBounds (root.getLocalBounds());
}

void ViceSynthEditor::timerCallback()
{
    ++frame;
    for (auto* k : ParamKnob::registry())
        if (k->isShowing()) k->refreshModulation();
    if (currentTab == 0)
    {
        for (auto& o : oscPanels) o->refresh();
        for (auto& f : filterPanels) f->refresh();
        modSection->refresh();
    }
    else if (currentTab == 3) lfoPage->refresh();
    else if (currentTab == 4) envPage->refresh();
    else if (currentTab == 5) arpPage->refresh();
    else if (currentTab == 6) seqPage->refresh();
    else if (currentTab == 7) mixerPage->refresh();
    if (frame % 3 == 0)
    {
        header->refresh();
        bottom->repaint (bottom->getLocalBounds().removeFromTop (24));
    }
    if (frame % 30 == 0) browser->repaint();
}

} // namespace vsx
