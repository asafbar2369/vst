#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace vsx
{
namespace Theme
{
    // Requested palette
    const juce::Colour neonPink { 0xffff329d };
    const juce::Colour electricPurple { 0xff963bdf };
    const juce::Colour cyberCyan { 0xff22ddeb };
    const juce::Colour sunsetOrange { 0xffff9252 };
    const juce::Colour darkPurple { 0xff151225 };
    const juce::Colour deepBlack { 0xff08090f };

    // derived
    const juce::Colour panel { 0xff1b1630 };
    const juce::Colour panelLight { 0xff272042 };
    const juce::Colour text { 0xffeee6ff };
    const juce::Colour textDim { 0xff9f93c4 };
    const juce::Colour outline { 0x55963bdf };

    juce::Font font (float height, bool bold = false);
    juce::Font displayFont (float height); // bold, slightly wide for headings
    void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 10.0f, juce::Colour accent = electricPurple);
    void neonText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Colour c, float height,
                   juce::Justification j = juce::Justification::centred);
    juce::Colour accentFor (int index); // cycles pink / cyan / orange / purple
}

class VsxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VsxLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize,
                        bool over, bool down) override;
};

} // namespace vsx
