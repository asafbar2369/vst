#pragma once

#include "Widgets.h"

namespace vsx
{
/** Mode-aware oscillator display: drag = position, double-click = wavetable editor, drop audio files to load. */
class OscDisplay : public juce::Component, public juce::FileDragAndDropTarget, public juce::SettableTooltipClient
{
public:
    OscDisplay (ViceSynthProcessor& p, int oscIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void refresh();
    std::function<void()> onOpenEditor;

private:
    void drawWavetable (juce::Graphics&, juce::Rectangle<float>, float pos);
    void drawAnalog (juce::Graphics&, juce::Rectangle<float>, float pos);
    void drawSample (juce::Graphics&, juce::Rectangle<float>, const SampleData*, float pos);
    void drawSpectral (juce::Graphics&, juce::Rectangle<float>, float pos);
    void drawPhaseDist (juce::Graphics&, juce::Rectangle<float>);
    void drawBars (juce::Graphics&, juce::Rectangle<float>, const float* amps, int count, const juce::String& label);
    void drawModal (juce::Graphics&, juce::Rectangle<float>, float pos);
    ViceSynthProcessor& proc;
    int osc;
    float dragStartPos = 0.0f;
    const SampleData* cachedSample = nullptr;
    std::vector<std::pair<float, float>> overview;
    float lastPos = -1.0f;
    int lastMode = -1;
    const void* lastResource = nullptr;
};

/** Measured small-signal response of a filter section (updates when its settings change). */
class FilterDisplay : public juce::Component
{
public:
    FilterDisplay (ViceSynthProcessor& p, int filterIndex);
    void paint (juce::Graphics&) override;
    void refresh();
private:
    ViceSynthProcessor& proc;
    int index;
    std::array<float, 160> mags {};
    juce::String key;
};

class Oscilloscope : public juce::Component
{
public:
    explicit Oscilloscope (ViceSynthProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ViceSynthProcessor& proc;
};

} // namespace vsx
