#include "StudioView.h"

namespace vsx
{
using namespace pid;

Card::Card (ViceSynthProcessor& p, juce::String t, int onParam, juce::Colour a) : proc (p), title (std::move (t)), accent (a)
{
    if (onParam >= 0)
    {
        power = std::make_unique<ParamToggle> (p, onParam, "", a);
        power->setTooltip ("Enable / disable " + title);
        addAndMakeVisible (*power);
    }
}

void Card::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::card (g, r, 8.0f);
    auto tr = titleRow().toFloat();
    const bool on = power == nullptr || power->getToggleState();
    if (power != nullptr)
        Icons::draw (g, Icons::Type::Power, tr.withWidth (18.0f).reduced (1.0f), on ? accent : Theme::textDim, 1.7f);
    g.setColour (on ? Theme::text : Theme::textDim);
    g.setFont (Theme::font (14.0f, true));
    g.drawText (title, tr.withTrimmedLeft (power != nullptr ? 26.0f : 0.0f), juce::Justification::centredLeft, true);
}

static void addKnob (juce::Component& parent, std::vector<std::unique_ptr<ParamKnob>>& v, ViceSynthProcessor& p, int idx, const char* label,
                     juce::Colour c, ParamKnob::Style st = ParamKnob::Style::ValueBox)
{
    v.push_back (std::make_unique<ParamKnob> (p, idx, label, c, st));
    parent.addAndMakeVisible (*v.back());
}

static void row (std::vector<std::unique_ptr<ParamKnob>>& v, juce::Rectangle<int> r)
{
    const int w = r.getWidth() / std::max (1, (int) v.size());
    for (size_t i = 0; i < v.size(); ++i) v[i]->setBounds (r.getX() + (int) i * w, r.getY(), w, r.getHeight());
}

// =================================================================================
OscCard::OscCard (ViceSynthProcessor& p, int o)
    : Card (p, "OSC " + juce::String (o + 1), O (o, osc::On), Theme::accentFor (o == 0 ? 0 : o == 1 ? 3 : 1)), mode (p, O (o, osc::Mode)), display (p, o)
{
    // the power LED is drawn by the card: make the toggle an invisible hit area over the icon
    power->setAlpha (0.01f);
    mode.setTooltip ("Synthesis method of OSC " + juce::String (o + 1) + ". Wavetable / sample choice and the full controls are on SOUND DESIGN > OSCILLATORS.");
    addAndMakeVisible (mode);
    addAndMakeVisible (display);
    addKnob (*this, knobs, p, O (o, osc::Octave), "Oct", accent);
    addKnob (*this, knobs, p, O (o, osc::Semi), "Semi", accent);
    addKnob (*this, knobs, p, O (o, osc::Detune), "Detune", accent);
    knobs.back()->setTooltipExtra ("Unison detune: active when OSC unison is 2 or more (SOUND DESIGN > OSCILLATORS, or UNISON below).");
}

void OscCard::resized()
{
    power->setBounds (titleRow().withWidth (90));
    auto b = body();
    mode.setBounds (b.removeFromTop (26));
    b.removeFromTop (6);
    display.setBounds (b.removeFromTop (b.getHeight() - 84));
    b.removeFromTop (6);
    row (knobs, b);
}

// =================================================================================
FilterCard::FilterCard (ViceSynthProcessor& p, int f)
    : Card (p, "FILTER " + juce::String (f + 1), F (f, flt::On), f == 0 ? Theme::neonPink : Theme::cyberCyan), model (p, F (f, flt::Model)),
      mode (p, F (f, flt::Mode)), display (p, f)
{
    power->setAlpha (0.01f);
    addAndMakeVisible (model);
    addAndMakeVisible (mode);
    addAndMakeVisible (display);
    model.onChange = [this] { model.setTooltip (choices::filterModelDescription (model.getSelectedItemIndex())); };
    model.setTooltip (choices::filterModelDescription (model.getSelectedItemIndex()));
    addKnob (*this, knobs, p, F (f, flt::Cutoff), "Cutoff", accent, ParamKnob::Style::Compact);
    addKnob (*this, knobs, p, F (f, flt::Resonance), "Reso", accent, ParamKnob::Style::Compact);
    addKnob (*this, knobs, p, f == 0 ? F (f, flt::Drive) : F (f, flt::Mix), f == 0 ? "Drive" : "Mix", accent, ParamKnob::Style::Compact);
}

void FilterCard::resized()
{
    auto tr = titleRow();
    power->setBounds (tr.withWidth (100));
    tr.removeFromLeft (100);
    model.setBounds (tr.removeFromLeft (tr.getWidth() * 58 / 100).reduced (2, 0));
    mode.setBounds (tr.reduced (2, 0));
    auto b = body();
    display.setBounds (b.removeFromLeft (b.getWidth() * 47 / 100));
    b.removeFromLeft (6);
    row (knobs, b);
}

