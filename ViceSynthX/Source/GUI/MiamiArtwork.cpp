#include "MiamiArtwork.h"
#include "../Core/DspUtils.h"

namespace vsx
{
namespace MiamiArtwork
{
void drawPalm (juce::Graphics& g, juce::Point<float> base, float h, float lean, juce::Colour c)
{
    // curved trunk
    juce::Path trunk;
    const juce::Point<float> top { base.x + lean * h * 0.35f, base.y - h };
    const juce::Point<float> ctrl { base.x + lean * h * 0.05f, base.y - h * 0.55f };
    const float w0 = h * 0.045f, w1 = h * 0.022f;
    trunk.startNewSubPath (base.x - w0, base.y);
    trunk.quadraticTo (ctrl.x - w0 * 0.7f, ctrl.y, top.x - w1, top.y);
    trunk.lineTo (top.x + w1, top.y);
    trunk.quadraticTo (ctrl.x + w0 * 0.7f, ctrl.y, base.x + w0, base.y);
    trunk.closeSubPath();
    g.setColour (c);
    g.fillPath (trunk);

    // fronds
    const float fl = h * 0.42f;
    const float angles[] = { -2.9f, -2.4f, -1.9f, -1.35f, -0.85f, -0.35f, 0.15f, 0.6f };
    for (float a : angles)
    {
        const float ang = a + lean * 0.2f;
        juce::Point<float> tip { top.x + std::cos (ang) * fl, top.y + std::sin (ang) * fl * 0.55f + fl * 0.35f };
        juce::Point<float> mid { top.x + std::cos (ang) * fl * 0.5f, top.y + std::sin (ang) * fl * 0.5f - fl * 0.08f };
        const juce::Point<float> nrm { -std::sin (ang) * h * 0.035f, std::cos (ang) * h * 0.035f };
        juce::Path frond;
        frond.startNewSubPath (top);
        frond.quadraticTo (mid + nrm, tip);
        frond.quadraticTo (mid - nrm * 0.4f, top);
        frond.closeSubPath();
        g.fillPath (frond);
    }
    g.fillEllipse (juce::Rectangle<float> (h * 0.07f, h * 0.06f).withCentre (top));
}

void draw (juce::Graphics& g, juce::Rectangle<float> r, float anim, float sunX, float horizonFrac, bool palmsOn)
{
    using namespace Theme;
    const float horizon = r.getY() + r.getHeight() * horizonFrac;

    // sky
    juce::ColourGradient sky (darkPurple, r.getX(), r.getY(), sunsetOrange, r.getX(), horizon, false);
    sky.addColour (0.35, electricPurple.darker (0.35f));
    sky.addColour (0.7, neonPink.darker (0.1f));
    g.setGradientFill (sky);
    g.fillRect (r.withBottom (horizon));

    // stars
    Rng rng (77);
    for (int i = 0; i < 60; ++i)
    {
        const float x = r.getX() + rng.uni() * r.getWidth();
        const float y = r.getY() + rng.uni() * (horizon - r.getY()) * 0.45f;
        const float tw = 0.4f + 0.6f * std::abs (std::sin (anim * 2.0f + (float) i));
        g.setColour (juce::Colours::white.withAlpha (0.25f * tw));
        g.fillEllipse (x, y, 1.6f, 1.6f);
    }

    // sun with horizontal cut stripes
    const float sunR = r.getHeight() * 0.42f;
    const juce::Point<float> sc { r.getX() + r.getWidth() * sunX, horizon - sunR * 0.35f };
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRectangle (r.withBottom (horizon));
        g.reduceClipRegion (clip);
        g.setColour (sunsetOrange.withAlpha (0.18f));
        g.fillEllipse (juce::Rectangle<float> (sunR * 2.9f, sunR * 2.9f).withCentre (sc));
        juce::ColourGradient sun (juce::Colour (0xffffd27a), sc.x, sc.y - sunR, neonPink, sc.x, sc.y + sunR, false);
        g.setGradientFill (sun);
        juce::Path disc;
        disc.addEllipse (juce::Rectangle<float> (sunR * 2.0f, sunR * 2.0f).withCentre (sc));
        for (int s = 0; s < 7; ++s)
        {
            const float y = sc.y + sunR * (0.05f + 0.13f * (float) s);
            const float hgt = 1.0f + (float) s * 1.3f;
            juce::Path cut;
            cut.addRectangle (sc.x - sunR, y, sunR * 2.0f, hgt);
            disc.setUsingNonZeroWinding (false);
            disc.addPath (cut);
        }
        g.fillPath (disc);
    }

