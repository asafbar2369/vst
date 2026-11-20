#include "Theme.h"

namespace vsx
{
static constexpr float kTwoPiF = 6.28318530718f;

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

    void card (juce::Graphics& g, juce::Rectangle<float> r, float corner, bool raised)
    {
        juce::ColourGradient fill (panelLight, r.getX(), r.getY(), panel, r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, corner);
        if (raised)
        {
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
        }
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }

    void displayWell (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        g.setColour (display);
        g.fillRoundedRectangle (r, corner);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.45f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 8.0f, false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (r, corner);
        g.setColour (border.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }

    juce::Colour macroAccent (int m)
    {
        static const juce::Colour cols[] = { neonPink, neonPink.interpolatedWith (electricPurple, 0.5f), electricPurple,
                                             electricPurple.interpolatedWith (cyberCyan, 0.5f), cyberCyan, cyberCyan,
                                             sunsetOrange, sunsetOrange };
        return cols[juce::jlimit (0, 7, m)];
    }

    void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour)
    {
        card (g, r, corner, true);
    }

    void neonText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c, float height, juce::Justification j)
    {
        g.setFont (displayFont (height));
        g.setColour (c.withAlpha (0.18f));
        g.drawText (t, r.translated (0.0f, 1.0f).expanded (1.0f), j, false);
        g.setColour (c);
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
    setColour (juce::Slider::textBoxBackgroundColourId, display);
    setColour (juce::Slider::textBoxHighlightColourId, electricPurple.withAlpha (0.5f));
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
    const float arcR = radius - 2.5f;
    const float thick = juce::jlimit (2.5f, 5.0f, radius * 0.12f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (juce::Colour (0xff23262f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    const float to = start + pos * (end - start);
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, to), std::max (from, to), true);
    g.setColour (accent.withAlpha (0.18f));
    g.strokePath (arc, juce::PathStrokeType (thick * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::ColourGradient arcGrad (accent.brighter (0.25f), c.x - arcR, c.y, accent, c.x + arcR, c.y, false);
    g.setGradientFill (arcGrad);
    g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float modDepth = (float) (double) s.getProperties().getWithDefault ("modDepth", 0.0);
    if (std::abs (modDepth) > 1.0e-4f)
    {
        const float mr = arcR + thick * 0.5f + 2.0f;
        const float a0 = juce::jlimit (start, end, to), a1 = juce::jlimit (start, end, to + modDepth * (end - start));
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, mr, mr, 0.0f, std::min (a0, a1), std::max (a0, a1), true);
        g.setColour (cyberCyan.withAlpha (0.9f));
        g.strokePath (ring, juce::PathStrokeType (1.6f));
        const float live = (float) (double) s.getProperties().getWithDefault ("modLive", -1.0);
        if (live >= 0.0f)
        {
            const float la = start + live * (end - start);
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre ({ c.x + mr * std::sin (la), c.y - mr * std::cos (la) }));
        }
    }

    // machined graphite cap with knurled rim and soft shadow
    const float capR = arcR - thick - 2.5f;
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cap.translated (0.0f, capR * 0.12f).expanded (1.0f));
    juce::ColourGradient body (juce::Colour (0xff3a3d48), cap.getX(), cap.getY(), juce::Colour (0xff111319), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (cap);
    const int ridges = 28;
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i < ridges; ++i)
    {
        const float a = kTwoPiF * (float) i / (float) ridges;
        g.drawLine (c.x + std::sin (a) * capR * 0.86f, c.y - std::cos (a) * capR * 0.86f, c.x + std::sin (a) * capR, c.y - std::cos (a) * capR, 1.0f);
    }
    const auto face = cap.reduced (capR * 0.2f);
    juce::ColourGradient top (juce::Colour (0xff2c2f39), face.getX(), face.getY(), juce::Colour (0xff181a21), face.getX(), face.getBottom(), false);
    g.setGradientFill (top);
    g.fillEllipse (face);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (face, 1.0f);
    g.setColour (border);
    g.drawEllipse (cap, 1.0f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.2f, -capR + capR * 0.12f, 2.4f, capR * 0.5f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (s.isEnabled() ? 0.92f : 0.35f));
    g.fillPath (ptr, juce::AffineTransform::rotation (to).translated (c.x, c.y));
    if (s.hasKeyboardFocus (false))
    {
        g.setColour (cyberCyan.withAlpha (0.6f));
        g.drawEllipse (bounds.withSizeKeepingCentre (radius * 2.0f, radius * 2.0f), 1.0f);
    }
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
    if (b.getProperties().getWithDefault ("flat", false) && ! on)
    {
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (r, 6.0f); }
        return;
    }
    if (on)
    {
        juce::ColourGradient grad (accent.withAlpha (0.55f), r.getX(), r.getY(), accent.withAlpha (0.22f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
    }
    else
        g.setColour (panelLight.brighter (over ? 0.08f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? accent.withAlpha (0.85f) : border.brighter (over ? 0.3f : 0.0f));
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
