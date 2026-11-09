#include "HardwarePanels.h"

namespace
{
const juce::Colour kBody (0xff2b2d31), kBodyDark (0xff1a1b1e), kInk (0xffe8e4dc), kLine (0xff4a4d53);
const juce::Colour kJdAccent (0xffe0a040), kD50Accent (0xff4fb3c8);
const juce::Colour kLcdBg (0xff0e1f14), kLcdInk (0xff8cf0a8);

juce::String jdShortLabel (const char* full)
{
    // Compact slider caption from the document's parameter name
    juce::String s (full);
    for (auto* p : { "LFO-1 ", "LFO-2 ", "LFO 1 ", "LFO 2 ", "Pitch ENV ", "Pitch Env ", "P-Env ", "TVF Env ", "TVA Env ", "ENV ", "TVF ", "TVA ", "WG " })
        if (s.startsWith (p)) { s = s.substring ((int) std::strlen (p)); break; }
    return s.replace ("Velocity", "Velo").replace ("Time Velo", "T-Velo").replace ("Key Follow", "KF")
            .replace ("Aftertouch", "AT").replace ("Sustain", "Sus").replace ("Resonance", "Reso");
}
} // namespace

// ===========================================================================
// JD-800 panel
const std::vector<JdPanel::Section>& JdPanel::sections()
{
    using namespace jd800::tone;
    static const std::vector<Section> s {
        { "LFO 1",          { Lfo1Rate, Lfo1Delay, Lfo1Fade }, 0 },
        { "LFO 2",          { Lfo2Rate, Lfo2Rate + 1, Lfo2Rate + 2 }, 0 },
        { "WG",             { PitchCoarse, PitchFine, PitchRandom, Lfo1Sens, Lfo2Sens, LeverSens, AftertouchModSens }, 0 },
        { "PITCH ENVELOPE", { PEnvVelo, PEnvTimeVelo, PEnvTimeKf, PEnvL0, PEnvT1, PEnvL1, PEnvT2, PEnvT3, PEnvL2 }, 0 },
        { "TVF",            { Cutoff, Resonance, CutoffKf, TvfAftertouch, TvfLfoDepth, TvfEnvDepth }, 1 },
        { "TVF ENVELOPE",   { TvfEnvVelo, TvfEnvTimeVelo, TvfEnvTimeKf, TvfT1, TvfL1, TvfT2, TvfL2, TvfT3, TvfSustain, TvfT4, TvfL4 }, 1 },
        { "TVA",            { BiasPoint, BiasLevel, Level, TvaAftertouch, TvaLfoDepth }, 1 },
        { "TVA ENVELOPE",   { TvaEnvVelo, TvaEnvTimeVelo, TvaEnvTimeKf, TvaT1, TvaL1, TvaT2, TvaL2, TvaT3, TvaSustain, TvaT4 }, 1 },
    };
    return s;
}