    // skyline silhouettes
    Rng b (4242);
    const juce::Colour building = deepBlack.interpolatedWith (darkPurple, 0.6f);
    float x = r.getX();
    while (x < r.getRight())
    {
        const float w = r.getWidth() * (0.012f + 0.03f * b.uni());
        float hh = r.getHeight() * (0.06f + 0.26f * b.uni() * b.uni());
        // keep the area around the sun lower for readability of the logo
        if (std::abs (x - sc.x) < sunR * 0.9f) hh *= 0.55f;
        const auto rect = juce::Rectangle<float> (x, horizon - hh, w, hh);
        g.setColour (building);
        g.fillRect (rect);
        if (b.uni() > 0.75f)
        {
            juce::Path spire;
            spire.addTriangle (x + w * 0.3f, horizon - hh, x + w * 0.7f, horizon - hh, x + w * 0.5f, horizon - hh - r.getHeight() * 0.05f);
            g.fillPath (spire);
        }
        for (float wy = horizon - hh + 4.0f; wy < horizon - 3.0f; wy += 5.0f)
            for (float wx = x + 2.0f; wx < x + w - 2.0f; wx += 4.0f)
                if (b.uni() > 0.72f)
                {
                    g.setColour ((b.uni() > 0.5f ? cyberCyan : neonPink).withAlpha (0.35f + 0.4f * b.uni()));
                    g.fillRect (wx, wy, 1.6f, 1.8f);
                }
        x += w + 1.0f;
    }

    // ocean with neon reflections
    juce::ColourGradient sea (darkPurple, r.getX(), horizon, deepBlack, r.getX(), r.getBottom(), false);
    g.setGradientFill (sea);
    g.fillRect (r.withTop (horizon));
    for (int i = 0; i < 9; ++i)
    {
        const float y = horizon + 3.0f + (float) i * (r.getBottom() - horizon) / 9.0f;
        const float half = sunR * (1.0f - (float) i * 0.08f) * (0.8f + 0.2f * std::sin (anim * 1.5f + (float) i));
        g.setColour (neonPink.withAlpha (0.45f - (float) i * 0.04f));
        g.fillRect (sc.x - half, y, half * 2.0f, 1.5f);
        g.setColour (cyberCyan.withAlpha (0.18f));
        g.fillRect (r.getX() + r.getWidth() * 0.05f + (float) (i * 37 % 200), y + 2.0f, 40.0f, 1.0f);
    }

    // palms
    if (palmsOn)
    {
    const juce::Colour palm = deepBlack.withAlpha (0.95f);
    drawPalm (g, { r.getX() + r.getWidth() * 0.035f, r.getBottom() }, r.getHeight() * 0.95f, 0.45f, palm);
    drawPalm (g, { r.getX() + r.getWidth() * 0.085f, r.getBottom() }, r.getHeight() * 0.7f, 0.25f, palm);
    drawPalm (g, { r.getRight() - r.getWidth() * 0.04f, r.getBottom() }, r.getHeight() * 0.9f, -0.5f, palm);
    drawPalm (g, { r.getRight() - r.getWidth() * 0.095f, r.getBottom() }, r.getHeight() * 0.65f, -0.2f, palm);
    }

    // vignette
    juce::ColourGradient vig (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(), deepBlack.withAlpha (0.6f), r.getX(), r.getY(), true);
    g.setGradientFill (vig);
    g.fillRect (r);
}

void drawViceScript (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace Theme;
    g.setFont (juce::Font (juce::FontOptions (r.getHeight() * 0.62f, juce::Font::bold | juce::Font::italic)));
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (neonPink.withAlpha (0.07f * (float) (4 - i)));
        g.drawText ("Vice", r.expanded ((float) i * 1.5f), juce::Justification::centred, false);
    }
    g.setColour (neonPink.brighter (0.35f));
    g.drawText ("Vice", r, juce::Justification::centred, false);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace Theme;
    auto top = r.removeFromTop (r.getHeight() * 0.72f);
    const auto f = juce::Font (juce::FontOptions (top.getHeight() * 0.86f, juce::Font::bold | juce::Font::italic)).withExtraKerningFactor (0.04f);
    g.setFont (f);
    const float w = juce::GlyphArrangement::getStringWidth (f, "VICE SYNTH ");
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawText ("VICE SYNTH", top.translated (0.0f, 2.0f), juce::Justification::centredLeft, false);
    g.setColour (text);
    g.drawText ("VICE SYNTH", top, juce::Justification::centredLeft, false);
    g.setColour (neonPink.withAlpha (0.25f));
    g.drawText ("X", top.withTrimmedLeft (w).expanded (1.5f), juce::Justification::centredLeft, false);
    g.setColour (neonPink);
    g.drawText ("X", top.withTrimmedLeft (w), juce::Justification::centredLeft, false);
    g.setColour (text.withAlpha (0.78f));
    g.setFont (font (r.getHeight() * 0.62f, true).withExtraKerningFactor (0.18f));
    g.drawText ("VIRTUAL ANALOG / WAVETABLE / HYBRID SYNTHESIZER", r, juce::Justification::centredLeft, false);
}
} // namespace MiamiArtwork

void MiamiHeader::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (cache.isNull() || std::abs (scale - cacheScale) > 0.01f)
    {
        cacheScale = scale;
        const int w = std::max (1, (int) (getWidth() * scale)), h = std::max (1, (int) (getHeight() * scale));
        cache = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics ig (cache);
        ig.addTransform (juce::AffineTransform::scale (scale));
        MiamiArtwork::draw (ig, getLocalBounds().toFloat(), 0.0f, sunX, horizonFrac, true);
        if (darkenAmount > 0.0f)
        {
            ig.setColour (Theme::deepBlack.withAlpha (darkenAmount));
            ig.fillRect (getLocalBounds());
        }
    }
    g.drawImage (cache, getLocalBounds().toFloat());
}

} // namespace vsx
