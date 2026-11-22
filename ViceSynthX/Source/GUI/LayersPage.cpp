#include "LayersPage.h"
#include "TabPages.h"

namespace vsx
{
namespace
{
const char* kCaptions[] = { "KEY LO", "KEY HI", "VEL LO", "VEL HI", "TRANSPOSE", "LEVEL dB", "PAN" };
juce::String layerLetter (int index) { return juce::String::charToString ((juce::juce_wchar) ('A' + index + 1)); }
}

LayerRow::LayerRow (ViceSynthProcessor& p, int i) : proc (p), index (i)
{
    const juce::String id = "vsx_layer_" + layerLetter (index).toLowerCase();
    setComponentID (id);
    name.setFont (Theme::font (13.0f, true));
    name.setColour (juce::Label::textColourId, Theme::text);
    name.setMinimumHorizontalScale (0.6f);
    for (auto* c : std::initializer_list<juce::Component*> { &name, &load, &edit, &clear }) addAndMakeVisible (c);
    if (index >= 0)
    {
        addAndMakeVisible (enable);
        enable.setComponentID (id + "_on");
        enable.onClick = [this] { proc.getLayer (index).enabled.store (enable.getToggleState()); };
        load.setComponentID (id + "_load");
        edit.setComponentID (id + "_edit");
        clear.setComponentID (id + "_clear");
        load.setTooltip ("Load a library sound into layer " + layerLetter (index) + " (the editor keeps layer A).");
        edit.setTooltip ("Swap this layer's sound with the editor, so it can be edited (layer A moves into this layer).");
        clear.setTooltip ("Remove the sound from this layer.");
        load.onClick = [this] { showLoadMenu(); };
        edit.onClick = [this] { proc.swapLayerWithMain (index); refresh(); };
        clear.onClick = [this] { proc.getLayer (index).clearSound(); refresh(); };
    }
    else
    {
        load.setVisible (false); edit.setVisible (false); clear.setVisible (false);
    }

    auto& r = routing();
    struct S { juce::Slider* s; double lo, hi; std::atomic<int>* a; };
    for (auto [s, lo, hi, a] : { S { &keyLo, 0, 127, &r.keyLo }, S { &keyHi, 0, 127, &r.keyHi }, S { &velLo, 1, 127, &r.velLo },
                                  S { &velHi, 1, 127, &r.velHi }, S { &transpose, -48, 48, &r.transpose } })
    {
        s->setSliderStyle (juce::Slider::IncDecButtons);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
        s->setRange (lo, hi, 1.0);
        s->setValue (a->load(), juce::dontSendNotification);
        auto* atom = a;
        s->onValueChange = [s, atom] { atom->store ((int) s->getValue()); };
        addAndMakeVisible (s);
    }
    keyLo.textFromValueFunction = keyHi.textFromValueFunction = [] (double v) { return juce::MidiMessage::getMidiNoteName ((int) v, true, true, 4); };
    keyLo.valueFromTextFunction = keyHi.valueFromTextFunction = [] (const juce::String& t)
    {
        for (int k = 0; k < 128; ++k) if (juce::MidiMessage::getMidiNoteName (k, true, true, 4).equalsIgnoreCase (t.trim())) return (double) k;
        return (double) t.getIntValue();
    };
    keyLo.updateText(); keyHi.updateText();
    keyLo.setTooltip ("Lowest key this layer plays (split point)."); keyHi.setTooltip ("Highest key this layer plays.");
    velLo.setTooltip ("Lowest velocity this layer plays (velocity layers)."); velHi.setTooltip ("Highest velocity this layer plays.");
    transpose.setTooltip ("Transpose in semitones (a key held while it changes is still released correctly).");
    keyLo.setComponentID (id + "_key_lo"); keyHi.setComponentID (id + "_key_hi");
    velLo.setComponentID (id + "_vel_lo"); velHi.setComponentID (id + "_vel_hi"); transpose.setComponentID (id + "_transpose");

    for (auto* s : { &level, &pan }) { s->setSliderStyle (juce::Slider::LinearHorizontal); s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20); }
    level.setRange (-48.0, 6.0, 0.1);
    pan.setRange (-1.0, 1.0, 0.01);
    level.setDoubleClickReturnValue (true, 0.0);
    pan.setDoubleClickReturnValue (true, 0.0);
    level.setComponentID (id + "_level"); pan.setComponentID (id + "_pan");
    if (index >= 0)
    {
        auto& L = proc.getLayer (index);
        level.setValue (L.levelDb.load(), juce::dontSendNotification);
        pan.setValue (L.pan.load(), juce::dontSendNotification);
        level.onValueChange = [this] { proc.getLayer (index).levelDb.store ((float) level.getValue()); };
        pan.onValueChange = [this] { proc.getLayer (index).pan.store ((float) pan.getValue()); };
        level.setTooltip ("Layer level (after the sound's own Preset Volume). Double-click: 0 dB.");
        pan.setTooltip ("Layer pan (equal power). Double-click: centre.");
        addAndMakeVisible (level);
        addAndMakeVisible (pan);
    }
    for (auto* c : kCaptions)
    {
        auto* l = captions.add (new juce::Label ({}, c));
        l->setFont (Theme::font (10.0f, true));
        l->setColour (juce::Label::textColourId, Theme::textDim);
        l->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    }
    if (index < 0) { captions[5]->setVisible (false); captions[6]->setVisible (false); }
    refresh();
}