JdPanel::JdPanel (DigitalComboProcessor& p) : proc (p)
{
    for (auto& sec : sections())
        for (int off : sec.offsets)
        {
            SliderItem it;
            it.offset = off;
            const auto& info = jd800::paramForOffset (jd800::kToneOffset[0] + off);
            it.label = jdShortLabel (info.name);
            it.slider = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            it.slider->setRange (info.minValue, info.maxValue, 1.0);
            it.slider->setTitle (juce::String ("JD-800 ") + info.name);
            it.slider->setColour (juce::Slider::trackColourId, kJdAccent.withAlpha (0.8f));
            it.slider->setColour (juce::Slider::backgroundColourId, kBodyDark);
            it.slider->setColour (juce::Slider::thumbColourId, kInk);
            auto* sl = it.slider.get();
            sl->onValueChange = [this, off, sl] { if (! updating) writeToActiveTones (off, (int) sl->getValue()); };
            addAndMakeVisible (*sl);
            sliders.push_back (std::move (it));
        }

    for (int t = 0; t < 4; ++t)
    {
        const auto letter = juce::String::charToString (juce::juce_wchar ('A' + t));
        layerButtons[t].setButtonText ("LAYER " + letter);
        activeButtons[t].setButtonText ("ACTIVE " + letter);
        layerButtons[t].onClick = [this, t] { toggleCommonBit (jd800::common::LayerTone, t); };
        activeButtons[t].onClick = [this, t] { toggleCommonBit (jd800::common::ActiveTone, t); };
        for (auto* b : { &layerButtons[t], &activeButtons[t] })
        {
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, kJdAccent);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            addAndMakeVisible (*b);
        }
    }

    // Selectors that are switches/lists on the original (wave select, filter mode, LFO wave)
    for (int w = 0; w < jd800::kNumWaveforms; ++w)
        waveBox.addItem (juce::String (w + 1).paddedLeft ('0', 3) + " " + jd800::kWaveNames[w], w + 1);
    waveBox.setTitle ("JD-800 Waveform");
    waveBox.onChange = [this]
    {
        if (updating || waveBox.getSelectedId() <= 0) return;
        const int w = waveBox.getSelectedId() - 1;
        writeToActiveTones (jd800::tone::WaveMsb, w >> 7);
        writeToActiveTones (jd800::tone::WaveLsb, w & 127);
        writeToActiveTones (jd800::tone::WaveSource, 0); // internal waveform
    };
    addAndMakeVisible (waveBox);
    filterModeBox.addItemList ({ "HPF", "BPF", "LPF" }, 1);
    filterModeBox.setTitle ("JD-800 Filter Mode");
    filterModeBox.onChange = [this] { if (! updating) writeToActiveTones (jd800::tone::FilterMode, filterModeBox.getSelectedId() - 1); };
    addAndMakeVisible (filterModeBox);
    for (int l = 0; l < 2; ++l)
    {
        const int off = l == 0 ? jd800::tone::Lfo1Wave : jd800::tone::Lfo1Wave + 6;
        const auto& info = jd800::paramForOffset (jd800::kToneOffset[0] + off);
        for (int v = info.minValue; v <= info.maxValue; ++v)
            lfoWave[l].addItem (params::formatJdValue (jd800::kToneOffset[0] + off, v), v + 1);
        lfoWave[l].setTitle (juce::String ("JD-800 LFO ") + juce::String (l + 1) + " Waveform");
        lfoWave[l].onChange = [this, l, off] { if (! updating) writeToActiveTones (off, lfoWave[l].getSelectedId() - 1); };
        addAndMakeVisible (lfoWave[l]);
    }
    timerCallback();
    startTimerHz (15);
}

JdPanel::~JdPanel() { stopTimer(); }

int JdPanel::displayTone() const noexcept
{
    const int active = proc.getJdByte (jd800::kCommonOffset + jd800::common::ActiveTone);
    for (int t = 0; t < 4; ++t)
        if ((active >> t) & 1) return t;
    return 0;
}

void JdPanel::writeToActiveTones (int toneOffset, int value)
{
    const int active = proc.getJdByte (jd800::kCommonOffset + jd800::common::ActiveTone) & 15;
    juce::String tones;
    for (int t = 0; t < 4; ++t)
        if (((active >> t) & 1) || (active == 0 && t == 0))
        {
            proc.setJdByteFromUi (jd800::kToneOffset[t] + toneOffset, value);
            tones << juce::String::charToString (juce::juce_wchar ('A' + t));
        }
    const int po = jd800::kToneOffset[0] + toneOffset;
    readout = "TONE " + tones + "  " + juce::String (jd800::paramForOffset (po).name).toUpperCase() + "  "
              + params::formatJdValue (po, value);
    repaint();
}

void JdPanel::toggleCommonBit (int commonOffset, int bit)
{
    const int o = jd800::kCommonOffset + commonOffset;
    const int v = proc.getJdByte (o) ^ (1 << bit);
    proc.setJdByteFromUi (o, v & 15);
    timerCallback();
}

