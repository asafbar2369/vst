#include "VectorPage.h"

namespace vsx
{
using namespace pid;

// =================================================================================
VectorPad::VectorPad (ViceSynthProcessor& p)
    : proc (p),
      xAttach (*p.getParameterObject (G (global::VecX)), [this] (float v) { x = v; repaint(); }, nullptr),
      yAttach (*p.getParameterObject (G (global::VecY)), [this] (float v) { y = v; repaint(); }, nullptr)
{
    setComponentID ("vsx_vector_pad");
    setTooltip ("Drag to mix OSC A (top left), OSC B (top right), OSC C (bottom left) and D (bottom right) with equal power. "
                "Double-click: centre. Modulate Vector X / Y in the matrix for vector motion.");
    xAttach.sendInitialUpdate();
    yAttach.sendInitialUpdate();
}

void VectorPad::setFrom (juce::Point<float> pos)
{
    const auto a = padArea();
    const float nx = juce::jlimit (-1.0f, 1.0f, (pos.x - a.getCentreX()) / (a.getWidth() * 0.5f));
    const float ny = juce::jlimit (-1.0f, 1.0f, (a.getCentreY() - pos.y) / (a.getHeight() * 0.5f));
    xAttach.setValueAsPartOfGesture (nx);
    yAttach.setValueAsPartOfGesture (ny);
}

void VectorPad::mouseDown (const juce::MouseEvent& e) { xAttach.beginGesture(); yAttach.beginGesture(); setFrom (e.position); }
void VectorPad::mouseDrag (const juce::MouseEvent& e) { setFrom (e.position); }
void VectorPad::mouseUp (const juce::MouseEvent&) { xAttach.endGesture(); yAttach.endGesture(); }
void VectorPad::mouseDoubleClick (const juce::MouseEvent&) { xAttach.setValueAsCompleteGesture (0.0f); yAttach.setValueAsCompleteGesture (0.0f); }

void VectorPad::paint (juce::Graphics& g)
{
    const auto a = padArea();
    const bool on = proc.getParam (G (global::VecOn)) > 0.5f;
    g.setColour (Theme::panel.darker (0.3f));
    g.fillRoundedRectangle (a, 8.0f);
    g.setColour (Theme::textDim.withAlpha (0.25f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * i / 4.0f), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * i / 4.0f), a.getX(), a.getRight());
    }
    g.setColour ((on ? Theme::neonPink : Theme::textDim).withAlpha (0.8f));
    g.drawRoundedRectangle (a, 8.0f, 1.5f);

    g.setFont (Theme::font (12.0f, true));
    const juce::String d = proc.getParam (G (global::VecD)) > 0.5f ? "D  NOISE" : "D  SUB";
    g.setColour (Theme::accentFor (0)); g.drawText ("A  OSC A", a.reduced (8).removeFromTop (16), juce::Justification::topLeft);
    g.setColour (Theme::accentFor (1)); g.drawText ("B  OSC B", a.reduced (8).removeFromTop (16), juce::Justification::topRight);
    g.setColour (Theme::accentFor (2)); g.drawText ("C  OSC C", a.reduced (8).removeFromBottom (16), juce::Justification::bottomLeft);
    g.setColour (Theme::electricPurple); g.drawText (d, a.reduced (8).removeFromBottom (16), juce::Justification::bottomRight);

    // equal-power weights at the current position (as the engine computes them)
    const float u = 0.5f * (x + 1.0f), v = 0.5f * (y + 1.0f);
    const float w[4] = { std::sqrt ((1 - u) * v), std::sqrt (u * v), std::sqrt ((1 - u) * (1 - v)), std::sqrt (u * (1 - v)) };
    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::textDim);
    g.drawText ("A " + juce::String (juce::roundToInt (w[0] * 100)) + "%   B " + juce::String (juce::roundToInt (w[1] * 100))
                    + "%   C " + juce::String (juce::roundToInt (w[2] * 100)) + "%   D " + juce::String (juce::roundToInt (w[3] * 100)) + "%",
                getLocalBounds().toFloat().removeFromBottom (16.0f), juce::Justification::centred);

    const juce::Point<float> dot (a.getCentreX() + x * a.getWidth() * 0.5f, a.getCentreY() - y * a.getHeight() * 0.5f);
    const auto col = on ? Theme::cyberCyan : Theme::textDim;
    g.setColour (col.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (dot));
    g.setColour (col);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (dot));
    if (! on)
    {
        g.setColour (Theme::textDim);
        g.drawText ("Vector off", a, juce::Justification::centred);
    }
}