LayerRouting& LayerRow::routing() { return index < 0 ? proc.getLayerARouting() : proc.getLayer (index).routing; }

void showLibraryMenu (ViceSynthProcessor& proc, juce::Component& target, std::function<void (const PatchSpec&)> onChosen)
{
    const auto& entries = proc.getPresetManager().getEntries();
    juce::StringArray cats;
    auto catOf = [] (const PresetManager::Entry& e) { return e.category.isEmpty() ? juce::String ("Other") : e.category; };
    for (const auto& e : entries) cats.addIfNotAlreadyThere (catOf (e));
    cats.sort (true);
    juce::PopupMenu menu;
    for (const auto& c : cats)
    {
        juce::PopupMenu sub;
        for (int k = 0; k < (int) entries.size(); ++k)
            if (catOf (entries[(size_t) k]) == c) sub.addItem (k + 1, entries[(size_t) k].name);
        menu.addSubMenu (c, sub);
    }
    juce::Component::SafePointer<juce::Component> safe (&target);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [safe, &proc, onChosen] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        const auto& es = proc.getPresetManager().getEntries();
        if (r - 1 >= (int) es.size()) return;
        PatchSpec spec; juce::String err;
        if (proc.getPresetManager().load (es[(size_t) (r - 1)], spec, err)) onChosen (spec);
    });
}

void LayerRow::showLoadMenu()
{
    juce::Component::SafePointer<LayerRow> self (this);
    showLibraryMenu (proc, load, [self] (const PatchSpec& spec)
    {
        if (self == nullptr) return;
        auto& L = self->proc.getLayer (self->index);
        L.setSound (spec);
        L.enabled.store (true);
        self->refresh();
    });
}

void LayerRow::refresh()
{
    auto& r = routing();
    auto sync = [] (juce::Slider& s, double v) { if (! s.isMouseButtonDown() && s.getValue() != v) s.setValue (v, juce::dontSendNotification); };
    sync (keyLo, r.keyLo.load()); sync (keyHi, r.keyHi.load()); sync (velLo, r.velLo.load()); sync (velHi, r.velHi.load()); sync (transpose, r.transpose.load());
    if (index < 0)
    {
        name.setText ("A   " + proc.getCurrentPatch().meta.name + "   (editor)", juce::dontSendNotification);
        return;
    }
    auto& L = proc.getLayer (index);
    enable.setToggleState (L.enabled.load(), juce::dontSendNotification);
    sync (level, L.levelDb.load()); sync (pan, L.pan.load());
    name.setText (layerLetter (index) + "   " + (L.hasSound() ? L.getSound().meta.name : juce::String ("(empty)")), juce::dontSendNotification);
    enable.setEnabled (L.hasSound());
    edit.setEnabled (L.hasSound());
    clear.setEnabled (L.hasSound());
}