juce::Slider* JdPanel::sliderFor (int toneOffset) noexcept
{
    for (auto& s : sliders)
        if (s.offset == toneOffset) return s.slider.get();
    return nullptr;
}

void JdPanel::timerCallback()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    const int t = displayTone();
    const int base = jd800::kToneOffset[t];
    for (auto& s : sliders)
        if (! s.slider->isMouseButtonDown())
            s.slider->setValue (proc.getJdByte (base + s.offset), juce::dontSendNotification);
    const int layer = proc.getJdByte (jd800::kCommonOffset + jd800::common::LayerTone);
    const int active = proc.getJdByte (jd800::kCommonOffset + jd800::common::ActiveTone);
    for (int i = 0; i < 4; ++i)
    {
        layerButtons[i].setToggleState (((layer >> i) & 1) != 0, juce::dontSendNotification);
        activeButtons[i].setToggleState (((active >> i) & 1) != 0, juce::dontSendNotification);
    }
    const int wave = (proc.getJdByte (base + jd800::tone::WaveMsb) << 7) | proc.getJdByte (base + jd800::tone::WaveLsb);
    waveBox.setSelectedId (juce::jlimit (0, jd800::kNumWaveforms - 1, wave) + 1, juce::dontSendNotification);
    filterModeBox.setSelectedId (proc.getJdByte (base + jd800::tone::FilterMode) + 1, juce::dontSendNotification);
    lfoWave[0].setSelectedId (proc.getJdByte (base + jd800::tone::Lfo1Wave) + 1, juce::dontSendNotification);
    lfoWave[1].setSelectedId (proc.getJdByte (base + jd800::tone::Lfo1Wave + 6) + 1, juce::dontSendNotification);
}