// =================================================================================
void WaveSeqDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (Theme::panel.darker (0.3f));
    g.fillRoundedRectangle (r, 6.0f);
    const bool on = proc.getParam (G (global::WseqOn)) > 0.5f;
    const int steps = juce::jlimit (2, 16, (int) proc.getParam (G (global::WseqSteps)));
    const float a = proc.getParam (G (global::WseqStart)), b = proc.getParam (G (global::WseqEnd));
    const int pattern = (int) proc.getParam (G (global::WseqPattern));
    auto area = r.reduced (10.0f, 18.0f);
    const float bw = area.getWidth() / (float) steps;
    for (int k = 0; k < steps; ++k)
    {
        const float pos = a + (b - a) * (float) k / (float) (steps - 1);
        const float h = std::max (3.0f, pos * area.getHeight());
        g.setColour ((on ? Theme::cyberCyan : Theme::textDim).withAlpha (0.35f + 0.5f * pos));
        g.fillRoundedRectangle (area.getX() + k * bw + 2.0f, area.getBottom() - h, bw - 4.0f, h, 2.0f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.0f));
        g.drawText (juce::String (k + 1), juce::Rectangle<float> (area.getX() + k * bw, area.getBottom() + 1.0f, bw, 14.0f), juce::Justification::centred);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f));
    const juce::String order = pattern == 1 ? "Down" : pattern == 2 ? "Ping-Pong" : pattern == 3 ? "Random" : "Up";
    g.drawText (juce::String (on ? "" : "Off  -  ") + juce::String (steps) + " steps, " + order + ", position "
                    + juce::String (juce::roundToInt (a * 100)) + " % to " + juce::String (juce::roundToInt (b * 100)) + " %",
                r.removeFromTop (16.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);
}

// =================================================================================
VectorPage::VectorPage (ViceSynthProcessor& p) : pad (p), seqDisplay (p)
{
    setComponentID ("vsx_vector_page");
    vectorBox.setComponentID ("vsx_vector_box");
    seqBox.setComponentID ("vsx_wave_seq_box");
    for (auto* c : std::initializer_list<juce::Component*> { &vectorBox, &seqBox, &pad, &seqDisplay }) addAndMakeVisible (c);
    for (int id : { G (global::VecOn), G (global::VecD), G (global::VecX), G (global::VecY) })
    {
        vecControls.push_back (makeControl (p, id, Theme::neonPink));
        addAndMakeVisible (*vecControls.back());
    }
    for (int id : { G (global::WseqOn), G (global::WseqPattern), G (global::WseqSync), G (global::WseqDiv) })
    {
        seqToggles.push_back (makeControl (p, id, Theme::cyberCyan));
        addAndMakeVisible (*seqToggles.back());
    }
    for (int id : { G (global::WseqSteps), G (global::WseqRate), G (global::WseqXfade), G (global::WseqStart), G (global::WseqEnd) })
    {
        seqKnobs.push_back (makeControl (p, id, Theme::cyberCyan));
        addAndMakeVisible (*seqKnobs.back());
    }
    startTimerHz (10);
}

void VectorPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto left = r.removeFromLeft (std::min (480, r.getWidth() / 2));
    vectorBox.setBounds (left);
    auto vc = vectorBox.getBounds().reduced (10).withTrimmedTop (22);
    auto row = vc.removeFromTop (26);
    vecControls[0]->setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    vecControls[1]->setBounds (row.removeFromLeft (120));
    auto knobs = vc.removeFromBottom (std::min (100, vc.getHeight() / 3));
    vecControls[2]->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (6, 0));
    vecControls[3]->setBounds (knobs.reduced (6, 0));
    pad.setBounds (vc.reduced (0, 6));

    r.removeFromLeft (8);
    seqBox.setBounds (r);
    auto sc = seqBox.getBounds().reduced (10).withTrimmedTop (22);
    auto top = sc.removeFromTop (26);
    const int tw = top.getWidth() / 4;
    for (auto& c : seqToggles) c->setBounds (top.removeFromLeft (tw).reduced (3, 0));
    sc.removeFromTop (8);
    auto kr = sc.removeFromTop (std::min (110, sc.getHeight() / 2));
    const int kw = kr.getWidth() / (int) seqKnobs.size();
    for (auto& k : seqKnobs) k->setBounds (kr.removeFromLeft (kw).reduced (4, 0));
    sc.removeFromTop (8);
    seqDisplay.setBounds (sc);
}
} // namespace vsx
