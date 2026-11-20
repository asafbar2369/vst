#include "HeaderHero.h"

namespace vsx
{
static bool currentIsFavourite (ViceSynthProcessor& p)
{
    const int i = p.getPresetManager().indexOfName (p.getCurrentMeta().name);
    return i >= 0 && p.getPresetManager().getEntries()[(size_t) i].favorite;
}

static void toggleFavourite (ViceSynthProcessor& p)
{
    auto& pm = p.getPresetManager();
    const int i = pm.indexOfName (p.getCurrentMeta().name);
    if (i >= 0)
    {
        const auto e = pm.getEntries()[(size_t) i];
        pm.setFavorite (e, ! e.favorite);
    }
}

// =================================================================================
MainHeader::MainHeader (ViceSynthProcessor& p) : proc (p)
{
    addAndMakeVisible (art);
    art.setInterceptsMouseClicks (false, false);
    const std::pair<Icons::Type, const char*> items[] = { { Icons::Type::Library, "LIBRARY" }, { Icons::Type::Studio, "STUDIO" },
                                                          { Icons::Type::SoundDesign, "SOUND DESIGN" }, { Icons::Type::Stage, "STAGE" } };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = tabs.add (new IconButton (items[i].first, juce::String (items[i].second).toLowerCase(), items[i].second));
        b->accent = Theme::neonPink;
        b->onClick = [this, i] { if (onTab) onTab (i); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &fav, &undo, &redo, &settings, &help }) addAndMakeVisible (b);
    fav.onClick = [this] { toggleFavourite (proc); refresh(); };
    undo.onClick = [this] { proc.undo(); };
    redo.onClick = [this] { proc.redo(); };
    settings.onClick = [this] { if (onSettings) onSettings(); };
    help.onClick = [this] { if (onHelp) onHelp(); };
    cpu.setFont (Theme::font (12.0f, true));
    cpu.setJustificationType (juce::Justification::centred);
    cpu.setColour (juce::Label::textColourId, Theme::text);
    cpu.setTooltip ("Measured share of the audio callback time used by VICE SYNTH X (JUCE AudioProcessLoadMeasurer)");
    addAndMakeVisible (cpu);
    setActiveTab (1);
}

void MainHeader::setActiveTab (int t)
{
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setToggleState (i == t, juce::dontSendNotification);
}

void MainHeader::refresh()
{
    fav.setIcon (currentIsFavourite (proc) ? Icons::Type::HeartFilled : Icons::Type::Heart);
    undo.setEnabled (proc.canUndo());
    redo.setEnabled (proc.canRedo());
    cpu.setText ("CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%", juce::dontSendNotification);
}

void MainHeader::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack);
}