void JdPanel::paint (juce::Graphics& g)
{
    g.fillAll (kBody);
    // Header strip with the readout window
    auto r = getLocalBounds().reduced (6);
    auto head = r.removeFromTop (56);
    g.setColour (kBodyDark);
    g.fillRoundedRectangle (head.toFloat(), 4.0f);
    auto lcd = head.reduced (8).removeFromRight (420);
    g.setColour (kLcdBg);
    g.fillRect (lcd);
    g.setColour (kLcdInk);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    const auto name = proc.getJdName();
    g.drawText ("JD-800  " + name, lcd.removeFromTop (lcd.getHeight() / 2).reduced (6, 0), juce::Justification::centredLeft);
    g.drawText (readout.isNotEmpty() ? readout : juce::String ("EDIT TONE ") + juce::String::charToString (juce::juce_wchar ('A' + displayTone())),
                lcd.reduced (6, 0), juce::Justification::centredLeft);
    g.setColour (kJdAccent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("JD-800 PANEL", head.reduced (10, 0).removeFromLeft (120), juce::Justification::centredLeft);

    for (auto& [frame, title] : sectionFrames)
    {
        g.setColour (kBodyDark.withAlpha (0.55f));
        g.fillRoundedRectangle (frame.toFloat(), 4.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (kJdAccent);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title, frame.withHeight (16).reduced (6, 0), juce::Justification::centredLeft);
    }
    g.setColour (kInk.withAlpha (0.85f));
    g.setFont (juce::FontOptions (9.5f));
    for (auto& s : sliders)
        g.drawFittedText (s.label, s.labelArea, juce::Justification::centredTop, 2, 0.7f);
    g.setColour (kInk.withAlpha (0.45f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("Sliders edit every ACTIVE tone. Layout follows the JD-800's slider sections; original drawing.",
                getLocalBounds().reduced (10, 4).removeFromBottom (14), juce::Justification::centredRight);
}

void JdPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto head = r.removeFromTop (56).reduced (8);
    head.removeFromLeft (130);
    head.removeFromRight (430);
    {
        auto top = head.removeFromTop (head.getHeight() / 2);
        for (int t = 0; t < 4; ++t) layerButtons[t].setBounds (top.removeFromLeft (top.getWidth() / (4 - t)).reduced (2, 1));
        for (int t = 0; t < 4; ++t) activeButtons[t].setBounds (head.removeFromLeft (head.getWidth() / (4 - t)).reduced (2, 1));
    }
    r.removeFromTop (4);
    r.removeFromBottom (16);
    auto selectors = r.removeFromTop (28);
    {
        auto lab = [] (juce::Rectangle<int>& row, int w) { return row.removeFromLeft (w); };
        lab (selectors, 4);
        waveBox.setBounds (lab (selectors, 260).reduced (2));
        lab (selectors, 8);
        filterModeBox.setBounds (lab (selectors, 90).reduced (2));
        lab (selectors, 8);
        lfoWave[0].setBounds (lab (selectors, 110).reduced (2));
        lfoWave[1].setBounds (lab (selectors, 110).reduced (2));
    }
    r.removeFromTop (4);
    sectionFrames.clear();
    const int rowH = r.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto band = r.removeFromTop (rowH).reduced (0, 2);
        int count = 0;
        for (auto& sec : sections()) if (sec.row == row) count += (int) sec.offsets.size();
        const int gaps = 6;
        int nSecs = 0;
        for (auto& sec : sections()) if (sec.row == row) ++nSecs;
        const float colW = float (band.getWidth() - gaps * nSecs) / float (count);
        float x = (float) band.getX();
        size_t sliderIndex = 0;
        for (auto& sec : sections())
        {
            if (sec.row != row) { sliderIndex += sec.offsets.size(); continue; }
            const int w = int (colW * float (sec.offsets.size()));
            juce::Rectangle<int> frame ((int) x, band.getY(), w, band.getHeight());
            sectionFrames.push_back ({ frame, sec.name });
            auto inner = frame.reduced (2).withTrimmedTop (16);
            for (size_t k = 0; k < sec.offsets.size(); ++k, ++sliderIndex)
            {
                auto cell = juce::Rectangle<int> (inner.getX() + int (colW * float (k)), inner.getY(), int (colW), inner.getHeight());
                auto& item = sliders[sliderIndex];
                item.labelArea = cell.removeFromBottom (26).reduced (1, 0);
                item.slider->setBounds (cell.reduced (2, 2));
            }
            x += float (w + gaps);
        }
    }
}

// ===========================================================================
// Joystick
void JoystickPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (kBodyDark);
    g.fillEllipse (r);
    g.setColour (kLine);
    g.drawEllipse (r, 1.5f);
    g.drawLine (r.getCentreX(), r.getY() + 6, r.getCentreX(), r.getBottom() - 6, 0.8f);
    g.drawLine (r.getX() + 6, r.getCentreY(), r.getRight() - 6, r.getCentreY(), 0.8f);
    const float rad = r.getWidth() * 0.5f - 12.0f;
    const auto c = r.getCentre() + juce::Point<float> (px * rad, -py * rad);
    g.setColour (kD50Accent);
    g.fillEllipse (c.x - 10, c.y - 10, 20, 20);
    g.setColour (kInk);
    g.drawEllipse (c.x - 10, c.y - 10, 20, 20, 1.2f);
}

void JoystickPad::mouseDrag (const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat().reduced (14.0f);
    const float x = juce::jlimit (-1.0f, 1.0f, (e.position.x - r.getCentreX()) / (r.getWidth() * 0.5f));
    const float y = juce::jlimit (-1.0f, 1.0f, -(e.position.y - r.getCentreY()) / (r.getHeight() * 0.5f));
    setPosition (x, y, true);
}

void JoystickPad::mouseUp (const juce::MouseEvent&)
{
    if (springBack) setPosition (0.0f, 0.0f, true);
}

void JoystickPad::setPosition (float x, float y, bool notify)
{
    px = juce::jlimit (-1.0f, 1.0f, x);
    py = juce::jlimit (-1.0f, 1.0f, y);
    repaint();
    if (notify && onMove) onMove (px, py);
}

