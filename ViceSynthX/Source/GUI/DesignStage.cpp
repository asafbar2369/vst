#include "DesignStage.h"

namespace vsx
{
using namespace pid;

// =================================================================================
SoundDesignView::SoundDesignView (ViceSynthProcessor& p)
{
    const juce::StringArray names { "OSCILLATORS", "FILTERS & MOD", "MATRIX", "LFOs", "ENVELOPES", "FX RACK", "ARP", "SEQ", "MIXER", "FM", "VECTOR", "LAYERS" };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
        b->getProperties().set ("flat", true);
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
    }
    for (int o = 0; o < kNumOscs; ++o)
    {
        oscPanels[(size_t) o] = std::make_unique<OscPanel> (p, o);
        oscPanels[(size_t) o]->onOpenWavetableEditor = [this] (int osc) { if (onOpenWavetableEditor) onOpenWavetableEditor (osc); };
        oscPage.addAndMakeVisible (*oscPanels[(size_t) o]);
    }
    subNoise = std::make_unique<SubNoisePanel> (p);
    oscPage.addAndMakeVisible (*subNoise);
    for (int f = 0; f < kNumFilters; ++f)
    {
        filterPanels[(size_t) f] = std::make_unique<FilterPanel> (p, f);
        filterPage.addAndMakeVisible (*filterPanels[(size_t) f]);
    }
    modSection = std::make_unique<ModulationSection> (p);
    filterPage.addAndMakeVisible (*modSection);
    matrix = std::make_unique<MatrixPage> (p);
    lfos = std::make_unique<LfoPage> (p);
    envs = std::make_unique<EnvPage> (p);
    fx = std::make_unique<FxPage> (p);
    arp = std::make_unique<ArpPage> (p);
    seq = std::make_unique<SeqPage> (p);
    mixer = std::make_unique<MixerPage> (p);
    fmEditor = std::make_unique<FmEditor> (p);
    vector = std::make_unique<VectorPage> (p);
    layersPage = std::make_unique<LayersPage> (p);
    pages = { &oscPage, &filterPage, matrix.get(), lfos.get(), envs.get(), fx.get(), arp.get(), seq.get(), mixer.get(), fmEditor.get(), vector.get(), layersPage.get() };
    for (auto* pg : pages) addChildComponent (pg);
    showPage (Matrix);
}

void SoundDesignView::showPage (int page)
{
    current = juce::jlimit (0, (int) pages.size() - 1, page);
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == current);
        tabs[i]->setToggleState (i == current, juce::dontSendNotification);
    }
    if (onPageChanged) onPageChanged (current);
}

void SoundDesignView::resourceChanged (int osc)
{
    if (juce::isPositiveAndBelow (osc, kNumOscs)) oscPanels[(size_t) osc]->resourceChanged();
}

void SoundDesignView::refresh()
{
    switch (current)
    {
        case Oscillators: for (auto& o : oscPanels) o->refresh(); break;
        case Filters: for (auto& f : filterPanels) f->refresh(); modSection->refresh(); break;
        case Lfos: lfos->refresh(); break;
        case Envelopes: envs->refresh(); break;
        case Arp: arp->refresh(); break;
        case Seq: seq->refresh(); break;
        case Mixer: mixer->refresh(); break;
        default: break;
    }
}

void SoundDesignView::paint (juce::Graphics& g)
{
    auto t = getLocalBounds().removeFromTop (34).toFloat();
    Theme::card (g, t, 8.0f);
}

void SoundDesignView::resized()
{
    auto r = getLocalBounds();
    auto t = r.removeFromTop (34).reduced (4, 4);
    const int tw = t.getWidth() / tabs.size();
    for (auto* b : tabs) b->setBounds (t.removeFromLeft (tw).reduced (2, 0));
    r.removeFromTop (6);
    for (auto* pg : pages) pg->setBounds (r);

    auto s = oscPage.getLocalBounds();
    auto subRow = s.removeFromBottom (92);
    const int ow = s.getWidth() / 3;
    for (auto& o : oscPanels) o->setBounds (s.removeFromLeft (ow).reduced (3));
    subNoise->setBounds (subRow.reduced (3));

    auto f = filterPage.getLocalBounds();
    auto filterRow = f.removeFromTop (f.getHeight() * 45 / 100);
    filterPanels[0]->setBounds (filterRow.removeFromLeft (filterRow.getWidth() / 2).reduced (3));
    filterPanels[1]->setBounds (filterRow.reduced (3));
    modSection->setBounds (f.reduced (3));
}

