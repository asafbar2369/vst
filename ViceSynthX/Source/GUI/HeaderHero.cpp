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
    // artwork is painted manually (not a visible child) so the fades sit below the controls
    // LIBRARY and STAGE were removed (owner request): STUDIO, SOUND DESIGN and direct FM access remain
    const std::tuple<Icons::Type, const char*, int> items[] = { { Icons::Type::Studio, "STUDIO", 1 }, { Icons::Type::SoundDesign, "SOUND DESIGN", 2 } };
    for (const auto& [icon, label, view] : items)
    {
        auto* b = tabs.add (new IconButton (icon, juce::String (label).toLowerCase(), label));
        b->accent = Theme::neonPink;
        const int v = view;
        b->onClick = [this, v] { if (onTab) onTab (v); };
        tabViews.push_back (view);
        addAndMakeVisible (b);
    }
    fmTab.accent = Theme::cyberCyan;
    fmTab.setComponentID ("vsx_header_fm");
    fmTab.onClick = [this] { if (onFm) onFm(); };
    addAndMakeVisible (fmTab);
    masterVol = std::make_unique<ParamKnob> (p, pid::G (pid::global::MasterOut), "MASTER", Theme::cyberCyan);
    masterVol->setComponentID ("vsx_master_vol");
    masterVol->setTooltipExtra ("Final stereo output after all effects. Global: presets never change it; saved with the project.");
    addAndMakeVisible (*masterVol);
    addAndMakeVisible (meter);
    panicB.showBackground = true;
    panicB.accent = Theme::sunsetOrange;
    panicB.setComponentID ("vsx_panic");
    panicB.onClick = [this] { proc.panic(); };
    addAndMakeVisible (panicB);
    fxBypass.setComponentID ("vsx_fx_bypass");
    fxBypass.setClickingTogglesState (true);
    fxBypass.setTooltip ("GLOBAL FX BYPASS: switch every effect off (not stored in presets)");
    fxBypass.setColour (juce::TextButton::buttonOnColourId, Theme::sunsetOrange.withAlpha (0.8f));
    fxBypass.onClick = [this] { proc.setParam (pid::G (pid::global::FxBypass), fxBypass.getToggleState() ? 1.0f : 0.0f); };
    addAndMakeVisible (fxBypass);
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
    setComponentID ("vsx_main_header");
    sizeButton.setComponentID ("vsx_ui_size");
    sizeButton.setTitle ("Interface size");
    sizeButton.setTooltip ("Resize the entire synthesizer interface (50% to 200%)");
    sizeButton.setWantsKeyboardFocus (true);
    sizeButton.onClick = [this] { showSizeMenu(); };
    addAndMakeVisible (sizeButton);

    initButton.setComponentID ("vsx_init_patch");
    initButton.setTooltip ("INIT: replace the sound with a clean starting patch (one saw, open filter, no effects). Undo restores it.");
    initButton.setColour (juce::TextButton::buttonColourId, Theme::neonPink.withAlpha (0.28f));
    initButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    initButton.onClick = [this] { proc.loadInitPatch(); refresh(); };
    addAndMakeVisible (initButton);

    // button order on screen: PRESETS, AI, MACROS, KEYS; panel indices follow MainHeader::Panel
    const char* panelNames[] = { "AI", "PRESETS", "MACROS", "KEYS" };
    const char* panelTips[] = { "Show / hide the AI sound designer", "Show / hide the preset browser", "Show / hide the macro strip",
                                "Show / hide the performance keyboard" };
    for (int i = 0; i < NumPanels; ++i)
    {
        auto* b = panelButtons.add (new juce::TextButton (panelNames[i]));
        b->setComponentID ("vsx_panel_toggle_" + juce::String (i));
        b->setClickingTogglesState (true);
        b->setToggleState (true, juce::dontSendNotification);
        b->setTooltip (panelTips[i]);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::cyberCyan.withAlpha (0.45f));
        b->setColour (juce::TextButton::buttonColourId, Theme::deepBlack.withAlpha (0.6f));
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b->setColour (juce::TextButton::textColourOffId, Theme::textDim);
        b->onClick = [this, i, b]
        {
            if (onPanelToggle) onPanelToggle (i, b->getToggleState());
            else if (onTogglePanel) onTogglePanel (i);
        };
        addAndMakeVisible (b);
    }
    setActiveTab (1);
    refresh();
}

