#include "TabPages.h"
#include "AISettingsPanel.h"

namespace vsx
{
using namespace pid;

std::unique_ptr<juce::Component> makeControl (ViceSynthProcessor& p, int idx, juce::Colour c)
{
    const auto& d = ParamRegistry::get()[idx];
    juce::String label = d.name;
    for (const auto& prefix : { "Filter 1 ", "Filter 2 ", "OSC A ", "OSC B ", "OSC C ", "Sub ", "Noise " })
        if (label.startsWith (prefix)) label = label.fromFirstOccurrenceOf (prefix, false, false);
    for (int t = 0; t < NumFx; ++t)
        if (label.startsWith (choices::fxTypeNames[t] + " ")) label = label.fromFirstOccurrenceOf (choices::fxTypeNames[t] + " ", false, false);
    if (label.startsWith ("ENV ") || label.startsWith ("LFO ")) label = label.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false);
    if (label.startsWith ("Arp ") || label.startsWith ("Seq ")) label = label.fromFirstOccurrenceOf (" ", false, false);
    switch (d.type)
    {
        case ParamType::Choice: return std::make_unique<ParamCombo> (p, idx);
        case ParamType::Bool: return std::make_unique<ParamToggle> (p, idx, label.toUpperCase(), c);
        default: return std::make_unique<ParamKnob> (p, idx, label, c);
    }
}

static void gridLayout (std::vector<std::unique_ptr<juce::Component>>& comps, juce::Rectangle<int> r, int cols, int rowH)
{
    const int w = r.getWidth() / std::max (1, cols);
    int i = 0;
    for (auto& c : comps)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (i % cols) * w, r.getY() + (i / cols) * rowH, w, rowH);
        if (dynamic_cast<juce::ComboBox*> (c.get()) != nullptr || dynamic_cast<juce::ToggleButton*> (c.get()) != nullptr)
            cell = cell.withSizeKeepingCentre (cell.getWidth() - 4, 24);
        c->setBounds (cell);
        ++i;
    }
}

// =================================================================================
class FxPage::Row : public juce::Component
{
public:
    Row (ViceSynthProcessor& p, int fxType, FxPage& o) : proc (p), type (fxType), owner (o), on (p, FX (fxType, fx::On), choices::fxTypeNames[fxType].toUpperCase(), Theme::accentFor (fxType)),
        bus (p, FX (fxType, fx::Bus))
    {
        addAndMakeVisible (on);
        addAndMakeVisible (bus);
        up.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb2"));
        down.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xbc"));
        up.onClick = [this] { move (-1); };
        down.onClick = [this] { move (1); };
        up.setTooltip ("Move earlier in the chain");
        down.setTooltip ("Move later in the chain");
        addAndMakeVisible (up);
        addAndMakeVisible (down);
        for (int pi = fx::Mix; pi < kFxParamCounts[fxType]; ++pi)
        {
            if (pi == fx::Bus) continue;
            controls.push_back (makeControl (p, FX (fxType, pi), Theme::accentFor (fxType)));
            addAndMakeVisible (*controls.back());
        }
    }
    void move (int delta)
    {
        auto& order = proc.getPatch().fxOrder;
        int pos = -1;
        for (int i = 0; i < NumFx; ++i) if (order[(size_t) i].load() == type) pos = i;
        const int np = pos + delta;
        if (pos < 0 || np < 0 || np >= NumFx) return;
        const int other = order[(size_t) np].load();
        order[(size_t) np] = type;
        order[(size_t) pos] = other;
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<FxPage> (&owner)] { if (safe != nullptr) safe->rebuildOrder(); });
    }
    void paint (juce::Graphics& g) override
    {
        const bool active = proc.getParam (FX (type, fx::On)) > 0.5f;
        Theme::glassPanel (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f, active ? Theme::accentFor (type) : Theme::outline);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        auto handle = r.removeFromLeft (22);
        up.setBounds (handle.removeFromTop (handle.getHeight() / 2).reduced (1));
        down.setBounds (handle.reduced (1));
        auto left = r.removeFromLeft (170);
        on.setBounds (left.removeFromTop (26));
        bus.setBounds (left.removeFromTop (24).withWidth (70));
        const int w = std::max (52, r.getWidth() / std::max (1, (int) controls.size()));
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto cell = juce::Rectangle<int> (r.getX() + (int) i * std::min (w, 78), r.getY(), std::min (w, 78), r.getHeight());
            if (dynamic_cast<juce::ComboBox*> (controls[i].get()) != nullptr || dynamic_cast<juce::ToggleButton*> (controls[i].get()) != nullptr)
                cell = cell.withSizeKeepingCentre (cell.getWidth() - 4, 24);
            controls[i]->setBounds (cell);
        }
    }
    int type;
private:
    ViceSynthProcessor& proc;
    FxPage& owner;
    ParamToggle on;
    ParamCombo bus;
    juce::TextButton up, down;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

