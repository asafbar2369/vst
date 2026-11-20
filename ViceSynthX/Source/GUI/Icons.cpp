#include "Icons.h"

namespace vsx
{
namespace Icons
{
void draw (juce::Graphics& g, Type t, juce::Rectangle<float> r, juce::Colour c, float sw)
{
    r = r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight()), std::min (r.getWidth(), r.getHeight()));
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    auto P = [&] (float px, float py) { return juce::Point<float> (x + px * w, y + py * h); };
    const juce::PathStrokeType stroke (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;
    g.setColour (c);
    switch (t)
    {
        case Type::Library:
            for (int i = 0; i < 4; ++i)
            {
                const float hh = (i == 0 ? 0.45f : i == 1 ? 0.75f : i == 2 ? 0.6f : 0.9f);
                g.fillRoundedRectangle (x + w * (0.12f + 0.21f * (float) i), y + h * (0.95f - hh), w * 0.13f, h * hh, 1.0f);
            }
            return;
        case Type::Studio:
        case Type::Wave:
            p.startNewSubPath (P (0.0f, 0.5f));
            p.lineTo (P (0.2f, 0.5f)); p.lineTo (P (0.32f, 0.15f)); p.lineTo (P (0.5f, 0.85f)); p.lineTo (P (0.66f, 0.3f)); p.lineTo (P (0.78f, 0.5f)); p.lineTo (P (1.0f, 0.5f));
            break;
        case Type::SoundDesign:
        case Type::Filter:
            for (int i = 0; i < 3; ++i)
            {
                const float yy = 0.2f + 0.3f * (float) i;
                p.startNewSubPath (P (0.05f, yy)); p.lineTo (P (0.95f, yy));
                const float kx = i == 0 ? 0.3f : i == 1 ? 0.68f : 0.45f;
                g.fillEllipse (juce::Rectangle<float> (w * 0.22f, h * 0.22f).withCentre (P (kx, yy)));
            }
            break;
        case Type::Stage:
        case Type::Keyboard:
            g.drawRoundedRectangle (juce::Rectangle<float> (x + w * 0.05f, y + h * 0.15f, w * 0.9f, h * 0.7f), 1.5f, sw);
            for (int i = 1; i < 4; ++i) g.drawVerticalLine ((int) (x + w * (0.05f + 0.225f * (float) i)), y + h * 0.15f, y + h * 0.85f);
            for (float bx : { 0.24f, 0.46f, 0.69f }) g.fillRect (x + w * bx, y + h * 0.15f, w * 0.08f, h * 0.38f);
            return;
        case Type::Heart:
        case Type::HeartFilled:
            p.startNewSubPath (P (0.5f, 0.88f));
            p.cubicTo (P (0.05f, 0.55f), P (0.0f, 0.2f), P (0.28f, 0.14f));
            p.cubicTo (P (0.42f, 0.11f), P (0.5f, 0.24f), P (0.5f, 0.3f));
            p.cubicTo (P (0.5f, 0.24f), P (0.58f, 0.11f), P (0.72f, 0.14f));
            p.cubicTo (P (1.0f, 0.2f), P (0.95f, 0.55f), P (0.5f, 0.88f));
            p.closeSubPath();
            if (t == Type::HeartFilled) { g.fillPath (p); return; }
            break;
        case Type::Undo:
        case Type::Redo:
        {
            const bool redo = t == Type::Redo;
            auto px = [&] (float v) { return redo ? 1.0f - v : v; };
            p.startNewSubPath (P (px (0.2f), 0.4f));
            p.cubicTo (P (px (0.45f), 0.2f), P (px (0.9f), 0.3f), P (px (0.82f), 0.68f));
            p.startNewSubPath (P (px (0.2f), 0.4f)); p.lineTo (P (px (0.22f), 0.15f));
            p.startNewSubPath (P (px (0.2f), 0.4f)); p.lineTo (P (px (0.45f), 0.45f));
            break;
        }
        case Type::Gear:
            for (int i = 0; i < 8; ++i)
            {
                const float a = 6.2831853f * (float) i / 8.0f;
                p.startNewSubPath (P (0.5f + 0.3f * std::sin (a), 0.5f - 0.3f * std::cos (a)));
                p.lineTo (P (0.5f + 0.45f * std::sin (a), 0.5f - 0.45f * std::cos (a)));
            }
            p.addEllipse (juce::Rectangle<float> (w * 0.6f, h * 0.6f).withCentre (r.getCentre()));
            p.addEllipse (juce::Rectangle<float> (w * 0.22f, h * 0.22f).withCentre (r.getCentre()));
            break;
        case Type::Help:
            p.addEllipse (r.reduced (w * 0.06f));
            g.strokePath (p, stroke);
            g.setFont (Theme::font (h * 0.62f, true));
            g.drawText ("?", r, juce::Justification::centred);
            return;
        case Type::Power:
            p.addCentredArc (r.getCentreX(), r.getCentreY() + h * 0.04f, w * 0.38f, h * 0.38f, 0.0f, 0.6f, 6.2831853f - 0.6f, true);
            p.startNewSubPath (P (0.5f, 0.08f)); p.lineTo (P (0.5f, 0.48f));
            break;
        case Type::Search:
            p.addEllipse (juce::Rectangle<float> (w * 0.55f, h * 0.55f).withPosition (x + w * 0.08f, y + h * 0.08f));
            p.startNewSubPath (P (0.56f, 0.56f)); p.lineTo (P (0.92f, 0.92f));
            break;
        case Type::Folder:
            p.startNewSubPath (P (0.05f, 0.25f)); p.lineTo (P (0.4f, 0.25f)); p.lineTo (P (0.5f, 0.35f)); p.lineTo (P (0.95f, 0.35f));
            p.lineTo (P (0.95f, 0.82f)); p.lineTo (P (0.05f, 0.82f)); p.closeSubPath();
            break;
        case Type::Sparkle:
        {
            auto star = [&] (float cx, float cy, float s)
            {
                juce::Path sp;
                sp.startNewSubPath (P (cx, cy - s));
                sp.quadraticTo (P (cx, cy), P (cx + s, cy));
                sp.quadraticTo (P (cx, cy), P (cx, cy + s));
                sp.quadraticTo (P (cx, cy), P (cx - s, cy));
                sp.quadraticTo (P (cx, cy), P (cx, cy - s));
                sp.closeSubPath();
                return sp;
            };
            g.strokePath (star (0.42f, 0.55f, 0.38f), stroke);
            g.fillPath (star (0.8f, 0.2f, 0.14f));
            return;
        }
        case Type::Refresh:
            p.addCentredArc (r.getCentreX(), r.getCentreY(), w * 0.36f, h * 0.36f, 0.0f, 0.4f, 5.6f, true);
            p.startNewSubPath (P (0.68f, 0.06f)); p.lineTo (P (0.7f, 0.24f)); p.lineTo (P (0.52f, 0.24f));
            break;
        case Type::User:
            p.addEllipse (juce::Rectangle<float> (w * 0.36f, h * 0.36f).withCentre (P (0.5f, 0.3f)));
            p.startNewSubPath (P (0.12f, 0.92f));
            p.cubicTo (P (0.15f, 0.55f), P (0.85f, 0.55f), P (0.88f, 0.92f));
            break;
        case Type::Box:
            p.startNewSubPath (P (0.5f, 0.08f)); p.lineTo (P (0.92f, 0.28f)); p.lineTo (P (0.92f, 0.74f)); p.lineTo (P (0.5f, 0.94f));
            p.lineTo (P (0.08f, 0.74f)); p.lineTo (P (0.08f, 0.28f)); p.closeSubPath();
            p.startNewSubPath (P (0.08f, 0.28f)); p.lineTo (P (0.5f, 0.48f)); p.lineTo (P (0.92f, 0.28f));
            p.startNewSubPath (P (0.5f, 0.48f)); p.lineTo (P (0.5f, 0.94f));
            break;
        case Type::ChevronLeft: p.startNewSubPath (P (0.65f, 0.15f)); p.lineTo (P (0.3f, 0.5f)); p.lineTo (P (0.65f, 0.85f)); break;
        case Type::ChevronRight: p.startNewSubPath (P (0.35f, 0.15f)); p.lineTo (P (0.7f, 0.5f)); p.lineTo (P (0.35f, 0.85f)); break;
        case Type::ChevronDown: p.startNewSubPath (P (0.2f, 0.38f)); p.lineTo (P (0.5f, 0.66f)); p.lineTo (P (0.8f, 0.38f)); break;
        case Type::Macro:
            p.addCentredArc (r.getCentreX(), r.getCentreY() + h * 0.1f, w * 0.42f, h * 0.42f, 0.0f, -2.2f, 2.2f, true);
            break;
        case Type::Save:
            p.addRoundedRectangle (r.reduced (w * 0.1f), 2.0f);
            p.startNewSubPath (P (0.3f, 0.1f)); p.lineTo (P (0.3f, 0.38f)); p.lineTo (P (0.7f, 0.38f)); p.lineTo (P (0.7f, 0.1f));
            break;
        case Type::Panic:
            p.addPolygon (r.getCentre(), 8, w * 0.46f, 0.3927f);
            g.strokePath (p, stroke);
            g.fillRoundedRectangle (juce::Rectangle<float> (w * 0.36f, h * 0.36f).withCentre (r.getCentre()), 1.5f);
            return;
        case Type::Play:
            p.startNewSubPath (P (0.25f, 0.12f)); p.lineTo (P (0.85f, 0.5f)); p.lineTo (P (0.25f, 0.88f)); p.closeSubPath();
            g.fillPath (p);
            return;
        case Type::Link:
            p.addRoundedRectangle (x + w * 0.05f, y + h * 0.32f, w * 0.5f, h * 0.36f, h * 0.18f);
            p.addRoundedRectangle (x + w * 0.45f, y + h * 0.32f, w * 0.5f, h * 0.36f, h * 0.18f);
            break;
        case Type::History:
            p.addCentredArc (r.getCentreX(), r.getCentreY(), w * 0.4f, h * 0.4f, 0.0f, -2.6f, 3.14f, true);
            p.startNewSubPath (P (0.5f, 0.25f)); p.lineTo (P (0.5f, 0.52f)); p.lineTo (P (0.68f, 0.62f));
            break;
        case Type::Audio:
            for (int i = 0; i < 7; ++i)
            {
                const float hh = 0.2f + 0.6f * std::abs (std::sin ((float) i * 1.3f));
                p.startNewSubPath (P (0.08f + 0.14f * (float) i, 0.5f - hh * 0.5f)); p.lineTo (P (0.08f + 0.14f * (float) i, 0.5f + hh * 0.5f));
            }
            break;
    }
    g.strokePath (p, stroke);
}
} // namespace Icons

