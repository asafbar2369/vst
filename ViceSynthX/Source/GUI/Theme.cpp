#include "Theme.h"

namespace vsx
{
namespace Theme
{
    juce::Font font (float height, bool bold)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font displayFont (float height)
    {
        return juce::Font (juce::FontOptions (height, juce::Font::bold)).withExtraKerningFactor (0.08f);
    }

    void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour accent)
    {
        juce::ColourGradient fill (panelLight.withAlpha (0.92f), r.getX(), r.getY(), darkPurple.withAlpha (0.95f), r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, corner);
        // top sheen
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + 26.0f, false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (r.withHeight (std::min (26.0f, r.getHeight())), corner);
        // neon border
        juce::ColourGradient border (accent.withAlpha (0.75f), r.getX(), r.getY(), cyberCyan.withAlpha (0.35f), r.getRight(), r.getBottom(), false);
        g.setGradientFill (border);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.2f);
    }

    void neonText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c, float height, juce::Justification j)
    {
        g.setFont (displayFont (height));
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (c.withAlpha (0.08f * (float) (4 - i)));
            g.drawText (t, r.translated (0.0f, 0.0f).expanded ((float) i), j, false);
        }
        g.setColour (c.brighter (0.4f));
        g.drawText (t, r, j, false);
    }

    juce::Colour accentFor (int i)
    {
        switch (((i % 4) + 4) % 4)
        {
            case 0: return neonPink;
            case 1: return cyberCyan;
            case 2: return sunsetOrange;
            default: return electricPurple;
        }
    }
}

VsxLookAndFeel::VsxLookAndFeel()
{
    using namespace Theme;
    setColour (juce::ResizableWindow::backgroundColourId, deepBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, darkPurple);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, cyberCyan);
    setColour (juce::PopupMenu::backgroundColourId, darkPurple);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, electricPurple.withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, panelLight);
    setColour (juce::TextButton::buttonOnColourId, neonPink);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, deepBlack.withAlpha (0.8f));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, electricPurple.withAlpha (0.5f));
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, cyberCyan);
    setColour (juce::CaretComponent::caretColourId, cyberCyan);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, electricPurple);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, cyberCyan);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff3eefc));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, deepBlack);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, darkPurple);
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cyberCyan.withAlpha (0.35f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, neonPink.withAlpha (0.75f));
    setColour (juce::MidiKeyboardComponent::shadowColourId, electricPurple.withAlpha (0.4f));
    setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, darkPurple);
    setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, cyberCyan);
}

void VsxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    using namespace Theme;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float arcR = radius - 3.0f;

    // outer track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (deepBlack.withAlpha (0.9f));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc with glow (bipolar knobs fill from the centre)
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    const float to = start + pos * (end - start);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, to), std::max (from, to), true);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // modulation ring: stored by ParamKnob as properties
    const float modDepth = (float) (double) s.getProperties().getWithDefault ("modDepth", 0.0);
    if (std::abs (modDepth) > 1.0e-4f)
    {
        const float mr = radius - 0.5f;
        const float a0 = juce::jlimit (start, end, to);
        const float a1 = juce::jlimit (start, end, to + modDepth * (end - start));
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, mr, mr, 0.0f, std::min (a0, a1), std::max (a0, a1), true);
        g.setColour (cyberCyan.withAlpha (0.9f));
        g.strokePath (ring, juce::PathStrokeType (2.0f));
        const float live = (float) (double) s.getProperties().getWithDefault ("modLive", -1.0);
        if (live >= 0.0f)
        {
            const float la = start + live * (end - start);
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ c.x + mr * std::sin (la), c.y - mr * std::cos (la) }));
        }
    }

    // knob cap: brushed metal with neon rim
    const float capR = arcR - 5.0f;
    juce::ColourGradient cap (juce::Colour (0xff3a3355), c.x - capR, c.y - capR, juce::Colour (0xff0f0c1c), c.x + capR, c.y + capR, false);
    g.setGradientFill (cap);
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    g.setColour (accent.withAlpha (0.45f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.0f);

    // pointer
    const float ang = to;
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.4f, -capR + 2.0f, 2.8f, capR * 0.55f, 1.4f);
    g.setColour (juce::Colours::white.withAlpha (s.isEnabled() ? 0.95f : 0.4f));
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void VsxLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace Theme;
    const auto accent = s.findColour (juce::Slider::trackColourId);
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (vertical)
    {
        auto track = r.withSizeKeepingCentre (6.0f, r.getHeight());
        g.setColour (deepBlack);
        g.fillRoundedRectangle (track, 3.0f);
        auto fill = track.withTop (pos);
        g.setColour (accent);
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (juce::Rectangle<float> (r.getWidth() * 0.8f, 6.0f).withCentre ({ r.getCentreX(), pos }), 3.0f);
    }
    else
    {
        auto track = r.withSizeKeepingCentre (r.getWidth(), 5.0f);
        g.setColour (deepBlack);
        g.fillRoundedRectangle (track, 2.5f);
        g.setColour (accent);
        g.fillRoundedRectangle (track.withRight (pos), 2.5f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ pos, r.getCentreY() }));
    }
}

void VsxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace Theme;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    juce::ColourGradient grad (on ? accent.withAlpha (0.85f) : panelLight.brighter (over ? 0.15f : 0.0f), r.getX(), r.getY(),
                               on ? accent.darker (0.5f) : darkPurple, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour ((on ? accent : electricPurple).withAlpha (down ? 1.0f : (over ? 0.9f : 0.55f)));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void VsxLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 1), juce::Justification::centred, 1);
}

void VsxLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace Theme;
    auto r = b.getLocalBounds().toFloat();
    const auto led = r.removeFromLeft (std::min (r.getHeight(), 18.0f)).withSizeKeepingCentre (11.0f, 11.0f);
    const auto c = b.findColour (juce::ToggleButton::tickColourId);
    if (b.getToggleState())
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
        g.setColour (c);
    }
    else
        g.setColour (deepBlack);
    g.fillEllipse (led);
    g.setColour (c.withAlpha (over ? 0.9f : 0.6f));
    g.drawEllipse (led, 1.0f);
    g.setColour (text.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (font (12.0f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft, true);
}

void VsxLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace Theme;
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (deepBlack.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? cyberCyan : outline.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (cyberCyan);
    g.fillPath (arrow);
}

juce::Font VsxLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return Theme::font (std::min (13.0f, (float) b.getHeight() * 0.62f)); }
juce::Font VsxLookAndFeel::getPopupMenuFont() { return Theme::font (14.0f); }
juce::Font VsxLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return Theme::font (std::min (13.5f, (float) h * 0.55f), true); }
juce::Font VsxLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void VsxLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void VsxLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Theme::darkPurple);
    g.setColour (Theme::electricPurple.withAlpha (0.7f));
    g.drawRect (0, 0, w, h, 1);
}

void VsxLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? Theme::cyberCyan : Theme::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
}

void VsxLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (Theme::deepBlack.withAlpha (0.85f));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 6.0f);
}

void VsxLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart,
                                    int thumbSize, bool over, bool)
{
    auto thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize) : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (Theme::electricPurple.withAlpha (over ? 0.9f : 0.6f));
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

} // namespace vsx