FxPage::FxPage (ViceSynthProcessor& p) : proc (p), routing (p, G (global::FxRouting))
{
    balance = std::make_unique<ParamKnob> (p, G (global::FxBalance), "A|B", Theme::cyberCyan);
    addAndMakeVisible (routing);
    addAndMakeVisible (*balance);
    hint.setText ("Reorder with the arrows. Serial: every enabled effect in order. Parallel A|B: bus A and bus B process the dry signal side by side.",
                  juce::dontSendNotification);
    hint.setFont (Theme::font (12.0f));
    hint.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (hint);
    for (int t = 0; t < NumFx; ++t)
    {
        auto* row = rows.add (new Row (p, t, *this));
        rack.addAndMakeVisible (row);
    }
    viewport.setViewedComponent (&rack, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

FxPage::~FxPage() = default;

void FxPage::rebuildOrder()
{
    const int w = viewport.getWidth() - 12, h = 66;
    for (int i = 0; i < NumFx; ++i)
    {
        const int t = proc.getPatch().fxOrder[(size_t) i].load();
        for (auto* r : rows)
            if (r->type == t) r->setBounds (0, i * (h + 4), w, h);
    }
    rack.setSize (w, NumFx * (h + 4));
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (54);
    routing.setBounds (top.removeFromLeft (160).withSizeKeepingCentre (160, 24));
    balance->setBounds (top.removeFromLeft (60));
    hint.setBounds (top.reduced (8, 0));
    viewport.setBounds (r);
    rebuildOrder();
}

// =================================================================================
class MatrixPage::Row : public juce::Component
{
public:
    Row (ViceSynthProcessor& p, int s) : proc (p), slot (s)
    {
        num.setText (juce::String (s + 1), juce::dontSendNotification);
        num.setColour (juce::Label::textColourId, Theme::textDim);
        addAndMakeVisible (num);
        for (int i = 0; i < modsrc::Count; ++i) source.addItem (modsrc::name (i), i + 1);
        for (int i = 0; i < modsrc::Count; ++i) aux.addItem (i == 0 ? juce::String ("No scaling") : "x " + modsrc::name (i), i + 1);
        polarity.addItemList ({ "Native", "Bipolar", "Unipolar" }, 1);
        // destinations grouped by section
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
        for (auto* c : std::initializer_list<juce::Component*> { &source, &dest, &polarity, &aux, &amount, &curve, &clear }) addAndMakeVisible (c);
        amount.setSliderStyle (juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        amount.setColour (juce::Slider::trackColourId, Theme::cyberCyan);
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.getAPVTS(), ParamRegistry::get()[MODAMT (s)].id, amount);
        curve.setSliderStyle (juce::Slider::LinearHorizontal);
        curve.setRange (-1.0, 1.0, 0.01);
        curve.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        curve.setColour (juce::Slider::trackColourId, Theme::sunsetOrange);
        curve.setTooltip ("Response curve");
        clear.setButtonText ("X");
        clear.setTooltip ("Clear this slot");
        auto& st = proc.getPatch().slots[(size_t) s];
        source.onChange = [this, &st] { if (! syncing) st.source = source.getSelectedId() - 1; };
        dest.onChange = [this, &st] { if (! syncing) st.dest = dest.getSelectedId() - 2; };
        polarity.onChange = [this, &st] { if (! syncing) st.polarity = polarity.getSelectedId() - 1; };
        aux.onChange = [this, &st] { if (! syncing) st.aux = aux.getSelectedId() - 1; };
        curve.onValueChange = [this, &st] { if (! syncing) st.curve = (float) curve.getValue(); };
        clear.onClick = [this] { proc.clearModulationSlot (slot); sync(); };
        sync();
    }
    void sync()
    {
        syncing = true;
        const auto& st = proc.getPatch().slots[(size_t) slot];
        source.setSelectedId (st.source.load() + 1, juce::dontSendNotification);
        dest.setSelectedId (st.dest.load() + 2, juce::dontSendNotification);
        polarity.setSelectedId (st.polarity.load() + 1, juce::dontSendNotification);
        aux.setSelectedId (st.aux.load() + 1, juce::dontSendNotification);
        curve.setValue (st.curve.load(), juce::dontSendNotification);
        syncing = false;
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (2, 2);
        num.setBounds (r.removeFromLeft (26));
        source.setBounds (r.removeFromLeft (130).reduced (2, 0));
        dest.setBounds (r.removeFromLeft (220).reduced (2, 0));
        amount.setBounds (r.removeFromLeft (220).reduced (2, 0));
        polarity.setBounds (r.removeFromLeft (100).reduced (2, 0));
        curve.setBounds (r.removeFromLeft (110).reduced (2, 0));
        aux.setBounds (r.removeFromLeft (150).reduced (2, 0));
        clear.setBounds (r.removeFromLeft (30).reduced (2, 0));
    }
    void paint (juce::Graphics& g) override
    {
        if (proc.getPatch().slots[(size_t) slot].active())
        {
            g.setColour (Theme::cyberCyan.withAlpha (0.07f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        }
    }
private:
    ViceSynthProcessor& proc;
    int slot;
    bool syncing = false;
    juce::Label num;
    juce::ComboBox source, dest, polarity, aux;
    juce::Slider amount, curve;
    juce::TextButton clear;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
};

MatrixPage::MatrixPage (ViceSynthProcessor& p) : proc (p)
{
    header.setText ("  #   SOURCE                     DESTINATION                                  AMOUNT                                           POLARITY            CURVE                  SCALED BY",
                    juce::dontSendNotification);
    header.setFont (Theme::font (11.0f, true));
    header.setColour (juce::Label::textColourId, Theme::cyberCyan);
    addAndMakeVisible (header);
    for (int s = 0; s < kNumModSlots; ++s) table.addAndMakeVisible (rows.add (new Row (p, s)));
    viewport.setViewedComponent (&table, false);
    addAndMakeVisible (viewport);
    startTimerHz (4);
}

MatrixPage::~MatrixPage() = default;

void MatrixPage::timerCallback()
{
    if (! isShowing()) return;
    for (auto* r : rows) { r->sync(); r->repaint(); }
}

void MatrixPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    header.setBounds (r.removeFromTop (20));
    viewport.setBounds (r);
    const int w = r.getWidth() - 14;
    for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, i * 28, w, 28);
    table.setSize (w, rows.size() * 28);
}

// =================================================================================
LfoPage::LfoPage (ViceSynthProcessor& p)
{
    for (int l = 0; l < kNumLfos; ++l)
    {
        Panel pn;
        pn.box = std::make_unique<Section> ("LFO " + juce::String (l + 1), Theme::cyberCyan);
        pn.editor = std::make_unique<LfoEditor> (p, l);
        addAndMakeVisible (*pn.box);
        addAndMakeVisible (*pn.editor);
        for (int pi = 0; pi < lfo::Count; ++pi)
        {
            pn.controls.push_back (makeControl (p, L (l, pi), Theme::cyberCyan));
            addAndMakeVisible (*pn.controls.back());
        }
        panels.push_back (std::move (pn));
    }
}

void LfoPage::refresh() { for (auto& pn : panels) pn.editor->repaint(); }

void LfoPage::resized()
{
    auto r = getLocalBounds().reduced (4);
    const int w = r.getWidth() / 5, h = r.getHeight() / 2;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (l % 5) * w, r.getY() + (l / 5) * h, w, h).reduced (3);
        auto& pn = panels[(size_t) l];
        pn.box->setBounds (cell);
        auto c = cell.reduced (8).withTrimmedTop (18);
        pn.editor->setBounds (c.removeFromTop (c.getHeight() * 38 / 100));
        c.removeFromTop (4);
        gridLayout (pn.controls, c, 3, c.getHeight() / 3);
    }
}

