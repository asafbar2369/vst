#include "CloudUI.h"

using namespace cloud;

// ===========================================================================
CloudLookAndFeel::CloudLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, jdCol.withAlpha (0.4f));
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, card);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cardHi);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, good);
    setColour (juce::TextButton::buttonColourId, cardHi);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ScrollBar::thumbColourId, line.brighter (0.3f));
    setColour (juce::TooltipWindow::backgroundColourId, card);
    setColour (juce::TooltipWindow::textColourId, text);
    // classic widgets used by the "Advanced" pages
    setColour (juce::TabbedButtonBar::tabTextColourId, text);
    setColour (juce::TabbedComponent::backgroundColourId, panel);
    setColour (juce::TabbedComponent::outlineColourId, line);
    setColour (juce::TextEditor::backgroundColourId, bg);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, line);
}

void CloudLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = a0 + pos * (a1 - a0);
    const float trackR = r - 3.0f;
    const auto acc = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, a0, a1, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val;
    val.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, a0, angle, true);
    g.setColour (s.isEnabled() ? acc : acc.withAlpha (0.4f));
    g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = r - 9.0f;
    juce::ColourGradient grad (juce::Colour (0xff4a505a), c.x, c.y - br, juce::Colour (0xff1b1e23), c.x, c.y + br, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (juce::Colour (0xff0b0c0e));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.8f, -br + 3.0f, 3.6f, br * 0.55f, 1.6f);
    g.setColour (text);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void CloudLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto acc = s.findColour (juce::Slider::rotarySliderFillColourId);
    if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (line);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
        g.setColour (acc);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, pos - (float) x, 6.0f, 3.0f);
        g.setColour (text);
        g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        g.setColour (juce::Colour (0xff0b0c0e));
        g.drawEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f, 1.2f);
    }
    else
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, pos, pos, style, s);
}

void CloudLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgCol, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : bgCol;
    if (! b.isEnabled()) fill = fill.withAlpha (0.45f);
    else if (down) fill = fill.darker (0.2f);
    else if (highlighted) fill = fill.brighter (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? fill.brighter (0.25f) : line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void CloudLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin (16.0f, r.getHeight() - 4.0f);
    auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 4.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    const auto acc = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? acc : offLed);
    g.fillEllipse (led);
    if (on)
    {
        g.setColour (acc.withAlpha (0.28f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (juce::Colour (0xff0b0c0e));
    g.drawEllipse (led, 1.0f);
    g.setColour (highlighted ? text : dim.brighter (0.2f));
    g.setFont (juce::FontOptions (kLabelPx, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (d + 10.0f), juce::Justification::centredLeft, true);
}

void CloudLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? jdCol : line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

// ===========================================================================
juce::String shortCaption (juce::String n, const juce::StringArray& strip)
{
    for (auto& p : strip)
        if (n.startsWithIgnoreCase (p)) { n = n.substring (p.length()); break; }
    n = n.trim();
    if (n.matchesWildcard ("Time ?", true) || n.matchesWildcard ("Time ??", true)) return "T" + n.fromLastOccurrenceOf (" ", false, false);
    if (n.matchesWildcard ("Level ?", true)) return "L" + n.fromLastOccurrenceOf (" ", false, false);
    return n.replace ("Velocity", "Velo").replace ("Keyfollow", "Key Fol.").replace ("Key follow", "Key Fol.").replace ("Key Follow", "Key Fol.")
            .replace ("After touch", "A-touch").replace ("Aftertouch", "A-touch").replace ("Frequency", "Freq")
            .replace ("Resonance", "Reso").replace ("Sustain level", "Sustain").replace ("Sustain Level", "Sustain")
            .replace ("End Level", "End").replace ("Select", "Sel.").replace ("Waveform", "Wave").replace ("Wave Form", "Wave")
            .replace ("Portamento", "Porta").replace ("Frequency", "Freq");
}

// ===========================================================================
namespace
{
int toneForJdOffset (int off) noexcept
{
    for (int t = 0; t < 4; ++t)
        if (off >= jd800::kToneOffset[t] && off < jd800::kToneOffset[t] + jd800::kToneSize) return t;
    return 0;
}
}

ByteControl::ByteControl (DigitalComboProcessor& p, ByteSpec s) : proc (p), spec (std::move (s))
{
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (kLabelPx));
    label.setColour (juce::Label::textColourId, dim.brighter (0.25f));
    label.setMinimumHorizontalScale (0.7f);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    build();
    startTimerHz (6);
}

ByteControl::~ByteControl() { stopTimer(); }

void ByteControl::loadRange()
{
    if (spec.kind == ByteSpec::Wave) { lo = 0; hi = jd800::kNumWaveforms - 1; }
    else if (spec.jd) { const auto& i = jd800::paramForOffset (spec.offset); lo = i.minValue; hi = i.maxValue; }
    else { lo = 0; hi = (int) d50::maxValueForTempOffset (spec.offset); }
}

juce::String ByteControl::format (int v) const
{
    if (spec.kind == ByteSpec::Wave)
        return juce::String (v + 1).paddedLeft ('0', 3) + " " + jd800::kWaveNames[juce::jlimit (0, jd800::kNumWaveforms - 1, v)];
    return spec.jd ? params::formatJdValue (spec.offset, v) : params::formatD50Value (spec.offset, v);
}

juce::String ByteControl::defaultCaption() const
{
    const juce::String name = spec.jd ? juce::String (jd800::paramForOffset (spec.offset).name)
                                      : [this]
    {
        const int local = spec.offset % d50::kBlockSize;
        const auto kind2 = d50::blockForTempOffset (spec.offset);
        if (kind2 == d50::Block::UpperCommon || kind2 == d50::Block::LowerCommon) return juce::String (d50::commonParam (local).name);
        if (kind2 == d50::Block::Patch) return juce::String (d50::patchParam (local).name);
        return juce::String (d50::partialParam (local).name);
    }();
    return shortCaption (name, {});
}

ByteSpec::Kind ByteControl::detect() const
{
    if (spec.kind != ByteSpec::Auto) return spec.kind;
    if (hi - lo == 1)
    {
        const auto t = format (lo).toUpperCase();
        return (t == "OFF" || t == "0") ? ByteSpec::Toggle : ByteSpec::Combo;
    }
    bool numeric = true;
    for (int v = lo; v <= hi && numeric; ++v)
        numeric = format (v).containsOnly ("+-0123456789.% ");
    return (! numeric && hi - lo <= 40) ? ByteSpec::Combo : ByteSpec::Knob;
}

void ByteControl::bindParam()
{
    attachment.reset();
    param = nullptr;
    paramId = {};
    if (spec.kind == ByteSpec::Wave) paramId = params::jdToneId (toneForJdOffset (spec.offset), "wave");
    else paramId = spec.jd ? params::jdParamIdForOffset (spec.offset) : params::d50ParamIdForOffset (spec.offset);
    if (paramId.isNotEmpty()) param = proc.apvts.getParameter (paramId);
    if (param != nullptr)
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { showValue ((int) std::lround (v)); });
}

int ByteControl::readByte() const
{
    if (spec.kind == ByteSpec::Wave)
        return juce::jlimit (0, jd800::kNumWaveforms - 1, ((int) proc.getJdByte (spec.offset) << 7) | (int) proc.getJdByte (spec.offset + 1));
    return spec.jd ? (int) proc.getJdByte (spec.offset) : (int) proc.getD50Byte (spec.offset);
}

void ByteControl::writeByte (int v)
{
    if (spec.kind == ByteSpec::Wave)
    {
        proc.setJdByteFromUi (spec.offset - 1, 0);                 // wave source: internal
        proc.setJdByteFromUi (spec.offset, v >> 7);
        proc.setJdByteFromUi (spec.offset + 1, v & 127);
    }
    else if (spec.jd) proc.setJdByteFromUi (spec.offset, v);
    else proc.setD50ByteFromUi (spec.offset, v);
}

int ByteControl::getValue() const { return readByte(); }

void ByteControl::commit (int v, bool partOfGesture)
{
    v = juce::jlimit (lo, hi, v);
    if (attachment != nullptr)
    {
        if (partOfGesture) attachment->setValueAsPartOfGesture ((float) v);
        else attachment->setValueAsCompleteGesture ((float) v);
    }
    writeByte (v);                                               // the instrument follows at once
    shown = v;
}

void ByteControl::setValue (int v)
{
    proc.pushUndoPoint();
    commit (v, false);
    showValue (juce::jlimit (lo, hi, v));
}

void ByteControl::showValue (int v)
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    shown = v;
    if (slider != nullptr) { slider->setValue (v, juce::dontSendNotification); slider->updateText(); }
    if (combo != nullptr) combo->setSelectedId (v - lo + 1, juce::dontSendNotification);
    if (toggle != nullptr) toggle->setToggleState (v > lo, juce::dontSendNotification);
}

juce::String ByteControl::getText() const { return format (shown); }

