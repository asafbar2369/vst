#include "MacroPerformance.h"
#include "../Synth/Macros.h"

namespace vsx
{
using namespace pid;

MacroStrip::MacroStrip (ViceSynthProcessor& p)
{
    for (int m = 0; m < kNumMacros; ++m)
    {
        knobs.push_back (std::make_unique<ParamKnob> (p, MACRO (m), Macros::name (m), Theme::macroAccent (m), ParamKnob::Style::Macro));
        knobs.back()->setTooltip (Macros::describe (m) + "\nAlso available as a modulation source (drag targets in SOUND DESIGN).");
        addAndMakeVisible (*knobs.back());
    }
    addAndMakeVisible (vignette);
    vignette.setInterceptsMouseClicks (false, false);
}

void MacroStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::card (g, r, 10.0f);
    Icons::draw (g, Icons::Type::Macro, juce::Rectangle<float> (26.0f, r.getCentreY() - 14.0f, 28.0f, 28.0f), Theme::text, 1.8f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (18.0f, true));
    g.drawText ("MACROS", juce::Rectangle<float> (64.0f, r.getCentreY() - 14.0f, 140.0f, 28.0f), juce::Justification::left);
    g.setColour (Theme::border);
    g.drawVerticalLine (200, r.getY() + 14.0f, r.getBottom() - 14.0f);
}

void MacroStrip::paintOverChildren (juce::Graphics& g)
{
    // circled macro numbers in front of each label
    for (int m = 0; m < (int) knobs.size(); ++m)
    {
        const auto kb = knobs[(size_t) m]->getBounds();
        const float tw = juce::GlyphArrangement::getStringWidth (Theme::font (13.5f, true), Macros::name (m));
        const auto c = juce::Rectangle<float> (15.0f, 15.0f).withCentre ({ (float) kb.getCentreX() - tw * 0.5f - 13.0f, (float) kb.getY() + 10.0f });
        g.setColour (Theme::textDim);
        g.drawEllipse (c, 1.0f);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (juce::String (m + 1), c, juce::Justification::centred);
    }
    auto v = vignette.getBounds().toFloat();
    juce::ColourGradient fade (Theme::panelLight, v.getX(), 0.0f, Theme::panelLight.withAlpha (0.0f), v.getX() + v.getWidth() * 0.35f, 0.0f, false);
    g.setGradientFill (fade);
    g.fillRect (v.withWidth (v.getWidth() * 0.36f));
    MiamiArtwork::drawViceScript (g, v.withTrimmedLeft (v.getWidth() * 0.3f).withTrimmedTop (v.getHeight() * 0.05f).withHeight (v.getHeight() * 0.6f));
}

void MacroStrip::resized()
{
    auto r = getLocalBounds().reduced (8);
    vignette.setBounds (r.removeFromRight (r.getWidth() * 15 / 100).reduced (0, 0));
    r.removeFromLeft (200);
    const int w = r.getWidth() / (int) knobs.size();
    for (auto& k : knobs) k->setBounds (r.removeFromLeft (w).reduced (6, 0));
}