// =================================================================================
EnvPage::EnvPage (ViceSynthProcessor& p)
{
    for (int e = 0; e < kNumEnvs; ++e)
    {
        Panel pn;
        pn.box = std::make_unique<Section> ("ENV " + juce::String (e + 1) + (e == 0 ? "  (AMP)" : e == 1 ? "  (FILTER)" : ""), Theme::neonPink);
        pn.editor = std::make_unique<EnvelopeEditor> (p, e);
        addAndMakeVisible (*pn.box);
        addAndMakeVisible (*pn.editor);
        for (int pi = 0; pi < env::Count; ++pi)
        {
            pn.controls.push_back (makeControl (p, E (e, pi), Theme::neonPink));
            addAndMakeVisible (*pn.controls.back());
        }
        panels.push_back (std::move (pn));
    }
}

void EnvPage::refresh() { for (auto& pn : panels) pn.editor->repaint(); }

void EnvPage::resized()
{
    auto r = getLocalBounds().reduced (4);
    const int w = r.getWidth() / 2, h = r.getHeight() / 2;
    for (int e = 0; e < kNumEnvs; ++e)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (e % 2) * w, r.getY() + (e / 2) * h, w, h).reduced (3);
        auto& pn = panels[(size_t) e];
        pn.box->setBounds (cell);
        auto c = cell.reduced (8).withTrimmedTop (18);
        pn.editor->setBounds (c.removeFromTop (c.getHeight() - 120));
        c.removeFromTop (4);
        gridLayout (pn.controls, c, 5, 58);
    }
}