// =================================================================================
StageView::StageView (ViceSynthProcessor& p) : proc (p), hero (p), scope (p), meter (p)
{
    hero.compact = false;
    addAndMakeVisible (hero);
    addAndMakeVisible (scope);
    addAndMakeVisible (meter);
    master = std::make_unique<ParamKnob> (p, G (global::MasterVolume), "Master", Theme::neonPink, ParamKnob::Style::Macro);
    addAndMakeVisible (*master);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = slots.add (new juce::TextButton());
        b->setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
        b->onClick = [this, i]
        {
            if (i >= (int) slotNames.size()) return;
            const int idx = proc.getPresetManager().indexOfName (slotNames[(size_t) i]);
            if (idx >= 0) proc.loadPresetEntry (proc.getPresetManager().getEntries()[(size_t) idx]);
        };
        addAndMakeVisible (b);
    }
    rebuildSlots();
}

void StageView::rebuildSlots()
{
    // quick slots = favourites (in library order), filled up with the current category
    const auto& entries = proc.getPresetManager().getEntries();
    slotNames.clear();
    for (const auto& e : entries) if (e.favorite && slotNames.size() < 8) slotNames.push_back (e.name);
    const bool haveFavs = ! slotNames.empty();
    const auto category = proc.getCurrentMeta().category;
    for (const auto& e : entries)
        if (slotNames.size() < 8 && e.category == category && std::find (slotNames.begin(), slotNames.end(), e.name) == slotNames.end())
            slotNames.push_back (e.name);
    slotSource = haveFavs ? "FAVOURITES" : (category.isNotEmpty() ? category.toUpperCase() : juce::String ("LIBRARY"));
    for (int i = 0; i < slots.size(); ++i)
    {
        const bool used = i < (int) slotNames.size();
        slots[i]->setButtonText (used ? juce::String (i + 1) + "  " + slotNames[(size_t) i] : juce::String ("-"));
        slots[i]->setEnabled (used);
        slots[i]->setTooltip (used ? "Load \"" + slotNames[(size_t) i] + "\"" : "Add favourites (heart) to fill the stage slots");
    }
    lastEntryCount = (int) entries.size();
    int favs = 0;
    for (const auto& e : entries) favs += e.favorite ? 1 : 0;
    lastFavCount = favs;
    repaint();
}

void StageView::refresh()
{
    hero.refresh();
    scope.repaint();
    meter.repaint();
    const auto& entries = proc.getPresetManager().getEntries();
    int favs = 0;
    for (const auto& e : entries) favs += e.favorite ? 1 : 0;
    if ((int) entries.size() != lastEntryCount || favs != lastFavCount || (favs == 0 && ! slotSource.equalsIgnoreCase (proc.getCurrentMeta().category)))
        rebuildSlots();
    const auto name = proc.getCurrentMeta().name;
    for (int i = 0; i < slots.size(); ++i)
        slots[i]->setToggleState (i < (int) slotNames.size() && slotNames[(size_t) i] == name, juce::dontSendNotification);
}

void StageView::paint (juce::Graphics& g)
{
    auto lower = getLocalBounds().withTrimmedTop (hero.getBottom() + 8).toFloat();
    Theme::card (g, lower, 10.0f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (12.0f, true).withExtraKerningFactor (0.1f));
    g.drawText (juce::String::fromUTF8 ("STAGE SLOTS  \xe2\x80\x94  ") + slotSource, lower.reduced (16.0f, 10.0f).removeFromTop (16.0f), juce::Justification::left);
    g.drawText ("OUTPUT", juce::Rectangle<float> ((float) scope.getX(), lower.getY() + 10.0f, 120.0f, 16.0f), juce::Justification::left);
}

void StageView::resized()
{
    auto r = getLocalBounds();
    hero.setBounds (r.removeFromTop (r.getHeight() * 52 / 100));
    r.removeFromTop (8);
    auto inner = r.reduced (16, 10).withTrimmedTop (22);
    auto right = inner.removeFromRight (inner.getWidth() * 42 / 100);
    master->setBounds (right.removeFromRight (96));
    meter.setBounds (right.removeFromRight (18).reduced (0, 4));
    right.removeFromRight (8);
    scope.setBounds (right.reduced (4, 0));
    inner.removeFromRight (16);
    const int rowsH = inner.getHeight() / 4, colW = inner.getWidth() / 2;
    for (int i = 0; i < slots.size(); ++i)
        slots[i]->setBounds (juce::Rectangle<int> (inner.getX() + (i / 4) * colW, inner.getY() + (i % 4) * rowsH, colW, rowsH).reduced (4, 3));
}