IconButton::IconButton (Icons::Type t, const juce::String& tip, juce::String label)
    : juce::Button (tip), type (t), text (std::move (label))
{
    setTooltip (tip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    if (showBackground || on)
    {
        if (on)
        {
            juce::ColourGradient grad (accent.withAlpha (0.55f), r.getX(), r.getY(), accent.withAlpha (0.25f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
        }
        else
            g.setColour (Theme::panelLight);
        g.fillRoundedRectangle (r.reduced (0.5f), 6.0f);
        g.setColour (on ? accent.withAlpha (0.8f) : Theme::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }
    else if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, 6.0f);
    }
    const auto c = ! isEnabled() ? Theme::textDim.withAlpha (0.35f) : on ? juce::Colours::white : (over || down ? Theme::text : Theme::textDim);
    if (text.isEmpty())
    {
        Icons::draw (g, type, r.reduced (r.getHeight() * 0.24f), c);
        return;
    }
    const float ih = std::min (r.getHeight() * 0.48f, 20.0f);
    juce::GlyphArrangement ga;
    const auto font = Theme::font (std::min (15.0f, r.getHeight() * 0.36f), true);
    const float tw = juce::GlyphArrangement::getStringWidth (font, text);
    const float total = ih + 10.0f + tw;
    auto iconR = juce::Rectangle<float> (r.getCentreX() - total * 0.5f, r.getCentreY() - ih * 0.5f, ih, ih);
    Icons::draw (g, type, iconR, c);
    g.setColour (c);
    g.setFont (font);
    g.drawText (text, juce::Rectangle<float> (iconR.getRight() + 10.0f, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft, false);
}

} // namespace vsx
