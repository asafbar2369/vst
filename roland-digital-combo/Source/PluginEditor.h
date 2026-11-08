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

// Address-level editor for the whole D-50 temporary area (7 blocks x 64 bytes), labelled with the
// parameter names and ranges of the D-50 MIDI Implementation. Not automatable; host-bound
// parameters follow automatically.
class D50FullEditor : public juce::Component, private juce::Timer
{
public:
    explicit D50FullEditor (DigitalComboProcessor&);
    ~D50FullEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void selectBlock (int index);                 // 0..6, order of the address map
    juce::Slider* sliderForOffset (int tempOffset) noexcept;
    juce::TextEditor& getNameEditor() noexcept { return nameEditor; }
    int getNumRows() const noexcept { return (int) rows.size(); }

private:
    void timerCallback() override;
    void commitName();
    struct Row
    {
        int offset = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
    };
    DigitalComboProcessor& proc;
    juce::ComboBox blockBox;
    juce::Label nameLabel { {}, "Name" };
    juce::TextEditor nameEditor;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<Row> rows;
    int block = 0, nameOffset = -1, nameLength = 0;
    bool refreshing = false;
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
    D50FullEditor& getFullEditor() noexcept { return *fullEditor; }
    int getNumTabs() const noexcept { return tabs.getNumTabs(); }
    juce::String getTabName (int i) const { return tabs.getTabNames()[i]; }
    void showTab (int i) { tabs.setCurrentTabIndex (i); }

private:
    void timerCallback() override;
    void showJdTone (int t);
    static juce::String shortName (const juce::String& full);
    friend class ParamPanel;

    DigitalComboProcessor& proc;
    ComboLookAndFeel lnf;

    std::unique_ptr<ParamPanel> globalPanel, d50P1, d50P2, d50Common, d50Patch, jdTones[4], jdCommon;
    std::unique_ptr<D50FullEditor> fullEditor;
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
