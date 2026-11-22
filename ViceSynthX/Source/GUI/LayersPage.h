#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace vsx
{
/**
    LAYERS page: layer A is the patch being edited; layers B, C and D are complete sounds that play with it.
    Each row: enable, sound (LOAD from the library, EDIT swaps it with the editor, CLEAR), key range, velocity
    range, transpose, level and pan. Layer settings are session state (saved with the project, not in presets).
*/
class LayerRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    LayerRow (ViceSynthProcessor& p, int index); // index -1 = layer A
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    void showLoadMenu();
    LayerRouting& routing();
    ViceSynthProcessor& proc;
    const int index;
    juce::ToggleButton enable { "ON" };
    juce::Label name;
    juce::TextButton load { "LOAD" }, edit { "EDIT" }, clear { "CLEAR" };
    juce::Slider keyLo, keyHi, velLo, velHi, transpose, level, pan;
    juce::OwnedArray<juce::Label> captions;
};

class LayersPage : public juce::Component, private juce::Timer
{
public:
    explicit LayersPage (ViceSynthProcessor& p);
    ~LayersPage() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshAll() { for (auto* r : rows) { r->refresh(); r->repaint(); } }
    void visibilityChanged() override { if (isVisible()) refreshAll(); }
private:
    void timerCallback() override { refreshAll(); }
    Section box { "LAYERS  -  splits, velocity layers and stacks", Theme::neonPink };
    juce::OwnedArray<LayerRow> rows;
    juce::Label help;
};
} // namespace vsx
