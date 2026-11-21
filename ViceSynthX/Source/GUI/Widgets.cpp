#include "Widgets.h"

namespace vsx
{
// =================================================================================
class ParamKnob::KnobSlider : public juce::Slider
{
public:
    explicit KnobSlider (ParamKnob& o) : owner (o) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { owner.showMenu(); return; }
        if (e.mods.isAltDown() && owner.activeSlot >= 0)
        {
            modDrag = true;
            startAmount = owner.proc.getParam (pid::MODAMT (owner.activeSlot));
            return;
        }
        modDrag = false;
        juce::Slider::mouseDown (e);
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        const double step = (getMaximum() - getMinimum()) / (k.getModifiers().isShiftDown() ? 1000.0 : 100.0);
        const double inc = std::max (step, getInterval());
        if (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey) { setValue (getValue() + inc); return true; }
        if (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey) { setValue (getValue() - inc); return true; }
        return false;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        setMouseDragSensitivity (e.mods.isShiftDown() ? 1400 : 250);
        if (modDrag)
        {
            const float amt = juce::jlimit (-1.0f, 1.0f, startAmount - (float) e.getDistanceFromDragStartY() / 150.0f);
            owner.proc.setParam (pid::MODAMT (owner.activeSlot), amt);
            owner.refreshModulation();
            return;
        }
        juce::Slider::mouseDrag (e);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (modDrag) { modDrag = false; return; }
        juce::Slider::mouseUp (e);
    }

private:
    ParamKnob& owner;
    bool modDrag = false;
    float startAmount = 0.0f;
};

juce::Array<ParamKnob*>& ParamKnob::registry()
{
    static juce::Array<ParamKnob*> knobs;
    return knobs;
}