void MainHeader::paintOverChildren (juce::Graphics& g)
{
    // keep the left side calm behind the logo: artwork fades in towards the right
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient fade (Theme::deepBlack, r.getX() + r.getWidth() * 0.22f, 0.0f, Theme::deepBlack.withAlpha (0.0f), r.getX() + r.getWidth() * 0.62f, 0.0f, false);
    g.setGradientFill (fade);
    g.fillRect (r.withRight (r.getX() + r.getWidth() * 0.62f));
    g.setColour (Theme::deepBlack.withAlpha (0.35f));
    g.fillRect (r);
    MiamiArtwork::drawLogo (g, juce::Rectangle<float> (26.0f, 10.0f, 470.0f, 58.0f));
    // navigation container
    g.setColour (Theme::deepBlack.withAlpha (0.65f));
    g.fillRoundedRectangle (navBox.toFloat(), 8.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (navBox.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (Theme::border);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void MainHeader::resized()
{
    art.setBounds (getLocalBounds());
    auto r = getLocalBounds();
    navBox = juce::Rectangle<int> (500, 12, 676, 50);
    auto n = navBox.reduced (4);
    const int tw = n.getWidth() / 4;
    for (auto* b : tabs) b->setBounds (n.removeFromLeft (tw).reduced (2, 0));
    auto right = r.removeFromRight (290).reduced (8, 20);
    help.setBounds (right.removeFromRight (34));
    settings.setBounds (right.removeFromRight (34));
    right.removeFromRight (6);
    cpu.setBounds (right.removeFromRight (74));
    right.removeFromRight (6);
    redo.setBounds (right.removeFromRight (32));
    undo.setBounds (right.removeFromRight (32));
    fav.setBounds (right.removeFromRight (34));
    // these sit inside paintOverChildren's dark overlay; bring them to the front
    for (auto* c : getChildren()) if (c != &art) c->toFront (false);
}

// =================================================================================
PresetHero::PresetHero (ViceSynthProcessor& p) : proc (p)
{
    // the artwork is painted manually (not as a visible child) so the fade, title and
    // waveform can be layered over it while the buttons stay on top
    art.setInterceptsMouseClicks (false, false);
    for (auto* b : { &prev, &next, &fav, &panic, &play }) addAndMakeVisible (b);
    prev.onClick = [this] { proc.stepPreset (-1); };
    next.onClick = [this] { proc.stepPreset (1); };
    fav.onClick = [this] { toggleFavourite (proc); refresh(); };
    panic.onClick = [this] { proc.panic(); };
    play.onClick = [this] { proc.audition (60, 100, 1.6); };
    panic.showBackground = play.showBackground = true;
    refresh();
}

void PresetHero::refresh()
{
    const auto& m = proc.getCurrentMeta();
    juce::StringArray t;
    if (m.category.isNotEmpty()) t.add (m.category.toUpperCase());
    for (const auto& tag : m.tags) if (t.size() < 4 && tag.isNotEmpty()) t.addIfNotAlreadyThere (tag.toUpperCase());
    const auto newTags = t.joinIntoString (juce::String::fromUTF8 ("  \xe2\x80\xa2  "));
    if (m.name != name || newTags != tags)
    {
        name = m.name;
        tags = newTags;
        layoutFav();
        repaint();
    }
    fav.setIcon (currentIsFavourite (proc) ? Icons::Type::HeartFilled : Icons::Type::Heart);
    repaint (waveBounds);
}

void PresetHero::resized()
{
    auto r = getLocalBounds();
    artBounds = r.withTrimmedLeft (r.getWidth() * 34 / 100).reduced (1);
    art.setBounds (artBounds);
    waveBounds = artBounds.withTrimmedLeft (artBounds.getWidth() / 10).withSizeKeepingCentre (artBounds.getWidth() * 8 / 10, artBounds.getHeight() * 7 / 10);
    prev.setBounds (r.removeFromLeft (34).withSizeKeepingCentre (30, 40));
    next.setBounds (r.removeFromRight (34).withSizeKeepingCentre (30, 40));
    layoutFav();
    auto buttons = juce::Rectangle<int> (52, getHeight() - 38, 70, 26);
    play.setBounds (buttons.removeFromLeft (32));
    buttons.removeFromLeft (6);
    panic.setBounds (buttons.removeFromLeft (32));
}

void PresetHero::layoutFav()
{
    const float nameH = nameHeight();
    const auto nf = juce::Font (juce::FontOptions (nameH, juce::Font::bold));
    const float maxW = (float) artBounds.getX() - 52.0f + 120.0f;
    const float nw = std::min (maxW, juce::GlyphArrangement::getStringWidth (nf, name));
    fav.setBounds (juce::Rectangle<int> ((int) (52.0f + nw + 10.0f), 32 + (int) (nameH * 0.22f), 28, 28));
}

void PresetHero::paint (juce::Graphics& g)
{
    Theme::card (g, getLocalBounds().toFloat(), 10.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (artBounds.toFloat(), 9.0f);
        g.reduceClipRegion (clip);
        g.setOrigin (artBounds.getPosition());
        art.paintEntireComponent (g, true);
    }
    // fade the artwork into the card on the left
    auto ab = artBounds.toFloat();
    juce::ColourGradient fade (Theme::panelLight, ab.getX(), 0.0f, Theme::panelLight.withAlpha (0.0f), ab.getX() + ab.getWidth() * 0.32f, 0.0f, false);
    g.setGradientFill (fade);
    g.fillRect (ab.withWidth (ab.getWidth() * 0.34f));

    // name / tags (drawn over the faded edge so long names stay readable)
    const float nameH = nameHeight();
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.5f, true).withExtraKerningFactor (0.12f));
    g.drawText ("PRESET", juce::Rectangle<float> (52.0f, 18.0f, 300.0f, 14.0f), juce::Justification::left);
    g.setColour (Theme::text);
    g.setFont (juce::Font (juce::FontOptions (nameH, juce::Font::bold)));
    const float maxW = (float) artBounds.getX() - 52.0f + 120.0f;
    g.drawFittedText (name, juce::Rectangle<int> (52, 32, (int) maxW, (int) nameH + 4), juce::Justification::left, 1, 0.7f);
    g.setColour (Theme::cyberCyan.withAlpha (0.9f));
    g.setFont (Theme::font (12.0f, true).withExtraKerningFactor (0.08f));
    g.drawText (tags, juce::Rectangle<float> (52.0f, 36.0f + nameH, maxW, 16.0f), juce::Justification::left);

    // live waveform of the actual output across the artwork
    const int w = proc.scopeWrite.load();
    const int n = 900;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const int idx = (w - n + i + 2048) & 2047;
        peak = std::max (peak, std::abs (proc.scopeBuffer.getSample (0, idx)));
    }
    const auto wr = waveBounds.toFloat();
    juce::Path p;
    const float norm = peak > 1.0e-4f ? 0.9f / std::max (0.15f, peak) : 0.0f;
    for (int i = 0; i < 300; ++i)
    {
        const int idx = (w - n + i * 3 + 2048) & 2047;
        const float v = 0.5f * (proc.scopeBuffer.getSample (0, idx) + proc.scopeBuffer.getSample (1, idx)) * norm;
        const float env = std::sin (juce::MathConstants<float>::pi * (float) i / 299.0f);
        const float x = wr.getX() + wr.getWidth() * (float) i / 299.0f;
        const float y = wr.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * env * wr.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    juce::ColourGradient wg (Theme::neonPink, wr.getX(), 0.0f, Theme::cyberCyan, wr.getRight(), 0.0f, false);
    g.setColour (Theme::neonPink.withAlpha (0.18f));
    g.strokePath (p, juce::PathStrokeType (6.0f));
    g.setGradientFill (wg);
    g.strokePath (p, juce::PathStrokeType (1.8f));
}

} // namespace vsx