// =================================================================================
class ArpPage::Grid : public juce::Component
{
public:
    explicit Grid (ViceSynthProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        const auto ap = proc.getPatch().getArpPattern();
        const int n = ap.length;
        auto r = getLocalBounds().toFloat();
        const float cw = r.getWidth() / 32.0f;
        const float laneH = r.getHeight() / 4.0f;
        const char* names[] = { "STEP", "VELOCITY", "OCTAVE", "GATE" };
        const int cur = proc.getEngine().seqStepDisplay.load();
        for (int lane = 0; lane < 4; ++lane)
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (10.0f, true));
            g.drawText (names[lane], juce::Rectangle<float> (r.getX(), r.getY() + laneH * (float) lane, 80.0f, 12.0f), juce::Justification::left);
        }
        for (int i = 0; i < 32; ++i)
        {
            const auto& st = ap.steps[(size_t) i];
            const bool inRange = i < n;
            const float x = r.getX() + cw * (float) i;
            const auto accent = (i % 4 == 0) ? Theme::neonPink : Theme::cyberCyan;
            auto cell = juce::Rectangle<float> (x + 1.0f, r.getY() + 13.0f, cw - 2.0f, laneH - 15.0f);
            g.setColour (inRange ? (st.on ? accent.withAlpha (0.8f) : Theme::panelLight) : Theme::deepBlack);
            g.fillRoundedRectangle (cell, 3.0f);
            if (i == cur && proc.getParam (ARP (arp::Mode)) > 0.5f && proc.getParam (ARP (arp::Mode)) < 1.5f)
            {
                g.setColour (juce::Colours::white);
                g.drawRoundedRectangle (cell, 3.0f, 2.0f);
            }
            auto vel = juce::Rectangle<float> (x + 1.0f, r.getY() + laneH + 13.0f, cw - 2.0f, laneH - 15.0f);
            g.setColour (Theme::deepBlack);
            g.fillRect (vel);
            g.setColour (Theme::sunsetOrange.withAlpha (inRange ? 0.9f : 0.2f));
            g.fillRect (vel.withTop (vel.getBottom() - vel.getHeight() * st.velocity));
            auto oct = juce::Rectangle<float> (x + 1.0f, r.getY() + laneH * 2.0f + 13.0f, cw - 2.0f, laneH - 15.0f);
            g.setColour (Theme::deepBlack);
            g.fillRect (oct);
            g.setColour (Theme::electricPurple.withAlpha (inRange ? 0.95f : 0.2f));
            const float mid = oct.getCentreY();
            g.fillRect (oct.withY (std::min (mid, mid - (float) st.octave * oct.getHeight() / 4.0f)).withHeight (std::max (2.0f, std::abs ((float) st.octave) * oct.getHeight() / 4.0f)));
            auto gate = juce::Rectangle<float> (x + 1.0f, r.getY() + laneH * 3.0f + 13.0f, cw - 2.0f, laneH - 15.0f);
            g.setColour (Theme::deepBlack);
            g.fillRect (gate);
            g.setColour (Theme::cyberCyan.withAlpha (inRange ? 0.9f : 0.2f));
            g.fillRect (gate.withTop (gate.getBottom() - gate.getHeight() * st.gate / 2.0f));
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { edit (e, true); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e, false); }
private:
    void edit (const juce::MouseEvent& e, bool click)
    {
        auto ap = proc.getPatch().getArpPattern();
        const float cw = (float) getWidth() / 32.0f, laneH = (float) getHeight() / 4.0f;
        const int i = juce::jlimit (0, 31, (int) ((float) e.x / cw));
        const int lane = juce::jlimit (0, 3, (int) ((float) e.y / laneH));
        auto& st = ap.steps[(size_t) i];
        const float frac = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - laneH * (float) lane - 13.0f) / (laneH - 15.0f));
        if (lane == 0 && click) { st.on = ! st.on; dragOn = st.on; }
        else if (lane == 0) st.on = dragOn;
        else if (lane == 1) st.velocity = frac;
        else if (lane == 2) st.octave = juce::jlimit (-2, 2, (int) std::round ((frac - 0.5f) * 4.0f));
        else st.gate = juce::jlimit (0.05f, 2.0f, frac * 2.0f);
        proc.getPatch().setArpPattern (ap);
        repaint();
    }
    ViceSynthProcessor& proc;
    bool dragOn = true;
};

ArpPage::ArpPage (ViceSynthProcessor& p) : proc (p)
{
    addAndMakeVisible (box);
    for (int pi = 0; pi < arp::Count; ++pi)
    {
        controls.push_back (makeControl (p, ARP (pi), Theme::cyberCyan));
        addAndMakeVisible (*controls.back());
    }
    grid = std::make_unique<Grid> (p);
    addAndMakeVisible (*grid);
    length.setRange (1, 32, 1);
    length.setValue (p.getPatch().getArpPattern().length, juce::dontSendNotification);
    length.setSliderStyle (juce::Slider::IncDecButtons);
    length.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 22);
    length.onValueChange = [this] { auto ap = proc.getPatch().getArpPattern(); ap.length = (int) length.getValue(); proc.getPatch().setArpPattern (ap); grid->repaint(); };
    addAndMakeVisible (length);
    lengthLabel.setText ("PATTERN LENGTH", juce::dontSendNotification);
    lengthLabel.setFont (Theme::font (11.0f, true));
    addAndMakeVisible (lengthLabel);
    addAndMakeVisible (presetsB);
    addAndMakeVisible (randomB);
    presetsB.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Straight (all on)");
        m.addItem (2, "Accents on the beat");
        m.addItem (3, "Offbeat");
        m.addItem (4, "Gallop");
        m.addItem (5, "Octave bounce");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsB), [this] (int r)
        {
            if (r <= 0) return;
            ArpPattern ap;
            ap.length = 16;
            for (int i = 0; i < 32; ++i)
            {
                auto& st = ap.steps[(size_t) i];
                st = ArpStep();
                if (r == 2) st.velocity = i % 4 == 0 ? 1.0f : 0.55f;
                if (r == 3) st.on = i % 2 == 1;
                if (r == 4) st.on = i % 4 != 1;
                if (r == 5) st.octave = (i % 4 == 2) ? 1 : (i % 8 == 7 ? -1 : 0);
            }
            proc.getPatch().setArpPattern (ap);
            length.setValue (16, juce::dontSendNotification);
            grid->repaint();
        });
    };
    randomB.onClick = [this]
    {
        auto ap = proc.getPatch().getArpPattern();
        juce::Random rnd;
        for (int i = 0; i < ap.length; ++i)
        {
            auto& st = ap.steps[(size_t) i];
            st.on = rnd.nextFloat() > 0.2f;
            st.velocity = 0.5f + 0.5f * rnd.nextFloat();
            st.octave = rnd.nextFloat() > 0.8f ? (rnd.nextBool() ? 1 : -1) : 0;
            st.gate = 0.5f + rnd.nextFloat();
        }
        proc.getPatch().setArpPattern (ap);
        grid->repaint();
    };
}

