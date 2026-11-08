#pragma once
#include "PluginProcessor.h"

// Panel look: original vector drawing only (no photographs, logos or Roland artwork).
class ComboLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ComboLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Colour accent { 0xffe0a040 };
};

// A grid of controls created from parameter IDs: bool -> toggle, choice -> combo box, others -> knob.
class ParamPanel : public juce::Component
{
public:
    ParamPanel (juce::AudioProcessorValueTreeState& state, const juce::StringArray& ids, int columns, juce::String title = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    const juce::StringArray& getIds() const noexcept { return ids; }
    int getNumControls() const noexcept { return (int) cells.size(); }

private:
    struct Cell
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
    };
    std::vector<std::unique_ptr<Cell>> cells;
    juce::StringArray ids;
    int columns;
    juce::String title;
};

class DigitalComboEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DigitalComboEditor (DigitalComboProcessor&);
    ~DigitalComboEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Every parameter ID that has an on-screen control (used by the automated tests).
    juce::StringArray getAttachedParameterIds() const;

private:
    void timerCallback() override;
    void showJdTone (int t);
    static juce::String shortName (const juce::String& full);
    friend class ParamPanel;

    DigitalComboProcessor& proc;
    ComboLookAndFeel lnf;

    std::unique_ptr<ParamPanel> globalPanel, d50P1, d50P2, d50Common, d50Patch, jdTones[4], jdCommon;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    struct Page : juce::Component
    {
        std::function<void (juce::Rectangle<int>)> layout;
        void resized() override { if (layout) layout (getLocalBounds().reduced (4)); }
    };
    Page d50PartialPage, d50CommonPage, jdTonePage, jdFxPage;
    juce::TextButton toneButtons[4];
    juce::ComboBox presetBox;
    juce::TextButton loadSyx { "Load .syx" }, saveSyx { "Save .syx" };
    juce::Label display;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFileMessage;
    int shownProgram = -1;
    int currentJdTone = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalComboEditor)
};