// ===========================================================================
// D-50 panel
const std::vector<D50Panel::Group>& D50Panel::groupsFor (int kind)
{
    static const std::vector<Group> partial {
        { "WG PITCH", { 0, 1, 2 } },              { "WG MOD", { 3, 4, 5 } },
        { "WG FORM", { 6, 7 } },                  { "WG PW", { 8, 9, 10, 11, 12 } },
        { "TVF", { 13, 14, 15, 16, 17 } },        { "TVF ENV", { 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 } },
        { "TVF MOD", { 32, 33, 34 } },            { "TVA", { 35, 36, 37, 38 } },
        { "TVA ENV", { 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50 } }, { "TVA MOD", { 51, 52, 53 } },
    };
    static const std::vector<Group> common {
        { "STRUCTURE", { 10, 46, 47 } },          { "P-ENV", { 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 } },
        { "P-MOD", { 22, 23, 24 } },              { "LFO-1", { 25, 26, 27, 28 } },
        { "LFO-2", { 29, 30, 31, 32 } },          { "LFO-3", { 33, 34, 35, 36 } },
        { "EQ", { 37, 38, 39, 40, 41 } },         { "CHORUS", { 42, 43, 44, 45 } },
    };
    static const std::vector<Group> patch {
        { "KEY MODE", { 18, 19, 33 } },           { "PORTAMENTO", { 20, 28 } },
        { "HOLD / BEND", { 21, 26, 27 } },        { "KEY SHIFT", { 22, 23, 24, 25 } },
        { "OUTPUT / REVERB", { 29, 30, 31, 32 } }, { "CHASE", { 34, 35, 36 } },
        { "MIDI", { 37, 38, 39 } },
    };
    return kind == 0 ? partial : (kind == 1 ? common : patch);
}

D50Panel::D50Panel (DigitalComboProcessor& p) : proc (p)
{
    const char* toneNames[] = { "UPPER", "LOWER" };
    const char* partNames[] = { "PARTIAL 1", "PARTIAL 2", "COMMON" };
    for (int i = 0; i < 2; ++i)
    {
        toneButtons[i].setButtonText (toneNames[i]);
        toneButtons[i].onClick = [this, i] { selectBlock (block == 6 ? i * 3 + 2 : i * 3 + block % 3); };
    }
    for (int i = 0; i < 3; ++i)
    {
        partButtons[i].setButtonText (partNames[i]);
        partButtons[i].onClick = [this, i] { selectBlock (block == 6 ? i : (block / 3) * 3 + i); };
    }
    patchButton.onClick = [this] { selectBlock (6); };
    for (juce::TextButton* b : { &toneButtons[0], &toneButtons[1], &partButtons[0], &partButtons[1], &partButtons[2], &patchButton })
    {
        b->setColour (juce::TextButton::buttonOnColourId, kD50Accent);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (*b);
    }
    cursorLeft.onClick = [this] { selectParam (param - 1); };
    cursorRight.onClick = [this] { selectParam (param + 1); };
    incButton.onClick = [this] { increment (+1); };
    decButton.onClick = [this] { increment (-1); };
    incButton.setRepeatSpeed (400, 60);
    decButton.setRepeatSpeed (400, 60);
    for (auto* b : { &cursorLeft, &cursorRight, &incButton, &decButton })
        addAndMakeVisible (*b);

    dataEntry.setTitle ("D-50 Data Entry");
    dataEntry.setColour (juce::Slider::trackColourId, kD50Accent.withAlpha (0.8f));
    dataEntry.setColour (juce::Slider::backgroundColourId, kBodyDark);
    dataEntry.onValueChange = [this]
    {
        if (updating) return;
        proc.setD50ByteFromUi (getSelectedOffset(), (int) dataEntry.getValue());
        refreshLcd();
    };
    addAndMakeVisible (dataEntry);

    balanceMode.onClick = [this] { setJoystickMode (false); };
    valueMode.onClick = [this] { setJoystickMode (true); };
    for (auto* b : { &balanceMode, &valueMode })
    {
        b->setColour (juce::TextButton::buttonOnColourId, kD50Accent);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (*b);
    }
    joystick.setTitle ("D-50 Joystick");
    joystick.onMove = [this] (float x, float y)
    {
        if (joystick.springBack)
        {
            joyRepeat = y; // VALUE: up = INC, down = DEC, speed from the deflection
            return;
        }
        // BALANCE (Owner's Manual: left = Partial 1 louder, up = Upper tone louder)
        const int partialBal = juce::roundToInt ((x + 1.0f) * 50.0f);
        const int toneBal = juce::roundToInt ((y + 1.0f) * 50.0f);
        proc.setD50ByteFromUi ((int) d50::kUpperCommon + d50::common::PartialBalance, partialBal);
        proc.setD50ByteFromUi ((int) d50::kLowerCommon + d50::common::PartialBalance, partialBal);
        proc.setD50ByteFromUi ((int) d50::kPatchFactors + d50::patch::ToneBalance, toneBal);
        refreshLcd();
    };
    addAndMakeVisible (joystick);
    setJoystickMode (false);

    selectBlock (0);
    startTimerHz (15);
}