ArpPage::~ArpPage() = default;

void ArpPage::refresh() { grid->repaint(); }

void ArpPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    box.setBounds (r);
    auto c = box.content();
    gridLayout (controls, c.removeFromTop (64), 10, 64);
    c.removeFromTop (6);
    auto tools = c.removeFromTop (26);
    lengthLabel.setBounds (tools.removeFromLeft (120));
    length.setBounds (tools.removeFromLeft (110));
    tools.removeFromLeft (10);
    presetsB.setBounds (tools.removeFromLeft (110));
    tools.removeFromLeft (6);
    randomB.setBounds (tools.removeFromLeft (110));
    c.removeFromTop (8);
    grid->setBounds (c);
}

// =================================================================================
class SeqPage::Grid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Grid (ViceSynthProcessor& p) : proc (p) {}
    int laneMode = 0; // 0 velocity, 1 gate, 2 probability, 3 mod
    void paint (juce::Graphics& g) override
    {
        const auto sp = proc.getPatch().getSeqPattern();
        const int len = (int) proc.getParam (SEQ (seq::Length));
        auto r = getLocalBounds().toFloat();
        auto notes = r.removeFromTop (r.getHeight() * 0.72f);
        auto laneR = r.withTrimmedTop (6.0f);
        const float cw = notes.getWidth() / 32.0f, rh = notes.getHeight() / 25.0f;
        const int cur = proc.getEngine().seqStepDisplay.load();
        for (int row = 0; row < 25; ++row)
        {
            const int semi = 12 - row;
            const bool black = juce::MidiMessage::isMidiNoteBlack (60 + semi);
            g.setColour (black ? Theme::deepBlack : Theme::panel);
            g.fillRect (notes.getX(), notes.getY() + rh * (float) row, notes.getWidth(), rh - 1.0f);
            if (semi == 0) { g.setColour (Theme::neonPink.withAlpha (0.25f)); g.fillRect (notes.getX(), notes.getY() + rh * (float) row, notes.getWidth(), rh - 1.0f); }
        }
        for (int i = 0; i < 32; ++i)
        {
            const float x = notes.getX() + cw * (float) i;
            if (i >= len) { g.setColour (Theme::deepBlack.withAlpha (0.7f)); g.fillRect (x, notes.getY(), cw, notes.getHeight()); }
            if (i % 4 == 0) { g.setColour (Theme::electricPurple.withAlpha (0.4f)); g.drawVerticalLine ((int) x, notes.getY(), notes.getBottom()); }
            const auto& st = sp.steps[(size_t) i];
            if (st.on)
                for (int n : st.notes)
                    if (n != SeqStep::kNone && n >= -12 && n <= 12)
                    {
                        auto cell = juce::Rectangle<float> (x + 1.0f, notes.getY() + rh * (float) (12 - n), cw * (st.tie ? 1.0f : 0.9f), rh - 2.0f);
                        g.setColour (Theme::cyberCyan.withAlpha (0.4f + 0.6f * st.velocity));
                        g.fillRoundedRectangle (cell, 2.0f);
                    }
            if (i == cur && proc.getParam (ARP (arp::Mode)) > 1.5f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.2f));
                g.fillRect (x, notes.getY(), cw, notes.getHeight());
            }
            // lane
            auto bar = juce::Rectangle<float> (laneR.getX() + cw * (float) i + 1.0f, laneR.getY(), cw - 2.0f, laneR.getHeight());
            g.setColour (Theme::deepBlack);
            g.fillRect (bar);
            float v = laneMode == 0 ? st.velocity : laneMode == 1 ? st.gate / 2.0f : laneMode == 2 ? st.probability : 0.5f + 0.5f * st.mod;
            g.setColour (Theme::sunsetOrange.withAlpha (i < len ? 0.9f : 0.25f));
            if (laneMode == 3)
            {
                const float mid = bar.getCentreY();
                g.fillRect (bar.withY (std::min (mid, bar.getBottom() - bar.getHeight() * v)).withHeight (std::abs (v - 0.5f) * bar.getHeight()));
            }
            else
                g.fillRect (bar.withTop (bar.getBottom() - bar.getHeight() * v));
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { edit (e, true); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e, false); }
private:
    void edit (const juce::MouseEvent& e, bool click)
    {
        auto sp = proc.getPatch().getSeqPattern();
        auto r = getLocalBounds().toFloat();
        auto notes = r.removeFromTop (r.getHeight() * 0.72f);
        auto laneR = r.withTrimmedTop (6.0f);
        const float cw = notes.getWidth() / 32.0f, rh = notes.getHeight() / 25.0f;
        const int i = juce::jlimit (0, 31, (int) ((float) e.x / cw));
        auto& st = sp.steps[(size_t) i];
        if (notes.contains (e.position))
        {
            if (! click) return;
            const int semi = 12 - juce::jlimit (0, 24, (int) ((e.position.y - notes.getY()) / rh));
            if (e.mods.isRightButtonDown()) { st.tie = ! st.tie; }
            else
            {
                auto it = std::find (st.notes.begin(), st.notes.end(), semi);
                if (it != st.notes.end() && st.on)
                {
                    *it = SeqStep::kNone;
                    std::stable_partition (st.notes.begin(), st.notes.end(), [] (int n) { return n != SeqStep::kNone; });
                    st.on = st.notes[0] != SeqStep::kNone;
                    if (! st.on) st.notes[0] = 0;
                }
                else
                {
                    if (! st.on) { st.notes = { semi, SeqStep::kNone, SeqStep::kNone, SeqStep::kNone }; st.on = true; }
                    else
                    {
                        auto slot = std::find (st.notes.begin(), st.notes.end(), SeqStep::kNone);
                        if (slot != st.notes.end()) *slot = semi; else st.notes[3] = semi;
                    }
                }
            }
        }
        else if (laneR.contains (e.position))
        {
            const float v = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - laneR.getY()) / laneR.getHeight());
            if (laneMode == 0) st.velocity = v;
            else if (laneMode == 1) st.gate = juce::jlimit (0.05f, 2.0f, v * 2.0f);
            else if (laneMode == 2) st.probability = v;
            else st.mod = v * 2.0f - 1.0f;
        }
        proc.getPatch().setSeqPattern (sp);
        repaint();
    }
    ViceSynthProcessor& proc;
};

