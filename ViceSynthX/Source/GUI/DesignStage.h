#pragma once
#include "VectorPage.h"

#include "FmEditor.h"

#include "HeaderHero.h"
#include "SynthPanels.h"
#include "TabPages.h"

namespace vsx
{
/**
    SOUND DESIGN page: every engine section at full depth. Sub-pages reuse the detailed
    editors (oscillator panels with wavetable/sample tools, filters with drag-and-drop
    modulation chips, the complete 32-slot matrix, LFOs, envelopes, the FX rack, arp,
    step sequencer and mixer).
*/
class SoundDesignView : public juce::Component
{
public:
    enum Page { Oscillators, Filters, Matrix, Lfos, Envelopes, Effects, Arp, Seq, Mixer, Fm, Vector, NumPages };
    explicit SoundDesignView (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void showPage (int page);
    int getPage() const { return current; }
    void resourceChanged (int osc);
    std::function<void (int)> onOpenWavetableEditor;
    std::function<void (int)> onPageChanged;

private:
    juce::OwnedArray<juce::TextButton> tabs;
    juce::Component oscPage, filterPage;
    std::array<std::unique_ptr<OscPanel>, kNumOscs> oscPanels;
    std::unique_ptr<SubNoisePanel> subNoise;
    std::array<std::unique_ptr<FilterPanel>, kNumFilters> filterPanels;
    std::unique_ptr<ModulationSection> modSection;
    std::unique_ptr<MatrixPage> matrix;
    std::unique_ptr<LfoPage> lfos;
    std::unique_ptr<EnvPage> envs;
    std::unique_ptr<FxPage> fx;
    std::unique_ptr<ArpPage> arp;
    std::unique_ptr<SeqPage> seq;
    std::unique_ptr<MixerPage> mixer;
    std::unique_ptr<VectorPage> vector;
    std::unique_ptr<FmEditor> fmEditor;
    std::vector<juce::Component*> pages;
    int current = Matrix;
};

/** STAGE page: large preset display, live output, master level and eight quick-load slots. */
class StageView : public juce::Component
{
public:
    explicit StageView (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void rebuildSlots();
    ViceSynthProcessor& proc;
    PresetHero hero;
    Oscilloscope scope;
    LevelMeter meter;
    std::unique_ptr<ParamKnob> master;
    juce::OwnedArray<juce::TextButton> slots;
    std::vector<juce::String> slotNames;
    juce::String slotSource;
    int lastEntryCount = -1, lastFavCount = -1;
};

/** Modal card shown over the whole interface (settings, help, AI history). Esc or click outside closes. */
class Overlay : public juce::Component
{
public:
    Overlay();
    void show (const juce::String& title, juce::Component* content, juce::Point<int> size);
    void hide();
    juce::Component* getContent() const { return content; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Component* content = nullptr;
    juce::String title;
    juce::Rectangle<int> card;
    juce::TextButton close { "CLOSE" };
};

/** Read-only quick help (controls, shortcuts and what is / is not implemented). */
class HelpView : public juce::Component
{
public:
    HelpView();
    void resized() override;
private:
    juce::TextEditor text;
};

} // namespace vsx