void MainHeader::setScaleDisplay (float scale)
{
    currentScale = scale;
    sizeButton.setButtonText ("SIZE " + juce::String (juce::roundToInt (scale * 100.0f)) + "%");
}

void MainHeader::showSizeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("INTERFACE SIZE");
    for (const int percent : { 50, 60, 75, 85, 100, 125, 150, 175, 200 })
        menu.addItem (percent, juce::String (percent) + "%", true,
                      juce::roundToInt (currentScale * 100.0f) == percent);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sizeButton),
                        [safe = juce::Component::SafePointer<MainHeader> (this)] (int result)
    {
        if (safe != nullptr && result >= 50 && result <= 200 && safe->onScale)
            safe->onScale ((float) result / 100.0f);
    });
}

void MainHeader::setPanelVisible (int panel, bool visible)
{
    if (juce::isPositiveAndBelow (panel, panelButtons.size())) panelButtons[panel]->setToggleState (visible, juce::dontSendNotification);
}

void MainHeader::setActiveTab (int t)
{
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setToggleState (tabViews[(size_t) i] == t, juce::dontSendNotification);
}

// =================================================================================
OutputMeter::OutputMeter (ViceSynthProcessor& p) : proc (p)
{
    setComponentID ("vsx_output_meter");
    setTooltip ("Output level L / R. CLIP lights when the output safety stage engages (click to reset).");
    startTimerHz (30);
}

void OutputMeter::timerCallback()
{
    const float l = proc.outputPeakL.load(), r = proc.outputPeakR.load();
    shownL = std::max (l, shownL * 0.85f);
    shownR = std::max (r, shownR * 0.85f);
    clip = proc.getClipLatched();
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto clipBox = r.removeFromTop (14.0f);
    g.setColour (clip ? juce::Colour (0xffff3b3b) : Theme::display);
    g.fillRoundedRectangle (clipBox.reduced (1.0f), 3.0f);
    g.setColour (clip ? juce::Colours::white : Theme::textDim);
    g.setFont (Theme::font (9.5f, true));
    g.drawText ("CLIP", clipBox, juce::Justification::centred);
    r.removeFromTop (3.0f);
    auto bar = [&] (juce::Rectangle<float> b, float peak)
    {
        g.setColour (Theme::display);
        g.fillRoundedRectangle (b, 2.0f);
        const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = b.withTop (b.getBottom() - b.getHeight() * frac);
        g.setColour (db > -1.0f ? juce::Colour (0xffff3b3b) : db > -6.0f ? Theme::sunsetOrange : Theme::cyberCyan);
        g.fillRoundedRectangle (fill, 2.0f);
    };
    const float w = (r.getWidth() - 3.0f) / 2.0f;
    bar (r.withWidth (w), shownL);
    bar (r.withTrimmedLeft (w + 3.0f), shownR);
}

void MainHeader::refresh()
{
    fxBypass.setToggleState (proc.getParam (pid::G (pid::global::FxBypass)) > 0.5f, juce::dontSendNotification);
    fav.setIcon (currentIsFavourite (proc) ? Icons::Type::HeartFilled : Icons::Type::Heart);
    undo.setEnabled (proc.canUndo());
    redo.setEnabled (proc.canRedo());
    cpu.setText ("CPU " + juce::String (juce::roundToInt (proc.cpuLoad.load() * 100.0f)) + "%", juce::dontSendNotification);
}