D50Panel::~D50Panel() { stopTimer(); }

int D50Panel::blockBase() const noexcept
{
    static const uint32_t bases[7] = { d50::kUpperPartial1, d50::kUpperPartial2, d50::kUpperCommon,
                                       d50::kLowerPartial1, d50::kLowerPartial2, d50::kLowerCommon, d50::kPatchFactors };
    return (int) bases[block];
}

int D50Panel::getSelectedOffset() const noexcept
{
    const auto& gs = groupsFor (blockKind());
    const auto& offs = gs[(size_t) juce::jlimit (0, (int) gs.size() - 1, group)].offsets;
    return blockBase() + offs[(size_t) juce::jlimit (0, (int) offs.size() - 1, param)];
}

void D50Panel::selectBlock (int b)
{
    block = juce::jlimit (0, 6, b);
    group = 0;
    param = 0;
    for (int i = 0; i < 2; ++i) toneButtons[i].setToggleState (block != 6 && block / 3 == i, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) partButtons[i].setToggleState (block != 6 && block % 3 == i, juce::dontSendNotification);
    patchButton.setToggleState (block == 6, juce::dontSendNotification);
    rebuildGroupButtons();
    selectGroup (0);
}

void D50Panel::rebuildGroupButtons()
{
    groupButtons.clear();
    const auto& gs = groupsFor (blockKind());
    for (int i = 0; i < (int) gs.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (gs[(size_t) i].name);
        b->setColour (juce::TextButton::buttonOnColourId, kD50Accent);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b->onClick = [this, i] { selectGroup (i); };
        addAndMakeVisible (*b);
        groupButtons.push_back (std::move (b));
    }
    resized();
}

void D50Panel::selectGroup (int g)
{
    group = juce::jlimit (0, (int) groupButtons.size() - 1, g);
    for (int i = 0; i < (int) groupButtons.size(); ++i)
        groupButtons[(size_t) i]->setToggleState (i == group, juce::dontSendNotification);
    selectParam (0);
}

void D50Panel::selectParam (int i)
{
    const auto& offs = groupsFor (blockKind())[(size_t) group].offsets;
    param = juce::jlimit (0, (int) offs.size() - 1, i);
    const int o = getSelectedOffset();
    const juce::ScopedValueSetter<bool> svs (updating, true);
    dataEntry.setRange (0.0, (double) d50::maxValueForTempOffset (o), 1.0);
    dataEntry.setValue (proc.getD50Byte (o), juce::dontSendNotification);
    refreshLcd();
}

void D50Panel::increment (int delta)
{
    const int o = getSelectedOffset();
    const int v = juce::jlimit (0, (int) d50::maxValueForTempOffset (o), proc.getD50Byte (o) + delta);
    proc.setD50ByteFromUi (o, v);
    const juce::ScopedValueSetter<bool> svs (updating, true);
    dataEntry.setValue (v, juce::dontSendNotification);
    refreshLcd();
}

