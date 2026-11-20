#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"

namespace vsx
{
/** Rotary knob bound to a registry parameter, with modulation rings and drag & drop targets. */
class ParamKnob : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    enum class Style { Compact, ValueBox, Macro };
    ParamKnob (ViceSynthProcessor& p, int paramIndex, const juce::String& label = {}, juce::Colour accent = Theme::neonPink,
               Style style = Style::Compact);
    ~ParamKnob() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void refreshModulation();
    void setTooltipExtra (const juce::String& extra);
    /** Marks a parameter that the last AI result changed (cleared when the value moves away). */
    void setAIHighlight (bool on) { if (on != aiHighlight) { aiHighlight = on; repaint(); } }
    juce::Slider& getSlider();
    int getParamIndex() const { return index; }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    static juce::Array<ParamKnob*>& registry();

private:
    class KnobSlider;
    void showMenu();
    ViceSynthProcessor& proc;
    int index;
    std::unique_ptr<KnobSlider> slider;
    juce::Label label, valueBox;
    Style style = Style::Compact;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool dragHover = false, aiHighlight = false;
    int activeSlot = -1;
    float lastDepth = 0.0f, lastLive = -2.0f;
};

class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (ViceSynthProcessor& p, int paramIndex, const juce::String& text = {}, juce::Colour c = Theme::cyberCyan);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (ViceSynthProcessor& p, int paramIndex);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

/** Draggable modulation-source chip (drop onto any knob to create a matrix routing). */
class ModSourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModSourceChip (int source);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override {}
    std::function<float()> liveValue;
private:
    int source;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (ViceSynthProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    ViceSynthProcessor& proc;
};

/** Titled glass section. */
class Section : public juce::Component
{
public:
    explicit Section (juce::String t, juce::Colour a = Theme::electricPurple) : title (std::move (t)), accent (a) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics& g) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (8).withTrimmedTop (18); }
    juce::String title;
    juce::Colour accent;
};

/** Simple grid helper for laying out knobs. */
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps, int gap = 4);

} // namespace vsx
