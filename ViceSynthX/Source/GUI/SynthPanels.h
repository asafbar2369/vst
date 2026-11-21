#pragma once

#include "EnvLfoEditors.h"
#include "OscSourceMenu.h"
#include "WaveDisplay.h"

namespace vsx
{
class OscPanel : public Section
{
public:
    OscPanel (ViceSynthProcessor& p, int oscIndex);
    void resized() override;
    void refresh();                 // periodic (displays, mode-dependent rows)
    void resourceChanged();         // wavetable / sample loaded
    std::function<void (int)> onOpenWavetableEditor;

private:
    void showResourceMenu();
    void chooseSample();
    void updateModeRows();
    ViceSynthProcessor& proc;
    int osc;
    ParamToggle onButton;
    ParamCombo mode, warpMode, xmodMode, xmodSource, route, loopMode, grainWindow, source;
    juce::TextButton resourceButton, loadSampleButton { "LOAD SAMPLE" };
    OscDisplay display;
    OscSourceArrow sourceArrow { proc, osc };
    std::vector<std::unique_ptr<ParamKnob>> row1, row2, rowAnalog, rowSample, rowGrain, rowSpectral;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastMode = -1;
};

class SubNoisePanel : public Section
{
public:
    explicit SubNoisePanel (ViceSynthProcessor& p);
    void resized() override;
private:
    ParamToggle subOn, noiseOn;
    ParamCombo subShape, subRoute, noiseType, noiseRoute;
    std::vector<std::unique_ptr<ParamKnob>> subKnobs, noiseKnobs;
};

class FilterPanel : public Section
{
public:
    FilterPanel (ViceSynthProcessor& p, int filterIndex);
    void resized() override;
    void refresh() { display.refresh(); }
private:
    ViceSynthProcessor& proc;
    int index;
    ParamToggle onButton;
    ParamCombo model, mode;
    std::unique_ptr<ParamCombo> routing;
    FilterDisplay display;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
};

class ModulationSection : public juce::Component
{
public:
    explicit ModulationSection (ViceSynthProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
private:
    void selectEnv (int e);
    void selectLfo (int l);
    ViceSynthProcessor& proc;
    Section envBox { "ENVELOPES", Theme::neonPink }, lfoBox { "LFOS", Theme::cyberCyan }, macroBox { "MACROS", Theme::sunsetOrange };
    EnvelopeEditor envEditor;
    LfoEditor lfoEditor;
    juce::OwnedArray<juce::TextButton> envTabs, lfoTabs;
    std::vector<std::vector<std::unique_ptr<ParamKnob>>> envKnobs;
    struct LfoControls { std::unique_ptr<ParamCombo> shape, div, mode; std::unique_ptr<ParamToggle> sync; std::vector<std::unique_ptr<ParamKnob>> knobs; };
    std::vector<LfoControls> lfoControls;
    std::vector<std::unique_ptr<ParamKnob>> macroKnobs;
    juce::OwnedArray<ModSourceChip> chips;
    int currentEnv = 0, currentLfo = 0;
};

/** Pitch / mod wheels that send MIDI into the engine. */
class Wheel : public juce::Slider
{
public:
    Wheel (ViceSynthProcessor& p, bool pitch);
    void mouseUp (const juce::MouseEvent&) override;
private:
    ViceSynthProcessor& proc;
    bool isPitch;
};

class BottomBar : public Section
{
public:
    explicit BottomBar (ViceSynthProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    ViceSynthProcessor& proc;
    Wheel pitchWheel, modWheel;
    juce::MidiKeyboardComponent keyboard;
    ParamCombo voiceMode, glideMode;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
};

} // namespace vsx