// =================================================================================
EnvCard::EnvCard (ViceSynthProcessor& p) : Card (p, "", -1, Theme::neonPink), editor (p, 0)
{
    addAndMakeVisible (editor);
    const juce::StringArray names { "ENVELOPE (AMP)", "ENV 2", "ENV 3", "ENV 4" };
    for (int e = 0; e < kNumEnvs; ++e)
    {
        auto* b = tabs.add (new juce::TextButton (names[e]));
        b->setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
        b->getProperties().set ("flat", true);
        b->onClick = [this, e] { select (e); };
        addAndMakeVisible (b);
        knobs.emplace_back();
        addKnob (*this, knobs.back(), p, E (e, env::Attack), "A", Theme::neonPink);
        addKnob (*this, knobs.back(), p, E (e, env::Decay), "D", Theme::neonPink);
        addKnob (*this, knobs.back(), p, E (e, env::Sustain), "S", Theme::neonPink);
        addKnob (*this, knobs.back(), p, E (e, env::Release), "R", Theme::neonPink);
    }
    select (0);
}

void EnvCard::select (int e)
{
    current = e;
    editor.setEnvelope (e);
    for (int i = 0; i < kNumEnvs; ++i)
    {
        tabs[i]->setToggleState (i == e, juce::dontSendNotification);
        for (auto& k : knobs[(size_t) i]) k->setVisible (i == e);
    }
    resized();
}

void EnvCard::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto t = r.removeFromTop (28);
    tabs[0]->setBounds (t.removeFromLeft (132).reduced (1));
    for (int i = 1; i < tabs.size(); ++i) tabs[i]->setBounds (t.removeFromLeft (62).reduced (1));
    r.removeFromTop (4);
    auto k = r.removeFromBottom (72);
    editor.setBounds (r.reduced (4, 0));
    row (knobs[(size_t) current], k);
}

// =================================================================================
LfoCard::LfoCard (ViceSynthProcessor& p) : Card (p, "", -1, Theme::cyberCyan), editor (p, 0)
{
    addAndMakeVisible (editor);
    for (int l = 0; l < 3; ++l)
    {
        auto* b = tabs.add (new juce::TextButton (l == 0 ? "LFO" : "LFO " + juce::String (l + 1)));
        b->setColour (juce::TextButton::buttonOnColourId, Theme::cyberCyan);
        b->getProperties().set ("flat", true);
        b->onClick = [this, l] { select (l); };
        addAndMakeVisible (b);
    }
    for (int l = 0; l < kNumLfos; ++l)
    {
        Ctl c;
        c.shape = std::make_unique<ParamCombo> (p, L (l, lfo::Shape));
        c.sync = std::make_unique<ParamToggle> (p, L (l, lfo::Sync), "Sync", Theme::neonPink);
        c.rate = std::make_unique<ParamKnob> (p, L (l, lfo::Rate), "Rate", Theme::cyberCyan, ParamKnob::Style::ValueBox);
        c.depth = std::make_unique<ParamKnob> (p, L (l, lfo::Depth), "Depth", Theme::cyberCyan, ParamKnob::Style::ValueBox);
        addChildComponent (*c.shape); addChildComponent (*c.sync); addChildComponent (*c.rate); addChildComponent (*c.depth);
        ctls.push_back (std::move (c));
    }
    select (0);
}

void LfoCard::select (int l)
{
    current = l;
    editor.setLfo (l);
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setToggleState (i == l, juce::dontSendNotification);
    for (int i = 0; i < (int) ctls.size(); ++i)
    {
        const bool v = i == l;
        ctls[(size_t) i].shape->setVisible (v); ctls[(size_t) i].sync->setVisible (v);
        ctls[(size_t) i].rate->setVisible (v); ctls[(size_t) i].depth->setVisible (v);
    }
    resized();
}

void LfoCard::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto t = r.removeFromTop (28);
    for (auto* b : tabs) b->setBounds (t.removeFromLeft (66).reduced (1));
    r.removeFromTop (4);
    auto& c = ctls[(size_t) current];
    auto top = r.removeFromTop (r.getHeight() - 74);
    auto side = top.removeFromRight (120);
    editor.setBounds (top.reduced (4, 0));
    c.shape->setBounds (side.removeFromTop (28).reduced (4, 2));
    auto bottom = r;
    auto syncArea = bottom.removeFromRight (120);
    c.sync->setBounds (syncArea.withSizeKeepingCentre (90, 28));
    c.rate->setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
    c.depth->setBounds (bottom);
}

