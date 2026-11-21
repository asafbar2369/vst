#pragma once

#include "EnvLfoEditors.h"
#include "Icons.h"
#include "OscSourceMenu.h"
#include "WaveDisplay.h"

namespace vsx
{
/** Card frame with power toggle + title, matching the cinematic concept. */
class Card : public juce::Component
{
public:
    Card (ViceSynthProcessor& p, juce::String title, int onParam, juce::Colour accent);
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> body() const { return getLocalBounds().reduced (10, 8).withTrimmedTop (30); }
    juce::Rectangle<int> titleRow() const { return getLocalBounds().reduced (10, 8).removeFromTop (24); }
protected:
    ViceSynthProcessor& proc;
    juce::String title;
    juce::Colour accent;
    std::unique_ptr<ParamToggle> power;
};

class OscCard : public Card
{
public:
    OscCard (ViceSynthProcessor& p, int osc);
    void resized() override;
    void refresh() { display.refresh(); }
private:
    ParamCombo mode;
    OscSourceArrow sourceArrow;
    OscDisplay display;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
};

class FilterCard : public Card
{
public:
    FilterCard (ViceSynthProcessor& p, int f);
    void resized() override;
    void refresh() { display.refresh(); }
private:
    ParamCombo model, mode;
    FilterDisplay display;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
};

class EnvCard : public Card
{
public:
    explicit EnvCard (ViceSynthProcessor& p);
    void resized() override;
    void refresh() { editor.repaint(); }
private:
    void select (int e);
    EnvelopeEditor editor;
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::vector<std::unique_ptr<ParamKnob>>> knobs;
    int current = 0;
};

class LfoCard : public Card
{
public:
    explicit LfoCard (ViceSynthProcessor& p);
    void resized() override;
    void refresh() { editor.repaint(); }
private:
    void select (int l);
    LfoEditor editor;
    juce::OwnedArray<juce::TextButton> tabs;
    struct Ctl { std::unique_ptr<ParamCombo> shape; std::unique_ptr<ParamToggle> sync; std::unique_ptr<ParamKnob> rate, depth; };
    std::vector<Ctl> ctls;
    int current = 0;
};

/** First matrix slots as editable rows: source > destination, amount bar (full matrix on Sound Design). */
class ModCard : public Card, private juce::Timer
{
public:
    explicit ModCard (ViceSynthProcessor& p);
    ~ModCard() override;
    void resized() override;
private:
    class Row;
    void timerCallback() override;
    juce::OwnedArray<Row> rows;
};

class StudioView : public juce::Component
{
public:
    explicit StudioView (ViceSynthProcessor& p);
    void resized() override;
    void refresh();
private:
    std::vector<std::unique_ptr<OscCard>> oscs;
    std::vector<std::unique_ptr<FilterCard>> filters;
    std::unique_ptr<EnvCard> env;
    std::unique_ptr<LfoCard> lfo;
    std::unique_ptr<ModCard> mod;
};

} // namespace vsx