ParamKnob::ParamKnob (ViceSynthProcessor& p, int paramIndex, const juce::String& text, juce::Colour accent, Style st)
    : proc (p), index (paramIndex), style (st)
{
    const auto& def = ParamRegistry::get()[paramIndex];
    slider = std::make_unique<KnobSlider> (*this);
    slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider->setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider->setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider->setPopupDisplayEnabled (style == Style::Compact || style == Style::Macro, true, nullptr, 1500);
    slider->setWantsKeyboardFocus (true);
    slider->setScrollWheelEnabled (true);
    slider->getProperties().set ("bipolar", def.minValue < 0.0f && def.maxValue > 0.0f);
    slider->setMouseDragSensitivity (220);
    slider->setVelocityBasedMode (false);
    addAndMakeVisible (*slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.getAPVTS(), def.id, *slider);
    slider->setDoubleClickReturnValue (true, def.defaultValue);

    label.setText (text.isNotEmpty() ? text : def.name.fromLastOccurrenceOf (" ", false, false), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (Theme::font (style == Style::Macro ? 13.5f : 11.0f, style == Style::Macro));
    label.setColour (juce::Label::textColourId, style == Style::Macro ? Theme::text : Theme::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    if (style == Style::ValueBox)
    {
        valueBox.setJustificationType (juce::Justification::centred);
        valueBox.setFont (Theme::font (11.0f, true));
        valueBox.setColour (juce::Label::textColourId, Theme::text);
        valueBox.setColour (juce::Label::backgroundColourId, Theme::display);
        valueBox.setColour (juce::Label::outlineColourId, Theme::border);
        valueBox.setEditable (false, true, false);
        valueBox.setTooltip ("Double-click to type an exact value");
        valueBox.onTextChange = [this] { slider->setValue (slider->getValueFromText (valueBox.getText())); };
        addAndMakeVisible (valueBox);
        auto update = [this] { valueBox.setText (slider->getTextFromValue (slider->getValue()), juce::dontSendNotification); };
        slider->onValueChange = update;
        update();
    }
    setTooltip (def.name + ": " + def.description + (def.modulatable ? "\nDrop a modulation source here; Alt-drag edits the modulation depth; right-click for options." : juce::String()));
    registry().add (this);
}

ParamKnob::~ParamKnob()
{
    registry().removeFirstMatchingValue (this);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    if (style == Style::Macro)
    {
        label.setBounds (r.removeFromTop (20));
        slider->setBounds (r);
        return;
    }
    if (style == Style::ValueBox)
    {
        auto vb = r.removeFromBottom (17);
        valueBox.setBounds (vb.withSizeKeepingCentre (std::min (vb.getWidth() - 4, 58), 17));
        r.removeFromBottom (1);
    }
    label.setBounds (r.removeFromBottom (14));
    slider->setBounds (r);
}

juce::Slider& ParamKnob::getSlider() { return *slider; }

void ParamKnob::setTooltipExtra (const juce::String& extra)
{
    const auto& def = ParamRegistry::get()[index];
    setTooltip (def.name + ": " + def.description + "\n" + extra);
}

void ParamKnob::paint (juce::Graphics& g)
{
    if (dragHover)
    {
        g.setColour (Theme::cyberCyan.withAlpha (0.25f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }
}

void ParamKnob::paintOverChildren (juce::Graphics& g)
{
    if (learning)
    {
        // MIDI Learn: waiting for a controller
        auto sb = slider->getBounds().toFloat();
        const float d = std::min (sb.getWidth(), sb.getHeight());
        g.setColour (Theme::cyberCyan);
        g.drawEllipse (juce::Rectangle<float> (d, d).withCentre (sb.getCentre()).reduced (0.5f), 2.0f);
        g.setFont (Theme::font (9.0f, true));
        g.drawText ("LEARN", sb.withHeight (10.0f).translated (0.0f, -2.0f), juce::Justification::centred);
    }
    if (! aiHighlight) return;
    auto sb = slider->getBounds().toFloat();
    const float d = std::min (sb.getWidth(), sb.getHeight());
    auto ring = juce::Rectangle<float> (d, d).withCentre (sb.getCentre()).reduced (1.0f);
    g.setColour (Theme::neonPink.withAlpha (0.85f));
    juce::Path p;
    p.addEllipse (ring);
    juce::PathStrokeType (1.4f).createDashedStroke (p, p, std::array<float, 2> { 3.0f, 3.0f }.data(), 2);
    g.fillPath (p);
    auto badge = juce::Rectangle<float> (15.0f, 11.0f).withPosition (ring.getRight() - 11.0f, ring.getY() - 2.0f);
    g.setColour (Theme::neonPink);
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (juce::Colours::white);
    g.setFont (Theme::font (8.0f, true));
    g.drawText ("AI", badge, juce::Justification::centred);
}

void ParamKnob::refreshModulation()
{
    const bool nowLearning = proc.getMidiLearnTarget() == index;
    const int cc = proc.ccForParam (index);
    if (nowLearning != learning || cc != learnedCc)
    {
        learning = nowLearning;
        learnedCc = cc;
        // keep the existing tooltip, replace only the trailing "MIDI CC n" line
        auto tip = getTooltip();
        if (tip.contains ("\nMIDI CC ")) tip = tip.upToLastOccurrenceOf ("\nMIDI CC ", false, false);
        setTooltip (cc >= 0 ? tip + "\nMIDI CC " + juce::String (cc) : tip);
        repaint();
    }
    const auto slots = proc.slotsTargeting (index);
    float depth = 0.0f;
    activeSlot = slots.isEmpty() ? -1 : (activeSlot >= 0 && slots.contains (activeSlot) ? activeSlot : slots.getLast());
    for (int s : slots) depth += proc.getParam (pid::MODAMT (s));
    const float live = slots.isEmpty() ? -1.0f : proc.getEngine().modDisplay[(size_t) index].load();
    if (std::abs (depth - lastDepth) > 1.0e-4f || std::abs (live - lastLive) > 2.0e-3f)
    {
        lastDepth = depth;
        lastLive = live;
        slider->getProperties().set ("modDepth", depth);
        slider->getProperties().set ("modLive", live);
        slider->repaint();
    }
}

bool ParamKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return ParamRegistry::get()[index].modulatable && d.description.toString().startsWith ("modsrc:");
}

void ParamKnob::itemDropped (const SourceDetails& d)
{
    dragHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("modsrc:", false, false).getIntValue();
    const float amount = modsrc::isBipolar (src) ? 0.15f : 0.3f;
    activeSlot = proc.addModulation (src, index, amount);
    refreshModulation();
    repaint();
}

void ParamKnob::showMenu()
{
    const auto& def = ParamRegistry::get()[index];
    juce::PopupMenu m;
    m.addSectionHeader (def.name);
    m.addItem (1, "Reset to default");
    {
        const int cc = proc.ccForParam (index);
        if (proc.getMidiLearnTarget() == index) m.addItem (6, "Cancel MIDI Learn");
        else m.addItem (6, cc >= 0 ? "MIDI Learn (now CC " + juce::String (cc) + ")" : "MIDI Learn");
        if (cc >= 0) m.addItem (7, "Forget MIDI CC " + juce::String (cc));
    }
    if (def.modulatable)
    {
        juce::PopupMenu add;
        for (int s = 1; s < modsrc::Count; ++s) add.addItem (1000 + s, modsrc::name (s));
        m.addSubMenu ("Add modulation", add);
        const auto slots = proc.slotsTargeting (index);
        for (int slot : slots)
        {
            juce::PopupMenu sm;
            const int src = proc.getPatch().slots[(size_t) slot].source.load();
            for (int a : { -100, -50, -25, -10, 10, 25, 50, 100 })
                sm.addItem (2000 + slot * 100 + (a + 100) / 5, "Amount " + juce::String (a) + " %");
            sm.addSeparator();
            sm.addItem (5000 + slot, "Remove");
            m.addSubMenu ("Mod " + juce::String (slot + 1) + ": " + modsrc::name (src) + " (" + juce::String ((int) (proc.getParam (pid::MODAMT (slot)) * 100.0f)) + "%)", sm);
        }
    }
    juce::Component::SafePointer<ParamKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        const auto& d = ParamRegistry::get()[self.index];
        if (r == 1) self.proc.setParam (self.index, d.defaultValue);
        else if (r == 6)
        {
            if (self.proc.getMidiLearnTarget() == self.index) self.proc.cancelMidiLearn();
            else self.proc.startMidiLearn (self.index);
        }
        else if (r == 7) self.proc.forgetMidiCc (self.index);
        else if (r >= 1000 && r < 2000) self.activeSlot = self.proc.addModulation (r - 1000, self.index, modsrc::isBipolar (r - 1000) ? 0.15f : 0.3f);
        else if (r >= 2000 && r < 5000)
        {
            const int slot = (r - 2000) / 100;
            const int a = ((r - 2000) % 100) * 5 - 100;
            self.proc.setParam (pid::MODAMT (slot), (float) a / 100.0f);
        }
        else if (r >= 5000) self.proc.removeModulation (r - 5000);
        self.refreshModulation();
    });
}

// =================================================================================
ParamToggle::ParamToggle (ViceSynthProcessor& p, int paramIndex, const juce::String& text, juce::Colour c)
{
    const auto& def = ParamRegistry::get()[paramIndex];
    setButtonText (text.isNotEmpty() ? text : def.name);
    setColour (juce::ToggleButton::tickColourId, c);
    setTooltip (def.description);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.getAPVTS(), def.id, *this);
}

