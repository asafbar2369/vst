#pragma once

#include "Icons.h"
#include "MiamiArtwork.h"
#include "Widgets.h"

namespace vsx
{
/** Persistent strip of the eight engine macros (Mood ... Mix) with a small Miami vignette. */
class MacroStrip : public juce::Component
{
public:
    explicit MacroStrip (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
private:
    std::vector<std::unique_ptr<ParamKnob>> knobs;
    MiamiHeader vignette { 0.62f, 0.7f, 0.0f };
};

/** Glossy keyboard with pink/cyan reflections (C2 - B5). */
class VsxKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit VsxKeyboard (juce::MidiKeyboardState& s);
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver, juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver, juce::Colour) override;
    void resized() override;
};

class PerformancePanel : public juce::Component, private juce::Timer
{
public:
    explicit PerformancePanel (ViceSynthProcessor& p);
    ~PerformancePanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    class WheelControl;
    void timerCallback() override;
    ViceSynthProcessor& proc;
    std::unique_ptr<WheelControl> pitch, mod;
    juce::ComboBox bend, poly, unison;
    std::unique_ptr<ParamKnob> glide;
    juce::TextButton hold { "HOLD" };
    VsxKeyboard keyboard;
    bool syncing = false;
};

} // namespace vsx
