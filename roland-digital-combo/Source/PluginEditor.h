#pragma once
#include "PluginProcessor.h"
#include "HardwarePanels.h"
#include "PerformanceControls.h"
#include "CloudUI.h"
#include "CloudPages.h"

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
    juce::TextButton chooseRomFolder { "ROM folder..." }, chooseRomZip { "ROM .zip..." }, clearRom { "Use built-in" };
    juce::ComboBox rendererBox;

private:
    void chooseWaveFolder (bool jd);
    void chooseRom (bool zip);
    DigitalComboProcessor& proc;
    std::unique_ptr<ParamPanel> mixer;
    ZoneView zones;
    juce::Label patchFolderLabel, waveLabels[2], waveCounts[2], romLabel, romCount, rendererLabel { {}, "Editor drawing" };
    juce::TextEditor waveInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
    int shownCounts[2] { -1, -1 }, shownRom = -1;
};

class DigitalComboEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DigitalComboEditor (DigitalComboProcessor&);
    ~DigitalComboEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // ---- Design size and scaling: everything is laid out at kBaseWidth x kBaseHeight and scaled ----
    static constexpr int kBaseWidth = 1280, kBaseHeight = 800;
    static constexpr int kScalePercents[6] = { 75, 100, 125, 150, 175, 200 };
    void setUiScale (float scale);          // resizes the window (the host is asked through the normal resize path)
    float getUiScale() const noexcept { return uiScale; }
    juce::ComboBox& getSizeBox() noexcept { return sizeBox; }

    // ---- Pages ----
    enum Page { PageSynthFirst = 0, PageMixer = 5, PageSettings = 6, PageAdvanced = 7, kNumPages = 8 };
    int getNumPages() const noexcept { return kNumPages; }
    juce::String getPageName (int page) const;
    void showPage (int page);
    int getCurrentPage() const noexcept { return currentPage; }
    SynthPage* getSynthPage (bool jd, int pageId) noexcept { return (jd ? jdPages : d50Pages)[pageId].get(); }
    bool getEditTargetIsJd() const noexcept { return editJd; }
    void setEditTarget (bool jd);
    juce::TextButton& getModeButton (int i) noexcept { return modeButtons[i]; }
    juce::TextButton& getPageButton (int i) noexcept { return pageButtons[i]; }
    juce::TextButton& getEngineButton (bool jd) noexcept { return engineButtons[jd ? 0 : 1]; }
    juce::TextButton& getInitButton() noexcept { return initButton; }
    juce::TextButton& getUndoButton() noexcept { return undoButton; }
    juce::TextButton& getRedoButton() noexcept { return redoButton; }
    juce::TextButton& getPrevButton() noexcept { return prevButton; }
    juce::TextButton& getNextButton() noexcept { return nextButton; }
    juce::Slider& getBalanceSlider() noexcept { return balanceSlider; }
    juce::Slider& getSplitSlider() noexcept { return splitSlider; }
    juce::ToggleButton& getSwapToggle() noexcept { return swapToggle; }
    juce::Slider& getVolumeSlider() noexcept { return volumeSlider; }
    void stepPatch (int delta);
    void refreshChrome() { timerCallback(); }   // what the 10 Hz timer does; the tests call it directly

    // Every parameter ID that has an on-screen control on the Advanced / Settings pages (used by the tests).
    juce::StringArray getAttachedParameterIds() const;
    AreaEditor& getFullEditor() noexcept { return *fullEditor; }
    AreaEditor& getJdFullEditor() noexcept { return *jdFullEditor; }
    // The classic tabbed pages live on the Advanced page
    int getNumTabs() const noexcept { return tabs.getNumTabs(); }
    juce::String getTabName (int i) const { return tabs.getTabNames()[i]; }
    void showTab (int i) { showPage (PageAdvanced); tabs.setCurrentTabIndex (i); }
    ComboPage& getComboPage() noexcept { return *comboPage; }
    D50Panel& getD50Panel() noexcept { return *d50Panel; }
    JdPanel& getJdPanel() noexcept { return *jdPanel; }
    WheelControl& getPitchWheel() noexcept { return *pitchWheel; }
    WheelControl& getModWheel() noexcept { return *modWheel; }
    MidiActivityLed& getMidiLed() noexcept { return *midiLed; }
    LevelMeter& getLevelMeter() noexcept { return *levelMeter; }
    int findTab (const juce::String& name) const { return tabs.getTabNames().indexOf (name); }
    juce::ComboBox& getPatchBrowser() noexcept { return presetBox; }
    void refreshPatchBrowser();
    static constexpr int kUserPatchIdBase = 1000, kD50BankIdBase = 3000, kJdBankIdBase = 4000;

private:
    void timerCallback() override;
    void showJdTone (int t);
    void updatePageVisibility();
    void layoutContent();
    void paintContent (juce::Graphics&);
    void updateSizeBox();
    static juce::String shortName (const juce::String& full);
    friend class ParamPanel;

    DigitalComboProcessor& proc;
    CloudLookAndFeel lnf;

    // Everything is a child of `content`, which is scaled as a whole.
    struct Content : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    } content;

    // header, patch browser, mode strip
    juce::TextButton modeButtons[4];
    juce::TextButton prevButton { "<" }, nextButton { ">" }, initButton { "INIT" }, undoButton { "UNDO" }, redoButton { "REDO" };
    juce::ComboBox presetBox, sizeBox;
    juce::TextButton loadSyx { "Import .syx" }, saveSyx { "Export .syx" }, savePatchButton { "Save patch" }, loadPatchButton { "Load patch" };
    juce::Slider balanceSlider, splitSlider, volumeSlider;
    juce::ToggleButton swapToggle { "JD-800 LOWER" };
    juce::Label stripLabel, volumeLabel, display;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> balanceAtt, splitAtt, volumeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> swapAtt;

    // navigation
    juce::TextButton engineButtons[2];        // [0] JD-800, [1] D-50
    juce::TextButton pageButtons[kNumPages];
    bool editJd = true;
    int currentPage = PageSound;

    // pages
    juce::Component pageHost;
    std::unique_ptr<SynthPage> jdPages[kNumSynthPages], d50Pages[kNumSynthPages];
    std::unique_ptr<ParamPanel> globalPanel, d50P1, d50P2, d50Common, d50Patch, jdTones[4], jdCommon;
    std::unique_ptr<AreaEditor> fullEditor, jdFullEditor;
    std::unique_ptr<ComboPage> comboPage;
    std::unique_ptr<D50Panel> d50Panel;
    std::unique_ptr<JdPanel> jdPanel;
    juce::Component settingsPage;
    juce::Label settingsNote;
    juce::Array<juce::File> listedPatches;
    juce::String importedBankSignature;
    int timerTicks = 0;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    struct LegacyPage : juce::Component
    {
        std::function<void (juce::Rectangle<int>)> layout;
        void resized() override { if (layout) layout (getLocalBounds().reduced (4)); }
    };
    LegacyPage d50PartialPage, d50CommonPage, jdTonePage, jdFxPage;
    juce::TextButton toneButtons[4];

    // performance
    std::unique_ptr<WheelControl> pitchWheel, modWheel;
    std::unique_ptr<MidiActivityLed> midiLed;
    std::unique_ptr<LevelMeter> levelMeter;
    juce::Label bendInfo;
    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFileMessage;
    int shownProgram = -1;
    int currentJdTone = 0;
    float uiScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DigitalComboEditor)
};