void MainHeader::paint (juce::Graphics& g)
{
    g.fillAll (Theme::deepBlack);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.setOrigin (art.getPosition());
        art.paintEntireComponent (g, true);
    }
    // keep the left side calm behind the logo: artwork fades in towards the right
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient fade (Theme::deepBlack, r.getX() + r.getWidth() * 0.22f, 0.0f, Theme::deepBlack.withAlpha (0.0f), r.getX() + r.getWidth() * 0.62f, 0.0f, false);
    g.setGradientFill (fade);
    g.fillRect (r.withRight (r.getX() + r.getWidth() * 0.62f));
    g.setColour (Theme::deepBlack.withAlpha (0.35f));
    g.fillRect (r);
    MiamiArtwork::drawLogo (g, juce::Rectangle<float> (16.0f, 15.0f, 272.0f, 44.0f));
    // navigation container
    g.setColour (Theme::deepBlack.withAlpha (0.78f));
    g.fillRoundedRectangle (navBox.toFloat(), 8.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (navBox.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void MainHeader::resized()
{
    art.setBounds (getLocalBounds());
    auto r = getLocalBounds();
    navBox = juce::Rectangle<int> (296, 12, 420, 50);
    auto n = navBox.reduced (4);
    const int tw = n.getWidth() / 3;
    for (auto* b : tabs) b->setBounds (n.removeFromLeft (tw).reduced (2, 0));
    fmTab.setBounds (n.reduced (2, 0));
    auto mid = juce::Rectangle<int> (navBox.getRight() + 10, 0, 0, getHeight()).withRight (getWidth() - 372);
    auto row = mid.reduced (0, 22);
    initButton.setBounds (row.removeFromLeft (50));
    row.removeFromLeft (6);
    for (int i : { (int) PresetsPanel, (int) AIPanel, (int) MacrosPanel, (int) KeysPanel })
        panelButtons[i]->setBounds (row.removeFromLeft (54));
    row.removeFromLeft (10);
    panelBox = {};
    // output section: MASTER VOL, meters + CLIP, PANIC, FX BYPASS
    outBox = juce::Rectangle<int> (row.getX(), 6, 0, getHeight() - 12).withRight (mid.getRight());
    auto o = outBox.reduced (4, 0);
    masterVol->setBounds (o.removeFromLeft (58));
    o.removeFromLeft (4);
    meter.setBounds (o.removeFromLeft (22).reduced (0, 6));
    o.removeFromLeft (8);
    auto btn = o.reduced (0, 16);
    panicB.setBounds (btn.removeFromLeft (34));
    btn.removeFromLeft (6);
    fxBypass.setBounds (btn.removeFromLeft (std::max (60, btn.getWidth())));
    auto right = r.removeFromRight (372).reduced (8, 20);
    help.setBounds (right.removeFromRight (34));
    settings.setBounds (right.removeFromRight (34));
    right.removeFromRight (6);
    cpu.setBounds (right.removeFromRight (70));
    right.removeFromRight (4);
    redo.setBounds (right.removeFromRight (30));
    undo.setBounds (right.removeFromRight (30));
    fav.setBounds (right.removeFromRight (32));
    right.removeFromRight (6);
    sizeButton.setBounds (right.removeFromRight (90).reduced (0, 3));
}

// =================================================================================
PresetHero::PresetHero (ViceSynthProcessor& p) : proc (p)
{
    // the artwork is painted manually (not as a visible child) so the fade, title and
    // waveform can be layered over it while the buttons stay on top
    art.setInterceptsMouseClicks (false, false);
    for (auto* b : { &prev, &next, &fav, &panic }) addAndMakeVisible (b);
    prev.onClick = [this] { proc.stepPreset (-1); };
    next.onClick = [this] { proc.stepPreset (1); };
    fav.onClick = [this] { toggleFavourite (proc); refresh(); };
    panic.onClick = [this] { proc.panic(); };
    panic.showBackground = true;
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
