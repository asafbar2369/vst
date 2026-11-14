#pragma once
// Interface building blocks for the redesigned editor: look and feel, a data-driven control for any
// byte of the D-50 / JD-800 parameter areas, titled cards, envelope graphs and scrolling pages.
// All drawing is original vector graphics (no Roland artwork, logos or photographs).

#include "PluginProcessor.h"

namespace cloud
{
inline const juce::Colour bg { 0xff121417 }, panel { 0xff1a1d22 }, cardBg { 0xff22262d }, cardHi { 0xff2b3038 },
                          line { 0xff3a404a }, text { 0xffeae7e0 }, dim { 0xffa3a9b2 }, jdCol { 0xffff9f2e },
                          d50Col { 0xff35c0d8 }, good { 0xff59e07a }, offLed { 0xff3d424b };
inline juce::Colour engineColour (bool jd) { return jd ? jdCol : d50Col; }
constexpr int kCellW = 80, kCellH = 92;          // one knob cell in design pixels
constexpr float kLabelPx = 13.0f, kValuePx = 13.0f;
}

class CloudLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CloudLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (cloud::kLabelPx + 0.5f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (15.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::FontOptions (juce::jmin (15.0f, (float) h * 0.5f)); }
    juce::Font getLabelFont (juce::Label&) override { return juce::FontOptions (cloud::kLabelPx); }
};

// ---------------------------------------------------------------------------
struct ByteSpec
{
    enum Kind { Auto, Knob, Combo, Toggle, Wave };
    bool jd = false;
    int offset = 0;              // absolute offset in the synth's parameter area (Wave: the waveform MSB byte)
    juce::String label;          // caption; empty = derived from the documented name
    Kind kind = Auto;
};

// One control for one documented parameter byte. Knob, list or switch is chosen from the parameter's
// real range and value names. Where the byte has a host parameter the edit goes through it as a proper
// gesture (so hosts can record automation); otherwise the byte is written directly.
class ByteControl : public juce::Component, private juce::Timer
{
public:
    ByteControl (DigitalComboProcessor&, ByteSpec);
    ~ByteControl() override;

    void resized() override;
    void setOffset (int newOffset);         // aim the same control at the equivalent byte of another tone/partial
    int getOffset() const noexcept { return spec.offset; }
    bool isJd() const noexcept { return spec.jd; }
    ByteSpec::Kind getKind() const noexcept { return kind; }
    int getMin() const noexcept { return lo; }
    int getMax() const noexcept { return hi; }
    int getValue() const;                    // read from the instrument
    void setValue (int v);                   // as a complete user edit (also what the tests use)
    juce::String getText() const;            // current value as displayed
    juce::String getCaption() const { return label.getText(); }
    juce::String getParameterId() const { return paramId; }
    void setDimmed (bool dimmed);            // the parameter has no effect with the current structure
    bool isDimmed() const noexcept { return dimmed; }
    void refresh();                          // pull the shown value from the byte now (the timer does this while visible)
    juce::Slider* getSlider() noexcept { return slider.get(); }
    juce::ComboBox* getCombo() noexcept { return combo.get(); }
    juce::ToggleButton* getToggle() noexcept { return toggle.get(); }

private:
    void timerCallback() override;
    void build();
    void bindParam();
    void loadRange();
    ByteSpec::Kind detect() const;
    juce::String format (int v) const;
    int readByte() const;
    void writeByte (int v);
    void commit (int v, bool partOfGesture);
    void showValue (int v);
    juce::String defaultCaption() const;

    DigitalComboProcessor& proc;
    ByteSpec spec;
    ByteSpec::Kind kind = ByteSpec::Knob;
    int lo = 0, hi = 0, shown = 0;
    bool updating = false, dragging = false, dimmed = false;
    juce::Label label;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    juce::RangedAudioParameter* param = nullptr;
    juce::String paramId;
    std::unique_ptr<juce::ParameterAttachment> attachment;   // declared last: destroyed first
};

// Strips a leading prefix and shortens common words so captions fit a knob cell.
juce::String shortCaption (juce::String name, const juce::StringArray& prefixesToStrip);

// ---------------------------------------------------------------------------
// Titled group of controls on a grid. May carry an on/off LED (e.g. a tone or partial) and a
// "sounding" light; an off card draws its controls dimmed.
class Card : public juce::Component, private juce::Timer
{
public:
    Card (juce::String title, juce::Colour accent, int columns);
    ~Card() override;

    void addControl (std::unique_ptr<juce::Component> c, int colSpan = 1, int rowSpan = 1);
    template <typename T> T* add (std::unique_ptr<T> c, int colSpan = 1, int rowSpan = 1)
    {
        auto* raw = c.get();
        addControl (std::move (c), colSpan, rowSpan);
        return raw;
    }
    void setEnableLed (std::function<bool()> get, std::function<void (bool)> set);
    void setActivity (std::function<bool()> isSounding) { activity = std::move (isSounding); }
    int getContentWidth() const noexcept { return columns * cloud::kCellW + 20; }
    int getContentHeight() const noexcept;
    bool isOn() const noexcept { return on; }
    void clickEnableLed() { if (setEnable) { setEnable (! on); on = getEnable(); refreshState(); } }
    juce::String getTitle() const { return title; }
    void setTitle (juce::String t) { title = std::move (t); repaint(); }
    void refreshState() { timerCallback(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    struct Item { juce::Component* comp; int colSpan, rowSpan; };
    juce::String title;
    juce::Colour accent;
    int columns;
    juce::OwnedArray<juce::Component> owned;
    std::vector<Item> items;
    std::function<bool()> getEnable, activity;
    std::function<void (bool)> setEnable;
    bool on = true, sounding = false, haveLed = false;
    juce::Rectangle<int> ledArea;
};

// Draws the shape of an envelope from its stored bytes (time axis compressed, not to scale).
class EnvelopeGraph : public juce::Component, private juce::Timer
{
public:
    // Returns x,y pairs in 0..1 (x ascending); polled a few times a second.
    EnvelopeGraph (std::function<std::vector<float>()> points, juce::Colour accent);
    ~EnvelopeGraph() override;
    void paint (juce::Graphics&) override;
    void refresh();
    const std::vector<float>& getPoints() const noexcept { return pts; }
private:
    void timerCallback() override;
    std::function<std::vector<float>()> fetch;
    juce::Colour accent;
    std::vector<float> pts;
};

// Wraps cards left to right, top to bottom; scrolls vertically when they do not fit.
class FlowPage : public juce::Component
{
public:
    FlowPage();
    void addCard (Card* c) { cards.add (c); addAndMakeVisible (c); }   // takes ownership
    int layoutFor (int width);              // positions the cards, returns the needed height
    void resized() override { layoutFor (getWidth()); }
    const juce::OwnedArray<Card>& getCards() const noexcept { return cards; }
    ~FlowPage() override;
private:
    juce::OwnedArray<Card> cards;
};

class ScrollPage : public juce::Component
{
public:
    ScrollPage();
    ~ScrollPage() override;
    FlowPage& getFlow() noexcept { return flow; }
    void resized() override;
    juce::Component* header = nullptr;      // optional strip (e.g. a tone selector) kept above the scrolling area
    int headerHeight = 0;
    juce::Viewport& getViewport() noexcept { return viewport; }
private:
    juce::Viewport viewport;
    FlowPage flow;
};