// =================================================================================
Overlay::Overlay()
{
    setWantsKeyboardFocus (true);
    close.onClick = [this] { hide(); };
    addAndMakeVisible (close);
}

void Overlay::show (const juce::String& t, juce::Component* c, juce::Point<int> size)
{
    if (content != nullptr && content != c) removeChildComponent (content);
    title = t;
    content = c;
    if (content != nullptr) addAndMakeVisible (content);
    card = juce::Rectangle<int> (size.x, size.y).withCentre (getLocalBounds().getCentre());
    resized();
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    repaint();
}

void Overlay::hide()
{
    if (content != nullptr) removeChildComponent (content);
    content = nullptr;
    setVisible (false);
}

void Overlay::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack.withAlpha (0.72f));
    Theme::card (g, card.toFloat(), 12.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (17.0f, true));
    g.drawText (title, card.reduced (18, 12).removeFromTop (28), juce::Justification::centredLeft);
}

void Overlay::resized()
{
    if (card.isEmpty()) return;
    card = card.withCentre (getLocalBounds().getCentre());
    auto c = card.reduced (14, 12);
    close.setBounds (c.removeFromTop (28).removeFromRight (90));
    c.removeFromTop (8);
    if (content != nullptr) content->setBounds (c);
}

void Overlay::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition())) hide();
}

bool Overlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { hide(); return true; }
    return false;
}

// =================================================================================
HelpView::HelpView()
{
    text.setMultiLine (true, true);
    text.setReadOnly (true);
    text.setCaretVisible (false);
    text.setScrollbarsShown (true);
    text.setFont (Theme::font (13.5f));
    text.setColour (juce::TextEditor::backgroundColourId, Theme::display);
    text.setText (
        "NAVIGATION\n"
        "  LIBRARY - preset cards for the current browser selection (click loads, double-click auditions, arrows + Enter).\n"
        "  STUDIO - the main sound controls: three oscillators, two filters, envelopes, LFOs and the first modulation slots.\n"
        "  SOUND DESIGN - every section in depth: oscillators (wavetable editor, sample import), filters + drag-and-drop\n"
        "                 modulation sources, the full 32-slot matrix, LFOs, envelopes, FX rack, arpeggiator, sequencer, mixer.\n"
        "  STAGE - large preset display, live output and eight quick-load slots (your favourites).\n\n"
        "KNOBS\n"
        "  Drag up/down to change. Shift + drag (or Shift + wheel) for fine control. Mouse wheel and arrow keys also work.\n"
        "  Double-click resets to the default. Double-click the value box to type an exact value.\n"
        "  Alt + drag on a knob edits the depth of the modulation currently assigned to it (coloured ring).\n"
        "  Drag a modulation chip (ENV / LFO / MACRO ...) onto a knob to create a routing.\n\n"
        "MACROS\n"
        "  The eight macros (Mood, Filter Sweep, Motion, Texture, Chorus, Space, Drive, Mix) are real engine controls\n"
        "  applied to every preset; hover a macro to see exactly which parameters it moves.\n\n"
        "PERFORMANCE\n"
        "  Pitch / mod wheels send MIDI to the engine. BEND sets the bend range, GLIDE the portamento time, POLY the voice\n"
        "  limit, UNISON the unison voices of every active oscillator. HOLD latches notes until it is released.\n"
        "  PANIC (hero panel) stops every note source: keyboard, host MIDI, arp, sequencer and held notes.\n\n"
        "AI SOUND DESIGNER\n"
        "  Without an API key the panel uses the built-in LOCAL RULE-BASED DESIGNER (offline, no network). The active engine is\n"
        "  always shown under GENERATE SOUND. Cloud / local LLM providers are configured in Settings (gear icon); keys are\n"
        "  stored encrypted by the operating system and never inside presets. AI results only ever become validated patch\n"
        "  changes (no code is executed) and every change can be undone.\n\n"
        "CPU\n"
        "  The header CPU value is the measured share of the audio callback time used by the plugin.\n");
    addAndMakeVisible (text);
}

void HelpView::resized() { text.setBounds (getLocalBounds()); }

} // namespace vsx