SeqPage::SeqPage (ViceSynthProcessor& p) : proc (p)
{
    addAndMakeVisible (box);
    controls.push_back (makeControl (p, ARP (arp::Mode), Theme::neonPink));
    controls.push_back (makeControl (p, ARP (arp::Latch), Theme::neonPink));
    for (int pi = 0; pi < seq::Count; ++pi) controls.push_back (makeControl (p, SEQ (pi), Theme::neonPink));
    for (auto& c : controls) addAndMakeVisible (*c);
    grid = std::make_unique<Grid> (p);
    addAndMakeVisible (*grid);
    lane.addItemList ({ "Lane: Velocity", "Lane: Gate", "Lane: Probability", "Lane: Mod (Seq Mod source)" }, 1);
    lane.setSelectedId (1, juce::dontSendNotification);
    lane.onChange = [this] { grid->laneMode = lane.getSelectedId() - 1; grid->repaint(); };
    addAndMakeVisible (lane);
    for (auto* b : { &presetsB, &randomB, &clearB }) addAndMakeVisible (b);
    grid->setTooltip ("Click to add/remove notes (up to 4 per step = chords). Right-click toggles tie. Edit the lane below. Notes are relative to C4 or the held key.");
    presetsB.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Bass pulse");
        m.addItem (2, "Minor arpeggio");
        m.addItem (3, "Chord stabs");
        m.addItem (4, "Driving 8ths");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsB), [this] (int r)
        {
            if (r <= 0) return;
            SeqPattern sp;
            const int arpNotes[] = { 0, 3, 7, 12, 7, 3, 0, -5 };
            for (int i = 0; i < 16; ++i)
            {
                auto& st = sp.steps[(size_t) i];
                st.on = true;
                st.notes = { 0, SeqStep::kNone, SeqStep::kNone, SeqStep::kNone };
                if (r == 1) { st.on = i % 2 == 0 || i == 7; st.notes[0] = (i == 7 || i == 14) ? 12 : 0; st.gate = 0.4f; }
                if (r == 2) { st.notes[0] = arpNotes[i % 8]; st.gate = 0.6f; }
                if (r == 3) { st.on = i % 4 == 0 || i == 6 || i == 10; st.notes = { 0, 3, 7, 10 }; st.gate = 0.3f; }
                if (r == 4) { st.notes[0] = i % 8 == 6 ? 3 : 0; st.velocity = i % 2 == 0 ? 1.0f : 0.6f; st.gate = 0.5f; }
                st.mod = std::sin ((float) i * 0.4f);
            }
            proc.getPatch().setSeqPattern (sp);
            proc.setParam (SEQ (seq::Length), 16.0f);
            grid->repaint();
        });
    };
    randomB.onClick = [this]
    {
        SeqPattern sp = proc.getPatch().getSeqPattern();
        juce::Random rnd;
        const int scale[] = { 0, 2, 3, 5, 7, 8, 10, 12 };
        for (auto& st : sp.steps)
        {
            st.on = rnd.nextFloat() > 0.3f;
            st.notes = { scale[rnd.nextInt (8)] - (rnd.nextFloat() > 0.8f ? 12 : 0), SeqStep::kNone, SeqStep::kNone, SeqStep::kNone };
            st.velocity = 0.5f + 0.5f * rnd.nextFloat();
            st.gate = 0.3f + 0.7f * rnd.nextFloat();
            st.probability = rnd.nextFloat() > 0.85f ? 0.5f : 1.0f;
            st.mod = rnd.nextFloat() * 2.0f - 1.0f;
            st.tie = false;
        }
        proc.getPatch().setSeqPattern (sp);
        grid->repaint();
    };
    clearB.onClick = [this]
    {
        SeqPattern sp;
        proc.getPatch().setSeqPattern (sp);
        grid->repaint();
    };
}

