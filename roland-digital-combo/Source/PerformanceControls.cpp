#include "PerformanceControls.h"

namespace
{
const juce::Colour kSlotDark (0xff0c0d0f), kWheelLight (0xff6a6f78), kWheelDark (0xff2a2d33), kInk (0xffe8e4dc), kAccent (0xffe0a040);
constexpr int kBendCentre = 8192;
}

// ===========================================================================
WheelControl::WheelControl (DigitalComboProcessor& p, Kind k) : proc (p), kind (k)
{
    param = proc.apvts.getParameter (getParameterId());
    setTitle (kind == Kind::Pitch ? "Pitch Bend wheel" : "Mod Wheel");
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    shown = liveValue();
    startTimerHz (30);
}

WheelControl::~WheelControl() { stopTimer(); }

juce::String WheelControl::getParameterId() const
{
    return kind == Kind::Pitch ? params::id::pitchBend : params::id::modWheel;
}

int WheelControl::liveValue() const noexcept
{
    return kind == Kind::Pitch ? proc.getPitchBendValue() : proc.getModWheelValue();
}

juce::Rectangle<float> WheelControl::slot() const noexcept
{
    return getLocalBounds().toFloat().reduced (getWidth() * 0.16f, 22.0f).withTrimmedBottom (14.0f);
}

int WheelControl::valueForY (float y) const noexcept
{
    const auto s = slot();
    const float t = juce::jlimit (0.0f, 1.0f, 1.0f - (y - s.getY()) / s.getHeight()); // top = 1
    return kind == Kind::Pitch ? juce::roundToInt (t * 16383.0f) : juce::roundToInt (t * 127.0f);
}

void WheelControl::dragToValue (int value)
{
    value = kind == Kind::Pitch ? juce::jlimit (0, 16383, value) : juce::jlimit (0, 127, value);
    if (! dragging)
    {
        dragging = true;
        if (param != nullptr) param->beginChangeGesture();
    }
    // Same route as a hardware wheel: a MIDI message for the audio thread ...
    if (kind == Kind::Pitch) proc.sendPerformanceMessage (juce::MidiMessage::pitchWheel (1, value));
    else                     proc.sendPerformanceMessage (juce::MidiMessage::controllerEvent (1, 1, value));
    // ... and the host parameter, so the move is recorded as automation.
    if (param != nullptr) param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
    shown = value;
    repaint();
}

void WheelControl::endDrag()
{
    if (! dragging) return;
    if (kind == Kind::Pitch)
    {
        proc.sendPerformanceMessage (juce::MidiMessage::pitchWheel (1, kBendCentre));   // a real wheel springs back
        if (param != nullptr) param->setValueNotifyingHost (param->convertTo0to1 ((float) kBendCentre));
        shown = kBendCentre;
    }
    if (param != nullptr) param->endChangeGesture();
    dragging = false;
    repaint();
}

void WheelControl::mouseDown (const juce::MouseEvent& e) { dragToValue (valueForY (e.position.y)); }
void WheelControl::mouseDrag (const juce::MouseEvent& e) { dragToValue (valueForY (e.position.y)); }
void WheelControl::mouseUp (const juce::MouseEvent&)     { endDrag(); }

void WheelControl::mouseDoubleClick (const juce::MouseEvent&)
{
    dragToValue (kind == Kind::Pitch ? kBendCentre : 0);
    endDrag();
}

void WheelControl::timerCallback()
{
    if (dragging) return;
    const int v = liveValue();
    if (v != shown) { shown = v; repaint(); }
}

juce::String WheelControl::getReadout() const
{
    if (kind == Kind::Mod)
        return juce::String (shown);
    const int d = shown - kBendCentre;
    return (d > 0 ? "+" : "") + juce::String (d);
}

