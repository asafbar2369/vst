#pragma once

#include "Icons.h"
#include "MiamiArtwork.h"
#include "Widgets.h"

namespace vsx
{
/** Stereo output meter with a latching CLIP indicator (click to reset). Peaks come from the processor's output stage. */
class OutputMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit OutputMeter (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.resetClipLatch(); repaint(); }
private:
    void timerCallback() override;
    ViceSynthProcessor& proc;
    float shownL = 0.0f, shownR = 0.0f;
    bool clip = false;
};

/** Top bar: logo, STUDIO / SOUND DESIGN / FM, INIT, panel toggles, MASTER VOL + meters + CLIP, PANIC, FX BYPASS,
    SIZE, favourite, undo / redo, CPU, the single SETTINGS button and help. */
class MainHeader : public juce::Component
{
public:
    explicit MainHeader (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void setActiveTab (int t);
    void setScaleDisplay (float scale);
    std::function<void (float)> onScale;
    std::function<void (int)> onTab, onTogglePanel;
    std::function<void()> onFm;
    std::function<void()> onSettings, onHelp;
    /** Panel visibility toggles (0 presets, 1 AI, 2 macros, 3 keyboard). */
    enum Panel { AIPanel = 0, PresetsPanel, MacrosPanel, KeysPanel, NumPanels }; // order shared with PR #10's stored layout
    std::function<void (int panel, bool visible)> onPanelToggle;
    void setPanelVisible (int panel, bool visible);

private:
    ViceSynthProcessor& proc;
    MiamiHeader art { 0.80f, 0.72f, 0.0f };
    juce::OwnedArray<IconButton> tabs;
    IconButton fav { Icons::Type::Heart, "Favourite this preset" }, undo { Icons::Type::Undo, "Undo (preset loads, AI changes)" },
        redo { Icons::Type::Redo, "Redo" }, settings { Icons::Type::Gear, "Settings (audio quality, AI provider, interface)" },
        help { Icons::Type::Help, "Quick help" };
    juce::Label cpu;
    juce::TextButton sizeButton { "SIZE 85%" }, initButton { "INIT" };
    juce::OwnedArray<juce::TextButton> panelButtons;
    std::vector<int> tabViews;                 // view id of each navigation tab
    IconButton fmTab { Icons::Type::Wave, "FM editor (six-operator FM, cartridges)", "FM" },
        panicB { Icons::Type::Panic, "PANIC: stop all sound immediately (voices, tails, arp)" };
    juce::TextButton fxBypass { "FX BYPASS" };
    std::unique_ptr<ParamKnob> masterVol;
    OutputMeter meter { proc };
    juce::Rectangle<int> outBox;
    juce::Rectangle<int> panelBox;
    float currentScale = 0.85f;
    void showSizeMenu();
    juce::Rectangle<int> navBox;
};

/** Preset hero: name, tags, favourite, prev/next, panic and the skyline artwork with the live output waveform. */
class PresetHero : public juce::Component
{
public:
    explicit PresetHero (ViceSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    bool compact = false; // Stage view uses a taller variant

private:
    ViceSynthProcessor& proc;
    MiamiHeader art { 0.78f, 0.74f, 0.0f };
    IconButton prev { Icons::Type::ChevronLeft, "Previous preset" }, next { Icons::Type::ChevronRight, "Next preset" },
        fav { Icons::Type::Heart, "Favourite" }, panic { Icons::Type::Panic, "Panic: stop all notes" }, play { Icons::Type::Play, "Audition this preset" };
    void layoutFav();
    float nameHeight() const { return compact ? 34.0f : 54.0f; }
    juce::String name, tags;
    juce::Rectangle<int> artBounds, waveBounds;
};

} // namespace vsx