// =================================================================================
VsxKeyboard::VsxKeyboard (juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent (s, horizontalKeyboard)
{
    setAvailableRange (0, 127);
    setLowestVisibleKey (36);
    setScrollButtonsVisible (true);
    setOctaveForMiddleC (4);
    setWantsKeyboardFocus (false); // never steal host shortcuts
}

void VsxKeyboard::resized()
{
    setKeyWidth ((float) getWidth() / 28.0f); // 4 octaves = 28 white keys
    juce::MidiKeyboardComponent::resized();
}

void VsxKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> a, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    const float pos = (float) (note - 36) / 48.0f;
    const auto tint = Theme::neonPink.interpolatedWith (Theme::cyberCyan, pos);
    auto k = a.reduced (0.6f, 0.0f);
    juce::ColourGradient body (juce::Colour (0xfff4f1fa), k.getX(), k.getY(), juce::Colour (0xffdcd6ea).interpolatedWith (tint, 0.18f), k.getX(), k.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (k.withTrimmedTop (-4.0f), 3.0f);
    // reflection of the neon light along the lower part of the key
    juce::ColourGradient refl (tint.withAlpha (0.0f), k.getX(), k.getY() + k.getHeight() * 0.55f, tint.withAlpha (0.35f), k.getX(), k.getBottom(), false);
    g.setGradientFill (refl);
    g.fillRect (k.withTrimmedTop (k.getHeight() * 0.55f));
    if (isDown || isOver)
    {
        g.setColour ((isDown ? Theme::neonPink : Theme::cyberCyan).withAlpha (isDown ? 0.55f : 0.18f));
        g.fillRoundedRectangle (k, 3.0f);
    }
    g.setColour (juce::Colour (0xff7d7690));
    g.drawRoundedRectangle (k, 3.0f, 0.8f);
    if (note % 12 == 0)
    {
        g.setColour (juce::Colour (0xff6c6880));
        g.setFont (Theme::font (10.0f, true));
        g.drawText ("C" + juce::String (note / 12 - 1), k.removeFromBottom (16.0f), juce::Justification::centred);
    }
}

void VsxKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> a, bool isDown, bool isOver, juce::Colour)
{
    auto k = a.reduced (a.getWidth() * 0.06f, 0.0f);
    juce::ColourGradient body (juce::Colour (0xff2a2733), k.getX(), k.getY(), juce::Colour (0xff07070b), k.getX(), k.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (k.withTrimmedTop (-4.0f), 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRoundedRectangle (k.reduced (k.getWidth() * 0.18f, 2.0f).withTrimmedBottom (k.getHeight() * 0.35f), 2.0f);
    if (isDown || isOver)
    {
        g.setColour ((isDown ? Theme::neonPink : Theme::cyberCyan).withAlpha (isDown ? 0.7f : 0.25f));
        g.fillRoundedRectangle (k, 2.5f);
    }
}

// =================================================================================
class PerformancePanel::WheelControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    WheelControl (ViceSynthProcessor& p, bool isPitch) : proc (p), pitch (isPitch)
    {
        setTooltip (pitch ? "Pitch wheel (springs back to centre)" : "Modulation wheel (MIDI CC 1)");
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        auto slot = r.withTrimmedBottom (18.0f);
        Theme::displayWell (g, slot, 5.0f);
        auto wheel = slot.reduced (4.0f);
        juce::ColourGradient grad (juce::Colour (0xff15171e), wheel.getX(), wheel.getY(), juce::Colour (0xff15171e), wheel.getX(), wheel.getBottom(), false);
        grad.addColour (0.5, juce::Colour (0xff3a3d48));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (wheel, 4.0f);
        const float norm = pitch ? 0.5f + 0.5f * value : value;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (int i = 0; i < 14; ++i)
        {
            const float y = wheel.getY() + std::fmod ((float) i / 14.0f + (1.0f - norm) * 0.5f, 1.0f) * wheel.getHeight();
            g.drawHorizontalLine ((int) y, wheel.getX() + 2.0f, wheel.getRight() - 2.0f);
        }
        const float y = wheel.getBottom() - norm * wheel.getHeight();
        g.setColour (pitch ? Theme::cyberCyan : Theme::neonPink);
        g.fillRoundedRectangle (wheel.getX() + 2.0f, juce::jlimit (wheel.getY(), wheel.getBottom() - 4.0f, y - 2.0f), wheel.getWidth() - 4.0f, 4.0f, 2.0f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.5f, true));
        g.drawText (pitch ? "PITCH" : "MOD", r.removeFromBottom (14.0f), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override { startValue = value; mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float range = (float) getHeight() - 26.0f;
        set (juce::jlimit (pitch ? -1.0f : 0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() / range * (pitch ? 2.0f : 1.0f)));
    }
    void mouseUp (const juce::MouseEvent&) override { if (pitch) set (0.0f); }
    void set (float v)
    {
        value = v;
        if (pitch) proc.injectMidi (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) (8192.0f + v * 8191.0f))));
        else proc.injectMidi (juce::MidiMessage::controllerEvent (1, 1, juce::jlimit (0, 127, (int) (v * 127.0f))));
        repaint();
    }
private:
    ViceSynthProcessor& proc;
    bool pitch;
    float value = 0.0f, startValue = 0.0f;
};