void WheelControl::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kInk.withAlpha (0.85f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (kind == Kind::Pitch ? "PITCH" : "MOD", b.removeFromTop (20.0f), juce::Justification::centred);

    const auto s = slot();
    g.setColour (kSlotDark);
    g.fillRoundedRectangle (s.expanded (3.0f, 3.0f), 7.0f);

    // The wheel: a vertical cylinder with grooves that travel with the value.
    juce::ColourGradient grad (kWheelDark, s.getX(), 0.0f, kWheelDark, s.getRight(), 0.0f, false);
    grad.addColour (0.5, kWheelLight);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (s, 5.0f);
    const float t = kind == Kind::Pitch ? float (shown) / 16383.0f : float (shown) / 127.0f;   // 0 bottom .. 1 top
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    const float pitchGroove = 11.0f;
    const float phase = std::fmod (t * 14.0f * pitchGroove, pitchGroove);
    for (float y = s.getBottom() - phase; y > s.getY(); y -= pitchGroove)
        g.drawLine (s.getX() + 2.0f, y, s.getRight() - 2.0f, y, 1.2f);

    // Marker band at the current value; centre detent for the pitch wheel
    const float ty = s.getBottom() - t * s.getHeight();
    g.setColour (kAccent);
    g.fillRoundedRectangle (s.getX() - 3.0f, ty - 3.0f, s.getWidth() + 6.0f, 6.0f, 3.0f);
    if (kind == Kind::Pitch)
    {
        const float cy = s.getCentreY();
        g.setColour (kInk.withAlpha (0.55f));
        g.drawLine (s.getX() - 5.0f, cy, s.getX() - 1.0f, cy, 1.5f);
        g.drawLine (s.getRight() + 1.0f, cy, s.getRight() + 5.0f, cy, 1.5f);
    }

    g.setColour (kInk.withAlpha (0.9f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (getReadout(), getLocalBounds().toFloat().removeFromBottom (16.0f), juce::Justification::centred);
}

// ===========================================================================
MidiActivityLed::MidiActivityLed (DigitalComboProcessor& p) : proc (p)
{
    setTitle ("MIDI activity");
    lastCount = proc.getMidiInCount();
    startTimerHz (30);
}
MidiActivityLed::~MidiActivityLed() { stopTimer(); }

void MidiActivityLed::timerCallback()
{
    const auto c = proc.getMidiInCount();
    bool was = lit;
    if (c != lastCount) { lastCount = c; holdMs = 180; }
    else holdMs = std::max (0, holdMs - 33);
    lit = holdMs > 0;
    if (lit != was) repaint();
}

void MidiActivityLed::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float d = std::min (b.getWidth(), b.getHeight() - 14.0f);
    auto led = juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), b.getY() + d * 0.5f + 1.0f });
    g.setColour (lit ? juce::Colour (0xff59e07a) : juce::Colour (0xff1e3a26));
    g.fillEllipse (led);
    if (lit)
    {
        g.setColour (juce::Colour (0x5559e07a));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (kInk.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("MIDI", b.removeFromBottom (13.0f), juce::Justification::centred);
}

// ===========================================================================
LevelMeter::LevelMeter (DigitalComboProcessor& p) : proc (p)
{
    setTitle ("Output level");
    startTimerHz (30);
}
LevelMeter::~LevelMeter() { stopTimer(); }

void LevelMeter::timerCallback()
{
    bool changed = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float now = juce::Decibels::gainToDecibels (proc.getOutputPeak (ch), -90.0f);
        const float next = now > levelDb[ch] ? now : std::max (now, levelDb[ch] - 2.2f);    // fall about 66 dB/s
        if (std::abs (next - levelDb[ch]) > 0.05f) changed = true;
        levelDb[ch] = next;
    }
    if (changed) repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = b.removeFromTop (b.getHeight() / float (2 - ch)).reduced (0.0f, 1.5f);
        g.setColour (kSlotDark);
        g.fillRoundedRectangle (bar, 2.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (levelDb[ch] + 60.0f) / 60.0f);
        auto fill = bar.withWidth (bar.getWidth() * t);
        g.setColour (levelDb[ch] > -3.0f ? juce::Colour (0xffe05050) : (levelDb[ch] > -12.0f ? kAccent : juce::Colour (0xff59e07a)));
        g.fillRoundedRectangle (fill, 2.0f);
    }
}
