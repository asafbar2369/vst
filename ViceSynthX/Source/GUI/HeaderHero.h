#pragma once

#include "Icons.h"
#include "MiamiArtwork.h"
#include "Widgets.h"

namespace vsx
{
/** Top bar: logo, LIBRARY / STUDIO / SOUND DESIGN / STAGE navigation, utility icons and real CPU readout. */
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
    std::function<void (int)> onTab;
    std::function<void()> onSettings, onHelp;

private:
    ViceSynthProcessor& proc;
    MiamiHeader art { 0.80f, 0.72f, 0.0f };
    juce::OwnedArray<IconButton> tabs;
    IconButton fav { Icons::Type::Heart, "Favourite this preset" }, undo { Icons::Type::Undo, "Undo (preset loads, AI changes)" },
        redo { Icons::Type::Redo, "Redo" }, settings { Icons::Type::Gear, "Settings (audio quality, AI provider, interface)" },
        help { Icons::Type::Help, "Quick help" };
    juce::Label cpu;
    juce::TextButton sizeButton { "SIZE 85%" };
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