void ByteControl::build()
{
    loadRange();
    kind = detect();
    slider.reset(); combo.reset(); toggle.reset();
    bindParam();
    label.setText (spec.label.isNotEmpty() ? spec.label : defaultCaption(), juce::dontSendNotification);
    const auto acc = engineColour (spec.jd);

    if (kind == ByteSpec::Knob)
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
        slider->setRange ((double) lo, (double) hi, 1.0);
        slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, true, kCellW - 8, 20);
        slider->setColour (juce::Slider::rotarySliderFillColourId, acc);
        slider->setColour (juce::Slider::textBoxTextColourId, text);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setMouseDragSensitivity (160);
        slider->setDoubleClickReturnValue (true, (double) juce::jlimit (lo, hi, spec.jd ? params::jdInitValue (spec.offset) : params::d50InitValue (spec.offset)));
        slider->textFromValueFunction = [this] (double v) { return format ((int) std::lround (v)); };
        slider->setTitle (label.getText());
        slider->onDragStart = [this] { dragging = true; proc.pushUndoPoint(); if (attachment) attachment->beginGesture(); };
        slider->onDragEnd = [this] { if (attachment) attachment->endGesture(); dragging = false; };
        slider->onValueChange = [this]
        {
            if (updating) return;
            const int v = (int) std::lround (slider->getValue());
            if (dragging) commit (v, true);
            else { proc.pushUndoPoint(); commit (v, false); }
        };
        addAndMakeVisible (*slider);
    }
    else if (kind == ByteSpec::Combo || kind == ByteSpec::Wave)
    {
        combo = std::make_unique<juce::ComboBox>();
        for (int v = lo; v <= hi; ++v) combo->addItem (format (v), v - lo + 1);
        combo->setTitle (label.getText());
        combo->setJustificationType (juce::Justification::centredLeft);
        combo->onChange = [this]
        {
            if (updating) return;
            proc.pushUndoPoint();
            commit (combo->getSelectedId() - 1 + lo, false);
        };
        addAndMakeVisible (*combo);
    }
    else
    {
        toggle = std::make_unique<juce::ToggleButton> ("ON");
        toggle->setColour (juce::ToggleButton::tickColourId, acc);
        toggle->setTitle (label.getText());
        toggle->onClick = [this]
        {
            if (updating) return;
            proc.pushUndoPoint();
            commit (toggle->getToggleState() ? hi : lo, false);
        };
        addAndMakeVisible (*toggle);
    }
    showValue (readByte());
    resized();
}

void ByteControl::setOffset (int newOffset)
{
    if (newOffset == spec.offset) { showValue (readByte()); return; }
    spec.offset = newOffset;
    bindParam();
    showValue (readByte());
    if (spec.label.isEmpty()) label.setText (defaultCaption(), juce::dontSendNotification);
}

void ByteControl::setDimmed (bool d)
{
    if (d == dimmed) return;
    dimmed = d;
    setAlpha (d ? 0.38f : 1.0f);
}

void ByteControl::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    if (slider != nullptr) slider->setBounds (r.reduced (2, 0));
    if (combo != nullptr) combo->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 6, 400), 30));
    if (toggle != nullptr) toggle->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 6, 90), 30));
}

void ByteControl::refresh()
{
    if (dragging) return;
    const int v = readByte();
    if (v != shown) showValue (v);
}

void ByteControl::timerCallback()
{
    if (isShowing()) refresh();
}

// ===========================================================================
Card::Card (juce::String t, juce::Colour acc, int cols) : title (std::move (t)), accent (acc), columns (juce::jmax (1, cols))
{
    startTimerHz (8);
}

Card::~Card() { stopTimer(); }

void Card::addControl (std::unique_ptr<juce::Component> c, int colSpan, int rowSpan)
{
    colSpan = juce::jlimit (1, columns, colSpan);
    items.push_back ({ c.get(), colSpan, rowSpan });
    addAndMakeVisible (*c);
    owned.add (c.release());
}

int Card::getContentHeight() const noexcept
{
    int col = 0, row = 0, maxRow = 0;
    for (auto& it : items)
    {
        if (col + it.colSpan > columns) { col = 0; ++row; }
        maxRow = juce::jmax (maxRow, row + it.rowSpan);
        col += it.colSpan;
        if (col >= columns) { col = 0; ++row; }
    }
    return 34 + maxRow * kCellH + 10;
}

void Card::setEnableLed (std::function<bool()> get, std::function<void (bool)> set)
{
    getEnable = std::move (get);
    setEnable = std::move (set);
    haveLed = true;
    on = getEnable();
}

void Card::resized()
{
    auto r = getLocalBounds();
    ledArea = r.removeFromTop (30).removeFromRight (74).reduced (4, 4);
    r.removeFromTop (4);
    r = r.reduced (10, 0);
    int col = 0, row = 0;
    for (auto& it : items)
    {
        if (col + it.colSpan > columns) { col = 0; ++row; }
        it.comp->setBounds (r.getX() + col * kCellW, r.getY() + row * kCellH, it.colSpan * kCellW, it.rowSpan * kCellH);
        col += it.colSpan;
        if (col >= columns) { col = 0; ++row; }
    }
}