void D50Panel::setJoystickMode (bool value)
{
    joystick.springBack = value;
    balanceMode.setToggleState (! value, juce::dontSendNotification);
    valueMode.setToggleState (value, juce::dontSendNotification);
    joyRepeat = 0.0f;
    if (value)
        joystick.setPosition (0.0f, 0.0f, false);
    else
    {
        const float x = float (proc.getD50Byte ((int) d50::kUpperCommon + d50::common::PartialBalance)) / 50.0f - 1.0f;
        const float y = float (proc.getD50Byte ((int) d50::kPatchFactors + d50::patch::ToneBalance)) / 50.0f - 1.0f;
        joystick.setPosition (x, y, false);
    }
}

static juce::String d50ParamName (int absOffset)
{
    const auto kind = d50::blockForTempOffset (absOffset);
    const int local = absOffset % 64;
    if (kind == d50::Block::UpperCommon || kind == d50::Block::LowerCommon) return d50::commonParam (local).name;
    if (kind == d50::Block::Patch) return d50::patchParam (local).name;
    return d50::partialParam (local).name;
}

juce::String D50Panel::getLcdLine (int line) const { return lcd[juce::jlimit (0, 1, line)]; }

void D50Panel::refreshLcd()
{
    static const char* blockNames[7] = { "U-P1", "U-P2", "U-COM", "L-P1", "L-P2", "L-COM", "PATCH" };
    const auto& gs = groupsFor (blockKind());
    const auto& offs = gs[(size_t) group].offsets;
    const int base = blockBase();
    juce::String l1;
    juce::String name;
    if (block == 6) name = proc.getD50PatchName();
    else name = proc.getD50Name (blockKind() == 1 ? base : (block < 3 ? (int) d50::kUpperCommon : (int) d50::kLowerCommon), 10);
    l1 << blockNames[block] << " " << name.trimEnd().substring (0, 18) << "  " << gs[(size_t) group].name;
    if (block != 6)
        l1 << "  Str" << juce::String (proc.getD50Byte ((block < 3 ? (int) d50::kUpperCommon : (int) d50::kLowerCommon) + d50::common::Structure) + 1).paddedLeft ('0', 2);

    // Line 2: a window of up to 3 parameters around the cursor; the selected one in [brackets]
    const int first = juce::jlimit (0, juce::jmax (0, (int) offs.size() - 3), param - 1);
    juce::String l2;
    for (int i = first; i < juce::jmin ((int) offs.size(), first + 3); ++i)
    {
        const int o = base + offs[(size_t) i];
        juce::String item = d50ParamName (o) + ":" + params::formatD50Value (o, proc.getD50Byte (o));
        l2 << (i == param ? "[" + item + "]" : " " + item + " ") << " ";
    }
    if (lcd[0] != l1 || lcd[1] != l2)
    {
        lcd[0] = l1;
        lcd[1] = l2.trimEnd();
        repaint (lcdArea);
    }
}

void D50Panel::timerCallback()
{
    if (joystick.springBack && std::abs (joyRepeat) > 0.15f)
        increment (joyRepeat > 0 ? juce::jmax (1, int (std::abs (joyRepeat) * 3.0f)) : -juce::jmax (1, int (std::abs (joyRepeat) * 3.0f)));
    if (! dataEntry.isMouseButtonDown())
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        dataEntry.setValue (proc.getD50Byte (getSelectedOffset()), juce::dontSendNotification);
    }
    refreshLcd();
}

