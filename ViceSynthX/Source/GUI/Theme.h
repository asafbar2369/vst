#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace vsx
{
namespace Theme
{
    // Design tokens (VICE SYNTH X cinematic v2)
    const juce::Colour neonPink { 0xffed4b9e };       // accent 1
    const juce::Colour electricPurple { 0xffa477eb }; // accent 2
    const juce::Colour cyberCyan { 0xff4cd9ec };      // accent 3
    const juce::Colour sunsetOrange { 0xfff0ab79 };   // accent 4
    const juce::Colour deepBlack { 0xff0c0e15 };      // background
    const juce::Colour darkPurple { 0xff151820 };     // panel (dark)
    const juce::Colour panel { 0xff151820 };
    const juce::Colour panelLight { 0xff1b1e27 };
    const juce::Colour display { 0xff0d1119 };
    const juce::Colour text { 0xffe9eaf2 };
    const juce::Colour textDim { 0xff8e93a5 };
    const juce::Colour outline { 0xff343641 };
    const juce::Colour border { 0xff343641 };

    juce::Font font (float height, bool bold = false);
    juce::Font displayFont (float height); // bold, slightly wide for headings
    void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 10.0f, juce::Colour accent = electricPurple);
    void neonText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Colour c, float height,
                   juce::Justification j = juce::Justification::centred);
    juce::Colour accentFor (int index); // cycles pink / cyan / orange / purple
    void card (juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f, bool raised = true);
    void displayWell (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f);
    juce::Colour macroAccent (int macro); // pink/violet -> cyan -> amber across the strip
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
