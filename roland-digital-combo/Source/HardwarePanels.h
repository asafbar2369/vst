#pragma once
// Front-panel style editors, laid out after the original instruments' control sections
// (JD-800: slider sections + tone select; D-50: LCD, block/parameter buttons, data entry, joystick).
// All drawing is original vector graphics - no photographs, logos or Roland artwork.

#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// JD-800: sliders act on every tone whose ACTIVE (edit) button is lit, like the original's
// tone select; LAYER buttons choose which tones sound.
class JdPanel : public juce::Component, private juce::Timer
{
public:
    explicit JdPanel (DigitalComboProcessor&);
    ~JdPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    struct Section { juce::String name; std::vector<int> offsets; int row; };
    static const std::vector<Section>& sections();     // tone offsets per panel section

    // For tests and automation of the panel itself
    juce::Slider* sliderFor (int toneOffset) noexcept;
    juce::TextButton& layerButton (int t) noexcept { return layerButtons[t]; }
    juce::TextButton& activeButton (int t) noexcept { return activeButtons[t]; }
    juce::String getReadout() const { return readout; }
    int getNumSliders() const noexcept { return (int) sliders.size(); }

private:
    void timerCallback() override;
    int displayTone() const noexcept;                   // first ACTIVE tone (0 if none)
    void writeToActiveTones (int toneOffset, int value);
    void toggleCommonBit (int commonOffset, int bit);

    DigitalComboProcessor& proc;
    struct SliderItem
    {
        int offset = 0;
        std::unique_ptr<juce::Slider> slider;
        juce::Rectangle<int> labelArea;
        juce::String label;
    };
    std::vector<SliderItem> sliders;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> sectionFrames;
    juce::TextButton layerButtons[4], activeButtons[4];
    juce::ComboBox waveBox, filterModeBox, lfoWave[2];
    juce::String readout;
    bool updating = false;
};

// ---------------------------------------------------------------------------
class JoystickPad : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setPosition (float x, float y, bool notify);    // -1..1, y up = +1
    float getX() const noexcept { return px; }
    float getY() const noexcept { return py; }
    bool springBack = false;                               // VALUE mode returns to the centre
    std::function<void (float, float)> onMove;
private:
    float px = 0.0f, py = 0.0f;
};

// D-50: 2-line LCD, block buttons (Upper/Lower, Partial 1/2, Common, Patch), parameter-group
// buttons, cursor buttons, data entry slider with INC/DEC, and the joystick (BALANCE / VALUE).
class D50Panel : public juce::Component, private juce::Timer
{
public:
    explicit D50Panel (DigitalComboProcessor&);
    ~D50Panel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    struct Group { const char* name; std::vector<int> offsets; };
    static const std::vector<Group>& groupsFor (int blockKind); // 0 partial, 1 common, 2 patch

    // Programmatic control (used by the tests; the buttons call the same functions)
    void selectBlock (int block);      // 0 U-P1, 1 U-P2, 2 U-COM, 3 L-P1, 4 L-P2, 5 L-COM, 6 PATCH
    void selectGroup (int group);
    void selectParam (int indexInGroup);
    void increment (int delta);
    int getSelectedOffset() const noexcept; // absolute temp-area offset 0..447
    juce::String getLcdLine (int line) const;
    juce::Slider& getDataEntry() noexcept { return dataEntry; }
    JoystickPad& getJoystick() noexcept { return joystick; }
    void setJoystickMode (bool valueMode);
    int getNumGroups() const noexcept { return (int) groupButtons.size(); }

private:
    void timerCallback() override;
    void rebuildGroupButtons();
    void refreshLcd();
    int blockKind() const noexcept { return block == 6 ? 2 : (block % 3 == 2 ? 1 : 0); }
    int blockBase() const noexcept;

    DigitalComboProcessor& proc;
    int block = 0, group = 0, param = 0;
    juce::String lcd[2];
    juce::TextButton toneButtons[2], partButtons[3], patchButton { "PATCH" };
    std::vector<std::unique_ptr<juce::TextButton>> groupButtons;
    juce::TextButton cursorLeft { "<" }, cursorRight { ">" }, incButton { "INC" }, decButton { "DEC" };
    juce::TextButton balanceMode { "BALANCE" }, valueMode { "VALUE" };
    juce::Slider dataEntry { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    JoystickPad joystick;
    juce::Rectangle<int> lcdArea;
    bool updating = false;
    float joyRepeat = 0.0f;
};