ParamCombo::ParamCombo (ViceSynthProcessor& p, int paramIndex)
{
    const auto& def = ParamRegistry::get()[paramIndex];
    addItemList (def.choices, 1);
    setTooltip (def.name + ": " + def.description);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getAPVTS(), def.id, *this);
}

// =================================================================================
ModSourceChip::ModSourceChip (int s) : source (s)
{
    setTooltip ("Drag " + modsrc::name (s) + " onto any knob to modulate it.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModSourceChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = source >= modsrc::Lfo1 && source <= modsrc::Lfo10 ? Theme::cyberCyan
                 : source >= modsrc::Env1 && source <= modsrc::Env4 ? Theme::neonPink
                 : source >= modsrc::Macro1 && source <= modsrc::Macro8 ? Theme::sunsetOrange : Theme::electricPurple;
    g.setColour (c.withAlpha (0.18f));
    g.fillRoundedRectangle (r, 5.0f);
    if (liveValue)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, liveValue());
        g.setColour (c.withAlpha (0.5f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * (modsrc::isBipolar (source) ? 0.5f + 0.5f * v : std::abs (v))), 5.0f);
    }
    g.setColour (c);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (modsrc::name (source), r, juce::Justification::centred, true);
}

void ModSourceChip::mouseDrag (const juce::MouseEvent&)
{
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! dnd->isDragAndDropActive())
            dnd->startDragging ("modsrc:" + juce::String (source), this);
}

// =================================================================================
void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float levels[2] = { proc.outputPeakL.load(), proc.outputPeakR.load() };
    const float w = r.getWidth() * 0.5f - 1.0f;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (r.getX() + (float) c * (w + 2.0f), r.getY(), w, r.getHeight());
        g.setColour (Theme::deepBlack);
        g.fillRoundedRectangle (bar, 2.0f);
        const float db = gainToDb (levels[c]);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * frac);
        juce::ColourGradient grad (Theme::cyberCyan, bar.getX(), bar.getBottom(), Theme::neonPink, bar.getX(), bar.getY(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);
    }
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::glassPanel (g, r, 10.0f, accent);
    g.setColour (accent.brighter (0.5f));
    g.setFont (Theme::displayFont (12.5f));
    g.drawText (title, r.reduced (10.0f, 4.0f).withHeight (16.0f), juce::Justification::centredLeft, true);
}

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, int gap)
{
    const int n = (int) comps.size();
    if (n == 0) return;
    const int w = (area.getWidth() - gap * (n - 1)) / n;
    for (auto* c : comps)
    {
        if (c != nullptr) c->setBounds (area.removeFromLeft (w));
        area.removeFromLeft (gap);
    }
}

} // namespace vsx