// =================================================================================
class ModCard::Row : public juce::Component
{
public:
    Row (ViceSynthProcessor& p, int s) : proc (p), slot (s)
    {
        for (int i = 0; i < modsrc::Count; ++i) source.addItem (i == 0 ? juce::String ("None") : modsrc::name (i), i + 1);
        auto* root = dest.getRootMenu();
        root->addItem (1, "None");
        std::map<juce::String, juce::PopupMenu> groups;
        juce::StringArray order;
        for (int idx : ParamRegistry::get().modulatableIndices())
        {
            const auto& d = ParamRegistry::get()[idx];
            if (! groups.count (d.group)) order.add (d.group);
            groups[d.group].addItem (idx + 2, d.name);
        }
        for (const auto& gname : order) root->addSubMenu (gname, groups[gname]);
        amount.setSliderStyle (juce::Slider::LinearBar);
        amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        amount.setColour (juce::Slider::trackColourId, Theme::neonPink.withAlpha (0.8f));
        amount.setColour (juce::Slider::backgroundColourId, Theme::display);
        amount.setTooltip ("Modulation depth (bipolar, fraction of the destination range). Full matrix: SOUND DESIGN > MATRIX.");
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.getAPVTS(), ParamRegistry::get()[MODAMT (s)].id, amount);
        for (auto* c : std::initializer_list<juce::Component*> { &source, &dest, &amount }) addAndMakeVisible (c);
        auto& st = proc.getPatch().slots[(size_t) s];
        source.onChange = [this, &st] { if (! syncing) st.source = source.getSelectedId() - 1; };
        dest.onChange = [this, &st] { if (! syncing) st.dest = dest.getSelectedId() - 2; };
        amount.onValueChange = [this] { repaint(); };
        sync();
    }
    void sync()
    {
        syncing = true;
        const auto& st = proc.getPatch().slots[(size_t) slot];
        source.setSelectedId (st.source.load() + 1, juce::dontSendNotification);
        dest.setSelectedId (st.dest.load() + 2, juce::dontSendNotification);
        syncing = false;
        repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        source.setBounds (r.removeFromLeft (r.getWidth() * 27 / 100));
        r.removeFromLeft (16);
        dest.setBounds (r.removeFromLeft (r.getWidth() * 48 / 100));
        r.removeFromLeft (6);
        valueArea = r.removeFromLeft (44);
        amount.setBounds (r.reduced (2, 6));
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (">", juce::Rectangle<int> (source.getRight(), 0, 16, getHeight()), juce::Justification::centred);
        const float v = (float) amount.getValue();
        g.setColour (Theme::text);
        g.drawText ((v >= 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %", valueArea, juce::Justification::centredRight);
    }
private:
    ViceSynthProcessor& proc;
    int slot;
    bool syncing = false;
    juce::ComboBox source, dest;
    juce::Slider amount;
    juce::Rectangle<int> valueArea;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

ModCard::ModCard (ViceSynthProcessor& p) : Card (p, "MODULATION", -1, Theme::electricPurple)
{
    for (int s = 0; s < 4; ++s) addAndMakeVisible (rows.add (new Row (p, s)));
    startTimerHz (4);
}

void ModCard::timerCallback()
{
    if (isShowing()) for (auto* r : rows) r->sync();
}

void ModCard::resized()
{
    auto b = body();
    const int h = std::min (34, b.getHeight() / 4);
    for (auto* r : rows) r->setBounds (b.removeFromTop (h));
}

// =================================================================================
StudioView::StudioView (ViceSynthProcessor& p)
{
    for (int o = 0; o < kNumOscs; ++o) { oscs.push_back (std::make_unique<OscCard> (p, o)); addAndMakeVisible (*oscs.back()); }
    for (int f = 0; f < kNumFilters; ++f) { filters.push_back (std::make_unique<FilterCard> (p, f)); addAndMakeVisible (*filters.back()); }
    env = std::make_unique<EnvCard> (p);
    lfo = std::make_unique<LfoCard> (p);
    mod = std::make_unique<ModCard> (p);
    addAndMakeVisible (*env);
    addAndMakeVisible (*lfo);
    addAndMakeVisible (*mod);
}

void StudioView::refresh()
{
    for (auto& o : oscs) o->refresh();
    for (auto& f : filters) f->refresh();
    env->refresh();
    lfo->refresh();
}

void StudioView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() * 54 / 100);
    r.removeFromTop (8);
    auto filterCol = top.removeFromRight (top.getWidth() * 38 / 100);
    const int ow = top.getWidth() / 3;
    for (auto& o : oscs) o->setBounds (top.removeFromLeft (ow).reduced (4, 0).withTrimmedRight (4));
    const int fh = filterCol.getHeight() / 2;
    filters[0]->setBounds (filterCol.removeFromTop (fh).withTrimmedBottom (4));
    filters[1]->setBounds (filterCol.withTrimmedTop (4));
    const int w = r.getWidth();
    env->setBounds (r.removeFromLeft (w * 33 / 100).withTrimmedRight (6));
    lfo->setBounds (r.removeFromLeft (w * 31 / 100).withTrimmedRight (6));
    mod->setBounds (r);
}

ModCard::~ModCard() { stopTimer(); }

} // namespace vsx
