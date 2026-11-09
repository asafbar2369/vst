#pragma once
#include "PluginProcessor.h"
#include "HardwarePanels.h"

// Panel look: original vector drawing only (no photographs, logos or Roland artwork).
class ComboLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ComboLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (12.0f); }
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
    void setLabels (const juce::StringArray& texts); // replaces the automatic short names, in order

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

// Address-level editor for a whole synth area (D-50 temporary area, JD-800 patch), labelled with
// the parameter names and ranges of the original MIDI implementation. Not automatable itself;
// host-bound parameters follow automatically.
struct AreaEditorSpec
{
    juce::StringArray blockNames;
    std::vector<std::pair<int, int>> blocks;           // start offset, size
    std::function<juce::String (int)> paramName;
    std::function<int (int)> minFor, maxFor;
    std::function<bool (int)> hidden;                   // name bytes, dummies, extensions
    std::function<juce::String (int, int)> format;
    std::function<int (int)> get;
    std::function<void (int, int)> set;
    std::function<int (int)> nameLength;                // per block; 0 = no name field
    std::function<juce::String (int)> getName;
    std::function<void (int, const juce::String&)> setName;
    juce::String allowedNameChars;
    juce::Colour colour;
};

class AreaEditor : public juce::Component, private juce::Timer
{
public:
    explicit AreaEditor (AreaEditorSpec);
    ~AreaEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void selectBlock (int index);
    int getNumBlocks() const noexcept { return (int) spec.blocks.size(); }
    juce::Slider* sliderForOffset (int offset) noexcept;
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
    AreaEditorSpec spec;
    juce::ComboBox blockBox;
    juce::Label nameLabel { {}, "Name" };
    juce::TextEditor nameEditor;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<Row> rows;
    int block = 0;
    bool refreshing = false;
};

// Keyboard strip showing which keys reach each engine (mode, split point, zones, transpose).
class ZoneView : public juce::Component
{
public:
    explicit ZoneView (DigitalComboProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    DigitalComboProcessor& proc;
};

// "COMBO" page: engine mixer and zones, patch files, the user's own wave folders, editor drawing.
class ComboPage : public juce::Component
{
public:
    explicit ComboPage (DigitalComboProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();                         // editor timer
    ParamPanel& getMixer() noexcept { return *mixer; }
    std::function<void()> onPatchesChanged; // after a save, so the browser re-reads the folder

    juce::TextButton savePatch { "Save patch..." }, loadPatch { "Load patch..." }, openPatchFolder { "Show folder" };
    juce::TextButton chooseWaves[2], clearWaves[2];
    juce::ComboBox rendererBox;

private:
    void chooseWaveFolder (bool jd);
    DigitalComboProcessor& proc;
    std::unique_ptr<ParamPanel> mixer;
    ZoneView zones;
    juce::Label patchFolderLabel, waveLabels[2], waveCounts[2], rendererLabel { {}, "Editor drawing" };
    juce::TextEditor waveInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
    int shownCounts[2] { -1, -1 };
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
    AreaEditor& getFullEditor() noexcept { return *fullEditor; }
    AreaEditor& getJdFullEditor() noexcept { return *jdFullEditor; }
    int getNumTabs() const noexcept { return tabs.getNumTabs(); }
    juce::String getTabName (int i) const { return tabs.getTabNames()[i]; }
    void showTab (int i) { tabs.setCurrentTabIndex (i); }
    ComboPage& getComboPage() noexcept { return *comboPage; }
    D50Panel& getD50Panel() noexcept { return *d50Panel; }
    JdPanel& getJdPanel() noexcept { return *jdPanel; }
    int findTab (const juce::String& name) const { return tabs.getTabNames().indexOf (name); }
    juce::ComboBox& getPatchBrowser() noexcept { return presetBox; }
    void refreshPatchBrowser();
    static constexpr int kUserPatchIdBase = 1000;

private:
    void timerCallback() override;
    void showJdTone (int t);
    static juce::String shortName (const juce::String& full);
    friend class ParamPanel;

    DigitalComboProcessor& proc;
    ComboLookAndFeel lnf;

    std::unique_ptr<ParamPanel> globalPanel, d50P1, d50P2, d50Common, d50Patch, jdTones[4], jdCommon;
    std::unique_ptr<AreaEditor> fullEditor, jdFullEditor;
    std::unique_ptr<ComboPage> comboPage;
    std::unique_ptr<D50Panel> d50Panel;
    std::unique_ptr<JdPanel> jdPanel;
    juce::Array<juce::File> listedPatches;
    int timerTicks = 0;
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
