#pragma once

#include "Widgets.h"
#include "TabPages.h"

namespace vsx
{
/** X/Y pad for vector synthesis: drags Vector X / Vector Y (host gestures via ParameterAttachment). */
class VectorPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VectorPad (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override; // back to the centre
private:
    void setFrom (juce::Point<float> pos);
    juce::Rectangle<float> padArea() const { return getLocalBounds().toFloat().reduced (24.0f, 18.0f); }
    ViceSynthProcessor& proc;
    juce::ParameterAttachment xAttach, yAttach;
    float x = 0.0f, y = 0.0f;
};

/** Shows the wave-sequence step positions (Start .. End) and the play order of the pattern. */
class WaveSeqDisplay : public juce::Component
{
public:
    explicit WaveSeqDisplay (ViceSynthProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ViceSynthProcessor& proc;
};

class VectorPage : public juce::Component, private juce::Timer
{
public:
    explicit VectorPage (ViceSynthProcessor& p);
    ~VectorPage() override { stopTimer(); }
    void resized() override;
private:
    void timerCallback() override { seqDisplay.repaint(); }
    Section vectorBox { "VECTOR", Theme::neonPink }, seqBox { "WAVE SEQUENCE  (OSC A position)", Theme::cyberCyan };
    VectorPad pad;
    WaveSeqDisplay seqDisplay;
    std::vector<std::unique_ptr<juce::Component>> vecControls, seqToggles, seqKnobs;
};
} // namespace vsx