PerformancePanel::PerformancePanel (ViceSynthProcessor& p) : proc (p), keyboard (p.getKeyboardState())
{
    pitch = std::make_unique<WheelControl> (p, true);
    mod = std::make_unique<WheelControl> (p, false);
    addAndMakeVisible (*pitch);
    addAndMakeVisible (*mod);
    for (int b = 1; b <= 24; ++b) bend.addItem (juce::String::fromUTF8 ("\xc2\xb1") + juce::String (b), b);
    for (int v = 1; v <= kMaxVoices; ++v) poly.addItem (juce::String (v), v);
    for (int u = 1; u <= kMaxUnison; ++u) unison.addItem (juce::String (u), u);
    bend.setTooltip ("Pitch-bend range in semitones (up and down)");
    poly.setTooltip ("Maximum number of voices");
    unison.setTooltip ("Unison voices for every active oscillator (per-oscillator values on SOUND DESIGN > OSCILLATORS)");
    bend.onChange = [this] { if (syncing) return; proc.setParam (G (global::BendUp), (float) bend.getSelectedId()); proc.setParam (G (global::BendDown), (float) bend.getSelectedId()); };
    poly.onChange = [this] { if (! syncing) proc.setParam (G (global::Polyphony), (float) poly.getSelectedId()); };
    unison.onChange = [this]
    {
        if (syncing) return;
        for (int o = 0; o < kNumOscs; ++o)
            if (proc.getParam (O (o, osc::On)) > 0.5f) proc.setParam (O (o, osc::Unison), (float) unison.getSelectedId());
    };
    for (auto* c : { &bend, &poly, &unison }) addAndMakeVisible (c);
    glide = std::make_unique<ParamKnob> (p, G (global::GlideTime), "GLIDE", Theme::electricPurple, ParamKnob::Style::ValueBox);
    addAndMakeVisible (*glide);
    hold.setClickingTogglesState (true);
    hold.setColour (juce::TextButton::buttonOnColourId, Theme::neonPink);
    hold.setTooltip ("Hold (sustain latch): notes keep sounding until HOLD is released or PANIC");
    hold.onClick = [this] { proc.setHold (hold.getToggleState()); };
    addAndMakeVisible (hold);
    addAndMakeVisible (keyboard);
    keyboard.onSampleDrop = [this] (const juce::StringArray& files)
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("LOAD SAMPLE - TARGET OSCILLATOR");
        for (int o = 0; o < kNumOscs; ++o)
            menu.addItem (o + 1, "Oscillator " + juce::String::charToString ((juce::juce_wchar) ('A' + o)));
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyboard),
                           [safe = juce::Component::SafePointer<PerformancePanel> (this), files] (int result)
        {
            if (safe == nullptr || result <= 0 || result > kNumOscs) return;
            safe->proc.importSampleFiles (result - 1, files);
        });
    };
    timerCallback();
    startTimerHz (5);
}

PerformancePanel::~PerformancePanel() { stopTimer(); }

void PerformancePanel::timerCallback()
{
    syncing = true;
    bend.setSelectedId ((int) proc.getParam (G (global::BendUp)), juce::dontSendNotification);
    poly.setSelectedId ((int) proc.getParam (G (global::Polyphony)), juce::dontSendNotification);
    unison.setSelectedId ((int) proc.getParam (O (0, osc::Unison)), juce::dontSendNotification);
    hold.setToggleState (proc.isHold(), juce::dontSendNotification);
    syncing = false;
}

void PerformancePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    Theme::card (g, r, 10.0f);
    Icons::draw (g, Icons::Type::Keyboard, juce::Rectangle<float> (22.0f, 18.0f, 26.0f, 22.0f), Theme::text, 1.5f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (15.0f, true));
    g.drawText ("PERFORMANCE", juce::Rectangle<float> (58.0f, 16.0f, 200.0f, 26.0f), juce::Justification::left);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    auto label = [&] (juce::Component& c, const char* t) { g.drawText (t, c.getBounds().translated (0, -18).withHeight (16), juce::Justification::centred); };
    label (bend, "BEND"); label (poly, "POLY"); label (unison, "UNISON");
}

void PerformancePanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto left = r.removeFromLeft (560);
    auto wheels = left.removeFromLeft (120).withTrimmedTop (36);
    pitch->setBounds (wheels.removeFromLeft (56).reduced (8, 0));
    mod->setBounds (wheels.removeFromLeft (56).reduced (8, 0));
    auto ctl = left.withTrimmedTop (48).withHeight (40);
    ctl.removeFromLeft (10);
    const int w = 82;
    bend.setBounds (ctl.removeFromLeft (w).reduced (4, 4));
    glide->setBounds (ctl.removeFromLeft (w).withY (getLocalBounds().getY() + 14).withBottom (ctl.getBottom() + 22).reduced (4, 0));
    poly.setBounds (ctl.removeFromLeft (w).reduced (4, 4));
    unison.setBounds (ctl.removeFromLeft (w).reduced (4, 4));
    hold.setBounds (ctl.removeFromLeft (w).reduced (8, 6));
    keyboard.setBounds (r.reduced (4, 0));
}

} // namespace vsx
