#include "Theme.h"
#include <cmath>

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
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float radius = 0.5f * (float) std::min (w, h) - 2.0f;
    if (radius < 3.0f || ! std::isfinite (pos) || ! std::isfinite (start)
        || ! std::isfinite (end) || end <= start)
        return;
    juce::Graphics::ScopedSaveState save (g);
    g.setOpacity (s.isEnabled() ? 1.0f : 0.45f);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float to = start + juce::jlimit (0.0f, 1.0f, pos) * (end - start);
    const float from = (bool) s.getProperties().getWithDefault ("bipolar", false)
                         ? (start + end) * 0.5f : start;
    const float capR = radius * 0.69f;
    const auto ivory = juce::Colour (0xffeee2c5);
    const auto amber = juce::Colour (0xffd8ae70);
    const auto point = [&] (float angle, float r)
    {
        return juce::Point<float> (c.x + r * std::sin (angle), c.y - r * std::cos (angle));
    };
    const auto disc = [&] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c); };

    // Hardware-style dial: an engraved fixed scale, not another neon value ring.
    const int ticks = radius >= 19.0f ? 11 : 7;
    for (int i = 0; i < ticks; ++i)
    {
        const float angle = start + (end - start) * (float) i / (float) (ticks - 1);
        const bool major = i == 0 || i == ticks - 1 || i == ticks / 2;
        const bool active = angle >= std::min (from, to) - 0.001f && angle <= std::max (from, to) + 0.001f;
        g.setColour (active ? amber : ivory.withAlpha (major ? 0.60f : 0.30f));
        g.drawLine (juce::Line<float> (point (angle, radius * (major ? 0.77f : 0.84f)),
                                       point (angle, radius * 0.95f)), major ? 1.4f : 0.9f);
    }

    // Recessed socket, dark rubber/Bakelite skirt and rotating micro-ribs.
    g.setColour (juce::Colour (0xff08090b));
    g.fillEllipse (disc (capR * 1.11f).translated (0.0f, radius * 0.055f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff62605b), c.x, c.y - capR,
                                             juce::Colour (0xff111214), c.x, c.y + capR, false));
    g.fillEllipse (disc (capR));
    if (radius >= 11.0f)
    {
        const int ribs = 32;
        for (int i = 0; i < ribs; ++i)
        {
            const float a = to + juce::MathConstants<float>::twoPi * (float) i / (float) ribs;
            g.setColour (juce::Colour (0xff101112));
            g.drawLine (juce::Line<float> (point (a, capR * 0.74f), point (a, capR * 0.97f)), 1.1f);
            g.setColour (juce::Colour (0xff74716a).withAlpha (0.50f));
            g.drawLine (juce::Line<float> (point (a + 0.035f, capR * 0.76f),
                                           point (a + 0.035f, capR * 0.96f)), 0.7f);
        }
    }

    // Bevel and matte face, illuminated from the upper left.
    g.setColour (juce::Colour (0xff79766e).withAlpha (0.55f));
    g.drawEllipse (disc (capR * 0.76f), 0.9f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff454440), c.x - capR, c.y - capR,
                                             juce::Colour (0xff17191a), c.x + capR, c.y + capR, false));
    g.fillEllipse (disc (capR * 0.72f));
    g.setColour (juce::Colour (0xff0d0e0f));
    g.drawEllipse (disc (capR * 0.72f), 0.7f);

    // High-contrast ivory indicator always reflects the actual slider value.
    juce::Path pointer;
    const float pointerWidth = juce::jmax (1.2f, capR * 0.13f);
    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -capR * 0.87f,
                                 pointerWidth, capR * 0.65f, pointerWidth * 0.28f);
    g.setColour (ivory);
    g.fillPath (pointer, juce::AffineTransform::rotation (to).translated (c.x, c.y));

    // Keep the modern UI's section colour only as a small live-modulation accent.
    const float depth = (float) (double) s.getProperties().getWithDefault ("modDepth", 0.0);
    const float live = (float) (double) s.getProperties().getWithDefault ("modLive", -1.0);
    const float modR = radius * 0.70f + radius * 0.06f;
    if (std::isfinite (depth) && std::abs (depth) > 1.0e-4f)
    {
        const float target = juce::jlimit (start, end, to + depth * (end - start));
        if (std::abs (target - to) > 1.0e-4f)
        {
            juce::Path ring;
            ring.addCentredArc (c.x, c.y, modR, modR, 0.0f, std::min (to, target),
                                std::max (to, target), true);
            g.setColour (cyberCyan.withAlpha (0.9f));
            g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.0f, radius * 0.065f)));
        }
    }
    if (std::isfinite (live) && live >= 0.0f)
    {
        const auto p = point (start + juce::jlimit (0.0f, 1.0f, live) * (end - start), modR);
        const float size = juce::jlimit (2.0f, 4.0f, radius * 0.15f);
        g.setColour (accent.brighter (0.45f));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (p));
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