void Card::timerCallback()
{
    bool changed = false;
    if (getEnable)
    {
        const bool now = getEnable();
        if (now != on)
        {
            on = now;
            changed = true;
            for (auto& it : items) it.comp->setAlpha (on ? 1.0f : 0.42f);
        }
    }
    if (activity)
    {
        const bool now = on && activity();
        if (now != sounding) { sounding = now; changed = true; }
    }
    if (changed) repaint();
}

void Card::mouseDown (const juce::MouseEvent& e)
{
    if (haveLed && setEnable && ledArea.expanded (6).contains (e.getPosition()))
        clickEnableLed();
}

void Card::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (card);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (on ? line : line.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 9.0f, 1.0f);
    g.setColour (on ? accent : accent.withAlpha (0.45f));
    g.fillRoundedRectangle (r.getX() + 10.0f, r.getY() + 9.0f, 4.0f, 16.0f, 2.0f);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setColour (on ? text : dim);
    g.drawText (title, getLocalBounds().withTrimmedLeft (22).withTrimmedRight (haveLed ? 82 : (activity ? 28 : 10)).withHeight (34),
                juce::Justification::centredLeft, true);

    if (haveLed)
    {
        auto led = ledArea.toFloat();
        const float d = 15.0f;
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ led.getRight() - d * 0.5f - 2.0f, led.getCentreY() });
        g.setColour (on ? accent : offLed);
        g.fillEllipse (dot);
        if (on) { g.setColour (accent.withAlpha (0.28f)); g.fillEllipse (dot.expanded (3.0f)); }
        g.setColour (juce::Colour (0xff0b0c0e));
        g.drawEllipse (dot, 1.0f);
        g.setColour (on ? text : dim);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (on ? "ON" : "OFF", led.withRight (dot.getX() - 4.0f), juce::Justification::centredRight);
    }
    if (activity)
    {
        const float d = 9.0f;
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ (float) getWidth() - (haveLed ? 92.0f : 22.0f), 17.0f });
        g.setColour (sounding ? good : offLed);
        g.fillEllipse (dot);
        if (sounding) { g.setColour (good.withAlpha (0.3f)); g.fillEllipse (dot.expanded (3.0f)); }
    }
}

// ===========================================================================
EnvelopeGraph::EnvelopeGraph (std::function<std::vector<float>()> points, juce::Colour acc) : fetch (std::move (points)), accent (acc)
{
    setInterceptsMouseClicks (false, false);
    pts = fetch();
    startTimerHz (6);
}

EnvelopeGraph::~EnvelopeGraph() { stopTimer(); }

void EnvelopeGraph::refresh()
{
    auto now = fetch();
    if (now != pts) { pts = std::move (now); repaint(); }
}

void EnvelopeGraph::timerCallback()
{
    if (isShowing()) refresh();
}

void EnvelopeGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (line.withAlpha (0.55f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX() + 2.0f, r.getRight() - 2.0f);
    if (pts.size() < 4) return;
    auto in = r.reduced (8.0f, 8.0f);
    juce::Path p;
    for (size_t i = 0; i + 1 < pts.size(); i += 2)
    {
        const float x = in.getX() + pts[i] * in.getWidth(), y = in.getBottom() - pts[i + 1] * in.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    juce::Path fill (p);
    fill.lineTo (in.getRight(), in.getBottom());
    fill.lineTo (in.getX(), in.getBottom());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.16f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (size_t i = 2; i + 1 < pts.size(); i += 2)
        g.fillEllipse (in.getX() + pts[i] * in.getWidth() - 3.0f, in.getBottom() - pts[i + 1] * in.getHeight() - 3.0f, 6.0f, 6.0f);
}

// ===========================================================================
FlowPage::FlowPage() = default;
FlowPage::~FlowPage() = default;

int FlowPage::layoutFor (int width)
{
    int x = 10, y = 10, rowH = 0;
    for (auto* c : cards)
    {
        const int w = c->getContentWidth(), h = c->getContentHeight();
        if (x + w > width - 6 && x > 10) { x = 10; y += rowH + 10; rowH = 0; }
        c->setBounds (x, y, w, h);
        x += w + 10;
        rowH = juce::jmax (rowH, h);
    }
    return y + rowH + 10;
}

ScrollPage::ScrollPage()
{
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    viewport.setViewedComponent (&flow, false);
    addAndMakeVisible (viewport);
}

ScrollPage::~ScrollPage() { viewport.setViewedComponent (nullptr, false); }

void ScrollPage::resized()
{
    auto b = getLocalBounds();
    if (header != nullptr) header->setBounds (b.removeFromTop (headerHeight));
    viewport.setBounds (b);
    const int w = b.getWidth() - viewport.getScrollBarThickness();
    const int h = flow.layoutFor (w);
    flow.setSize (w, juce::jmax (h, b.getHeight()));
}
