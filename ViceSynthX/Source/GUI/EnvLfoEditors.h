#pragma once

#include "Widgets.h"

namespace vsx
{
class EnvelopeEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnvelopeEditor (ViceSynthProcessor& p, int envIndex);
    void setEnvelope (int e) { env = e; repaint(); }
    int getEnvelope() const { return env; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Handles { juce::Point<float> a, d, s, r; float totalTime; };
    Handles layout() const;
    float timeToX (float t, float total) const;
    float xToTime (float x, float total) const;
    ViceSynthProcessor& proc;
    int env;
    int dragging = -1;
    float dragTotal = 1.0f;
};

class LfoEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoEditor (ViceSynthProcessor& p, int lfoIndex);
    void setLfo (int l) { lfo = l; repaint(); }
    int getLfo() const { return lfo; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const;
    ViceSynthProcessor& proc;
    int lfo;
    int dragPoint = -1;
    float dragStartTension = 0.0f;
};

} // namespace vsx