void LayerRow::paint (juce::Graphics& g)
{
    const bool active = index < 0 || (proc.getLayer (index).enabled.load() && proc.getLayer (index).hasSound());
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Theme::panel.darker (0.25f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour ((active ? Theme::accentFor (juce::jmax (0, index + 1) % 3) : Theme::textDim).withAlpha (active ? 0.8f : 0.3f));
    g.drawRoundedRectangle (b, 6.0f, 1.2f);
    // key range bar
    auto& r = const_cast<LayerRow*> (this)->routing();
    auto bar = getLocalBounds().toFloat().reduced (10.0f, 0.0f).removeFromBottom (8.0f).withTrimmedBottom (3.0f);
    g.setColour (Theme::textDim.withAlpha (0.15f));
    g.fillRect (bar);
    const float lo = (float) r.keyLo.load() / 128.0f, hi = (float) (r.keyHi.load() + 1) / 128.0f;
    if (hi > lo)
    {
        g.setColour ((active ? Theme::cyberCyan : Theme::textDim).withAlpha (0.7f));
        g.fillRect (bar.getX() + bar.getWidth() * lo, bar.getY(), bar.getWidth() * (hi - lo), bar.getHeight());
    }
}

void LayerRow::resized()
{
    auto r = getLocalBounds().reduced (10, 6).withTrimmedBottom (8);
    auto top = r.removeFromTop (24);
    if (index >= 0) enable.setBounds (top.removeFromLeft (52));
    else top.removeFromLeft (52);
    auto btns = top.removeFromRight (std::min (240, top.getWidth() / 2));
    const int bw = btns.getWidth() / 3;
    load.setBounds (btns.removeFromLeft (bw).reduced (2, 0));
    edit.setBounds (btns.removeFromLeft (bw).reduced (2, 0));
    clear.setBounds (btns.reduced (2, 0));
    name.setBounds (top);
    r.removeFromTop (4);
    auto caps = r.removeFromTop (14);
    juce::Slider* cs[] = { &keyLo, &keyHi, &velLo, &velHi, &transpose, &level, &pan };
    const int n = 7;
    // integer fields narrower than level / pan
    const int unit = r.getWidth() / (5 * 4 + 2 * 6);
    for (int k = 0; k < n; ++k)
    {
        const int w = k < 5 ? unit * 4 : unit * 6;
        captions[k]->setBounds (caps.removeFromLeft (w).reduced (3, 0));
        cs[k]->setBounds (r.removeFromLeft (w).reduced (3, 0).withSizeKeepingCentre (w - 6, std::min (24, r.getHeight())));
    }
}

// =================================================================================
MorphRow::MorphRow (ViceSynthProcessor& p) : proc (p)
{
    setComponentID ("vsx_morph");
    title.setFont (Theme::font (13.0f, true));
    title.setColour (juce::Label::textColourId, Theme::neonPink);
    target.setFont (Theme::font (12.0f));
    target.setColour (juce::Label::textColourId, Theme::text);
    target.setComponentID ("vsx_morph_target");
    set.setComponentID ("vsx_morph_set");
    useCurrent.setComponentID ("vsx_morph_use_current");
    clear.setComponentID ("vsx_morph_clear");
    set.setTooltip ("Choose the sound to morph towards from the library.");
    useCurrent.setTooltip ("Make the current patch the morph target; then load or edit another patch and morph between them.");
    clear.setTooltip ("Remove the morph target (the patch plays unchanged).");
    set.onClick = [this] { showLibraryMenu (proc, set, [this] (const PatchSpec& s) { proc.setMorphTarget (s); refresh(); }); };
    useCurrent.onClick = [this] { proc.setMorphTarget (proc.getCurrentPatch()); refresh(); };
    clear.onClick = [this] { proc.clearMorphTarget(); refresh(); };
    amount = makeControl (p, pid::G (pid::global::Morph), Theme::neonPink);
    for (auto* c : std::initializer_list<juce::Component*> { &title, &target, &set, &useCurrent, &clear, amount.get() }) addAndMakeVisible (c);
    refresh();
}

void MorphRow::refresh()
{
    target.setText (proc.hasMorphTarget() ? "to  " + proc.getMorphTargetName()
                                          : juce::String ("no target: SET TARGET or USE CURRENT"), juce::dontSendNotification);
    clear.setEnabled (proc.hasMorphTarget());
}

void MorphRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Theme::panel.darker (0.25f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour ((proc.hasMorphTarget() ? Theme::neonPink : Theme::textDim).withAlpha (0.6f));
    g.drawRoundedRectangle (b, 6.0f, 1.2f);
}

void MorphRow::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    amount->setBounds (r.removeFromRight (std::min (90, r.getWidth() / 6)));
    r.removeFromRight (8);
    auto top = r.removeFromTop (r.getHeight() / 2);
    title.setBounds (top.removeFromLeft (70));
    target.setBounds (top);
    const int bw = std::min (130, r.getWidth() / 3);
    for (auto* b : { &set, &useCurrent, &clear }) b->setBounds (r.removeFromLeft (bw).reduced (2, 2));
}

LayersPage::LayersPage (ViceSynthProcessor& p) : morph (p)
{
    setComponentID ("vsx_layers_page");
    addAndMakeVisible (box);
    for (int i = -1; i < ViceSynthProcessor::kExtraLayers; ++i) addAndMakeVisible (rows.add (new LayerRow (p, i)));
    help.setText ("Each layer plays the keys and velocities in its range. Overlapping ranges stack layers (for example a sampled attack over a "
                  "synthesised pad); separate ranges make splits. B, C and D play complete sounds with their own effects; EDIT swaps a "
                  "layer into the editor. Layers are saved with the project.", juce::dontSendNotification);
    help.setFont (Theme::font (11.0f));
    help.setColour (juce::Label::textColourId, Theme::textDim);
    help.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (help);
    addAndMakeVisible (morph);
    startTimerHz (6);
}

void LayersPage::paint (juce::Graphics&) {}

void LayersPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    box.setBounds (r);
    auto c = box.content().reduced (4);
    help.setBounds (c.removeFromBottom (36));
    morph.setBounds (c.removeFromBottom (std::min (96, c.getHeight() / 5)).reduced (0, 3));
    const int h = std::min (92, c.getHeight() / rows.size());
    for (auto* row : rows) { row->setBounds (c.removeFromTop (h).reduced (0, 3)); }
}
} // namespace vsx