SeqPage::~SeqPage() = default;

void SeqPage::refresh() { grid->repaint(); }

void SeqPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    box.setBounds (r);
    auto c = box.content();
    gridLayout (controls, c.removeFromTop (64), (int) controls.size(), 64);
    c.removeFromTop (6);
    auto tools = c.removeFromTop (26);
    lane.setBounds (tools.removeFromLeft (220));
    tools.removeFromLeft (8);
    presetsB.setBounds (tools.removeFromLeft (110));
    tools.removeFromLeft (6);
    randomB.setBounds (tools.removeFromLeft (110));
    tools.removeFromLeft (6);
    clearB.setBounds (tools.removeFromLeft (90));
    c.removeFromTop (6);
    grid->setBounds (c);
}

// =================================================================================
MixerPage::MixerPage (ViceSynthProcessor& p) : scope (p), meter (p)
{
    auto addStrip = [&] (const juce::String& name, juce::Colour c, std::initializer_list<int> ids)
    {
        Strip s;
        s.box = std::make_unique<Section> (name, c);
        addAndMakeVisible (*s.box);
        for (int id : ids) { s.controls.push_back (makeControl (p, id, c)); addAndMakeVisible (*s.controls.back()); }
        strips.push_back (std::move (s));
    };
    for (int o = 0; o < kNumOscs; ++o)
        addStrip ("OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + o)), Theme::accentFor (o),
                  { O (o, osc::On), O (o, osc::Route), O (o, osc::Level), O (o, osc::Pan) });
    addStrip ("SUB", Theme::electricPurple, { SUB (sub::On), SUB (sub::Route), SUB (sub::Level), SUB (sub::Pan) });
    addStrip ("NOISE", Theme::electricPurple, { NOISE (noise::On), NOISE (noise::Route), NOISE (noise::Level), NOISE (noise::Pan) });
    addStrip ("FILTER 1", Theme::sunsetOrange, { F (0, flt::On), G (global::FilterRouting), F (0, flt::Mix), F (0, flt::Gain) });
    addStrip ("FILTER 2", Theme::neonPink, { F (1, flt::On), F (1, flt::Model), F (1, flt::Mix), F (1, flt::Gain) });
    addStrip ("FX", Theme::cyberCyan, { G (global::FxRouting), G (global::Quality), G (global::FxBalance), G (global::AmpVelocity) });
    addAndMakeVisible (scope);
    addAndMakeVisible (meter);
    master = std::make_unique<ParamKnob> (p, kPresetVolume, "PRESET VOLUME", Theme::neonPink);
    addAndMakeVisible (*master);
}

void MixerPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto right = r.removeFromRight (260);
    const int w = r.getWidth() / (int) strips.size();
    for (size_t i = 0; i < strips.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (int) i * w, r.getY(), w, r.getHeight()).reduced (3);
        strips[i].box->setBounds (cell);
        auto c = cell.reduced (8).withTrimmedTop (22);
        auto& ctl = strips[i].controls;
        ctl[0]->setBounds (c.removeFromTop (26));
        c.removeFromTop (6);
        ctl[1]->setBounds (c.removeFromTop (24));
        c.removeFromTop (10);
        const int kh = std::min (110, c.getHeight() / 2);
        ctl[2]->setBounds (c.removeFromTop (kh));
        ctl[3]->setBounds (c.removeFromTop (kh));
    }
    right.removeFromLeft (8);
    scope.setBounds (right.removeFromTop (right.getHeight() / 2).reduced (0, 4));
    auto bottom = right;
    meter.setBounds (bottom.removeFromRight (40).reduced (4, 10));
    master->setBounds (bottom.withSizeKeepingCentre (150, 170));
}