void D50Panel::paint (juce::Graphics& g)
{
    g.fillAll (kBody);
    auto r = getLocalBounds().reduced (6);
    g.setColour (kBodyDark);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);

    // LCD window: 2 lines, backlit green, monospaced like the original's character display
    g.setColour (juce::Colour (0xff050807));
    g.fillRoundedRectangle (lcdArea.toFloat().expanded (6.0f), 5.0f);
    g.setColour (kLcdBg);
    g.fillRect (lcdArea);
    g.setColour (kLcdInk);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::plain));
    auto lines = lcdArea.reduced (10, 6);
    g.drawFittedText (lcd[0], lines.removeFromTop (lines.getHeight() / 2), juce::Justification::centredLeft, 1, 0.6f);
    g.drawFittedText (lcd[1], lines, juce::Justification::centredLeft, 1, 0.6f);

    g.setColour (kD50Accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("D-50 PANEL", r.reduced (14, 8).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (kInk.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    auto labels = [&] (juce::Component& c, const char* text)
    {
        g.drawText (text, c.getBounds().translated (0, -16).withHeight (14).expanded (40, 0), juce::Justification::centred);
    };
    labels (dataEntry, "DATA ENTRY");
    labels (joystick, "JOYSTICK");
    if (! groupButtons.empty())
        g.drawText ("PARAMETER", groupButtons.front()->getBounds().translated (0, -16).withWidth (200).withHeight (14), juce::Justification::centredLeft);
    g.drawText ("TONE / PARTIAL SELECT", toneButtons[0].getBounds().translated (0, -16).withWidth (300).withHeight (14), juce::Justification::centredLeft);
    g.setColour (kInk.withAlpha (0.45f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("Joystick BALANCE: left = Partial 1 louder, up = Upper louder (Owner's Manual p.18). VALUE: up = INC, down = DEC. Original drawing.",
                getLocalBounds().reduced (14, 10).removeFromBottom (14), juce::Justification::centredLeft);
}

void D50Panel::resized()
{
    auto r = getLocalBounds().reduced (6).reduced (14, 10);
    r.removeFromTop (24);
    r.removeFromBottom (18);
    auto right = r.removeFromRight (330);
    auto joyArea = right.removeFromRight (190);
    joyArea.removeFromTop (40);
    balanceMode.setBounds (joyArea.removeFromTop (28).removeFromLeft (92).reduced (2));
    valueMode.setBounds (balanceMode.getBounds().translated (94, 0));
    joyArea.removeFromTop (24);
    joystick.setBounds (joyArea.removeFromTop (juce::jmin (180, joyArea.getWidth())).withSizeKeepingCentre (176, 176));
    right.removeFromTop (40);
    auto dataCol = right.reduced (10, 0);
    auto incDec = dataCol.removeFromBottom (70);
    incButton.setBounds (incDec.removeFromTop (34).reduced (4, 2));
    decButton.setBounds (incDec.reduced (4, 2));
    dataEntry.setBounds (dataCol.withTrimmedTop (16).withSizeKeepingCentre (60, dataCol.getHeight() - 16));

    r.removeFromRight (10);
    lcdArea = r.removeFromTop (78).withTrimmedTop (8);
    r.removeFromTop (14);
    auto cursorRow = r.removeFromTop (34);
    cursorLeft.setBounds (cursorRow.removeFromLeft (60).reduced (2));
    cursorRight.setBounds (cursorRow.removeFromLeft (60).reduced (2));
    r.removeFromTop (22);
    auto selRow = r.removeFromTop (36);
    for (auto& b : toneButtons) b.setBounds (selRow.removeFromLeft (90).reduced (2));
    selRow.removeFromLeft (12);
    for (auto& b : partButtons) b.setBounds (selRow.removeFromLeft (100).reduced (2));
    selRow.removeFromLeft (12);
    patchButton.setBounds (selRow.removeFromLeft (90).reduced (2));
    r.removeFromTop (26);
    // Parameter group buttons in rows of 5
    const int perRow = 5;
    const int bw = r.getWidth() / perRow;
    for (int i = 0; i < (int) groupButtons.size(); ++i)
        groupButtons[(size_t) i]->setBounds (r.getX() + (i % perRow) * bw, r.getY() + (i / perRow) * 40, bw, 38);
    for (auto& b : groupButtons) b->setBounds (b->getBounds().reduced (3));
}
