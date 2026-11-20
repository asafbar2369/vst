#pragma once

#include "EnvLfoEditors.h"
#include "WaveDisplay.h"

namespace vsx
{
/** Builds the right control (knob / combo / toggle) for a registry parameter. */
std::unique_ptr<juce::Component> makeControl (ViceSynthProcessor& p, int paramIndex, juce::Colour c);

class FxPage : public juce::Component
{
public:
    explicit FxPage (ViceSynthProcessor& p);
    ~FxPage() override;
    void resized() override;
    void rebuildOrder();
private:
    class Row;
    ViceSynthProcessor& proc;
    ParamCombo routing;
    std::unique_ptr<ParamKnob> balance;
    juce::Label hint;
    juce::Viewport viewport;
    juce::Component rack;
    juce::OwnedArray<Row> rows;
};

class MatrixPage : public juce::Component, private juce::Timer
{
public:
    explicit MatrixPage (ViceSynthProcessor& p);
    ~MatrixPage() override;
    void resized() override;
private:
    class Row;
    void timerCallback() override;
    ViceSynthProcessor& proc;
    juce::Viewport viewport;
    juce::Component table;
    juce::OwnedArray<Row> rows;
    juce::Label header;
};

class LfoPage : public juce::Component
{
public:
    explicit LfoPage (ViceSynthProcessor& p);
    void resized() override;
    void refresh();
private:
    struct Panel { std::unique_ptr<Section> box; std::unique_ptr<LfoEditor> editor; std::vector<std::unique_ptr<juce::Component>> controls; };
    std::vector<Panel> panels;
};

class EnvPage : public juce::Component
{
public:
    explicit EnvPage (ViceSynthProcessor& p);
    void resized() override;
    void refresh();
private:
    struct Panel { std::unique_ptr<Section> box; std::unique_ptr<EnvelopeEditor> editor; std::vector<std::unique_ptr<juce::Component>> controls; };
    std::vector<Panel> panels;
};

class ArpPage : public juce::Component
{
public:
    explicit ArpPage (ViceSynthProcessor& p);
    ~ArpPage() override;
    void resized() override;
    void refresh();
private:
    class Grid;
    ViceSynthProcessor& proc;
    Section box { "ARPEGGIATOR", Theme::cyberCyan };
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<Grid> grid;
    juce::TextButton presetsB { "PATTERNS" }, randomB { "RANDOMIZE" };
    juce::Slider length;
    juce::Label lengthLabel;
};

class SeqPage : public juce::Component
{
public:
    explicit SeqPage (ViceSynthProcessor& p);
    ~SeqPage() override;
    void resized() override;
    void refresh();
private:
    class Grid;
    ViceSynthProcessor& proc;
    Section box { "POLYPHONIC STEP SEQUENCER", Theme::neonPink };
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<Grid> grid;
    juce::TextButton presetsB { "PATTERNS" }, randomB { "RANDOMIZE" }, clearB { "CLEAR" };
    juce::ComboBox lane;
};

class MixerPage : public juce::Component
{
public:
    explicit MixerPage (ViceSynthProcessor& p);
    void resized() override;
    void refresh() { scope.repaint(); meter.repaint(); }
private:
    struct Strip { std::unique_ptr<Section> box; std::vector<std::unique_ptr<juce::Component>> controls; };
    std::vector<Strip> strips;
    Oscilloscope scope;
    LevelMeter meter;
    std::unique_ptr<ParamKnob> master;
};

class SettingsPage : public juce::Component, private juce::Timer
{
public:
    explicit SettingsPage (ViceSynthProcessor& p);
    ~SettingsPage() override;
    void resized() override;
    std::function<void (float)> onScale;
    std::function<void (bool)> onOpenGL;
    bool openGLEnabled = false;
private:
    void timerCallback() override;
    ViceSynthProcessor& proc;
    Section engineBox { "ENGINE", Theme::sunsetOrange }, aiBox { "AI SOUND DESIGNER", Theme::cyberCyan }, uiBox { "INTERFACE & LIBRARY", Theme::neonPink };
    std::vector<std::unique_ptr<juce::Component>> engineControls;
    std::unique_ptr<juce::Component> aiSettings;
    juce::ToggleButton openGL { "OpenGL hardware acceleration" };
    juce::Label about, sizeLabel;
    juce::OwnedArray<juce::TextButton> scaleButtons;
    juce::TextButton openPresets { "OPEN PRESET FOLDER" }, openWavetables { "OPEN WAVETABLE FOLDER" }, openSamples { "OPEN SAMPLE FOLDER" },
        restore { "RESTORE LAST SESSION" };
};

} // namespace vsx