// =================================================================================
SettingsPage::SettingsPage (ViceSynthProcessor& p) : proc (p)
{
    for (auto* b : { &engineBox, &aiBox, &uiBox }) addAndMakeVisible (b);
    for (int id : { G (global::Quality), G (global::Polyphony), G (global::BendUp), G (global::BendDown), G (global::Transpose),
                    G (global::MasterTune), G (global::AmpVelocity), G (global::OscDrift), G (global::VoiceMode), G (global::GlideTime) })
    {
        engineControls.push_back (makeControl (p, id, Theme::sunsetOrange));
        addAndMakeVisible (*engineControls.back());
    }
    aiSettings = std::make_unique<AISettingsPanel> (p);
    addAndMakeVisible (*aiSettings);

    for (auto* b : { &saveSettingsButton, &resetSettingsButton }) addAndMakeVisible (b);
    saveSettingsButton.onClick = [this]
    {
        juce::String error;
        const bool ok = proc.saveSettings (error);
        juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
            ok ? "Settings saved" : "Settings not saved", ok ? "Interface and AI preferences saved. Patch and project master are unchanged." : error);
    };
    resetSettingsButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Reset settings?",
            "Restore interface defaults and offline AI mode? Presets, samples, current patch, master volume, credentials and host audio devices are preserved.",
            "RESET SETTINGS", "CANCEL", this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SettingsPage> (this)] (int result)
            {
                if (result == 0 || safe == nullptr) return;
                juce::String error;
                if (! safe->proc.resetSettings (error))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Settings not reset", error);
                else
                {
                    safe->aiSettings = std::make_unique<AISettingsPanel> (safe->proc);
                    safe->addAndMakeVisible (*safe->aiSettings);
                    if (safe->onSettingsReset) safe->onSettingsReset();
                    safe->resized();
                }
            }));
    };
    openGL.onClick = [this] { openGLEnabled = openGL.getToggleState(); if (onOpenGL) onOpenGL (openGLEnabled); };
    openGL.setTooltip ("Renders the interface with OpenGL. On Windows the default renderer already uses Direct2D (GPU accelerated).");
    addAndMakeVisible (openGL);
    sizeLabel.setText ("Interface size", juce::dontSendNotification);
    sizeLabel.setFont (Theme::font (12.0f, true));
    sizeLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (sizeLabel);
    for (int s : { 50, 60, 75, 85, 100, 125, 150, 175, 200 })
    {
        auto* b = scaleButtons.add (new juce::TextButton (juce::String (s) + "%"));
        b->onClick = [this, s] { if (onScale) onScale ((float) s / 100.0f); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &openPresets, &openWavetables, &openSamples, &restore }) addAndMakeVisible (b);
    openPresets.onClick = [] { PresetManager::rootFolder().getChildFile ("Presets").startAsProcess(); };
    openWavetables.onClick = [] { WavetableLibrary::instance().userFolder().startAsProcess(); };
    openSamples.onClick = [] { SampleLibrary::instance().userFolder().startAsProcess(); };
    restore.onClick = [this]
    {
        PatchSpec spec;
        if (proc.getPresetManager().readRecovery (spec)) proc.applyPatch (spec, "Restore last session");
    };
    about.setFont (Theme::font (11.5f));
    about.setColour (juce::Label::textColourId, Theme::textDim);
    about.setJustificationType (juce::Justification::topLeft);
    about.setText (juce::String ("VICE SYNTH X ") + VSX_VERSION_STRING + "\nOriginal synthesis engine, filters, artwork, wavetables, samples and presets.\n"
                   "Built with JUCE (AGPLv3 / commercial licence) - see LICENSES.md.\nFilter models are inspired by classic circuits; "
                   "product names are trademarks of their owners and are used only descriptively.",
                   juce::dontSendNotification);
    addAndMakeVisible (about);
    startTimerHz (2);
}

SettingsPage::~SettingsPage() { stopTimer(); }

void SettingsPage::timerCallback()
{
    openGL.setToggleState (openGLEnabled, juce::dontSendNotification);
}

void SettingsPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto left = r.removeFromLeft (r.getWidth() * 27 / 100);
    engineBox.setBounds (left.reduced (3));
    gridLayout (engineControls, engineBox.content().translated (engineBox.getX(), engineBox.getY()).withTrimmedTop (6), 3, 72);

    auto mid = r.removeFromLeft (r.getWidth() * 66 / 100);
    aiBox.setBounds (mid.reduced (3));
    aiSettings->setBounds (aiBox.content().translated (aiBox.getX(), aiBox.getY()).reduced (6, 4));

    uiBox.setBounds (r.reduced (3));
    auto u = uiBox.content().translated (uiBox.getX(), uiBox.getY()).reduced (6);
    sizeLabel.setBounds (u.removeFromTop (20));
    // two rows of size buttons (same nine sizes as the header SIZE menu)
    for (int row = 0; row < 2; ++row)
    {
        auto sb = u.removeFromTop (28);
        u.removeFromTop (4);
        const int perRow = row == 0 ? 5 : scaleButtons.size() - 5;
        const int bw = sb.getWidth() / 5;
        for (int i = 0; i < perRow; ++i) scaleButtons[row * 5 + i]->setBounds (sb.removeFromLeft (bw).reduced (2, 0));
    }
    u.removeFromTop (8);
    openGL.setBounds (u.removeFromTop (24));
    u.removeFromTop (12);
    for (auto* b : { &openPresets, &openWavetables, &openSamples, &restore }) { b->setBounds (u.removeFromTop (28)); u.removeFromTop (6); }
    u.removeFromTop (8);
    saveSettingsButton.setBounds (u.removeFromTop (28));
    u.removeFromTop (6);
    resetSettingsButton.setBounds (u.removeFromTop (28));
    u.removeFromTop (8);
    about.setBounds (u);
}

} // namespace vsx
