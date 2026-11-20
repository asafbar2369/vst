#pragma once

#include "Theme.h"

namespace vsx
{
/** Resolution-independent vector icons (original drawings). */
namespace Icons
{
    enum class Type { Library, Studio, SoundDesign, Stage, Heart, HeartFilled, Undo, Redo, Gear, Help, Power, Search, Filter, Folder,
                      Sparkle, Refresh, User, Box, ChevronLeft, ChevronRight, ChevronDown, Macro, Keyboard, Save, Panic, Wave, Play, Link, History, Audio };

    void draw (juce::Graphics& g, Type t, juce::Rectangle<float> r, juce::Colour c, float stroke = 1.6f);
}

/** Flat icon button (optionally with a label) used throughout the header and panels. */
class IconButton : public juce::Button
{
public:
    IconButton (Icons::Type t, const juce::String& tip, juce::String label = {});
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setIcon (Icons::Type t) { type = t; repaint(); }
    juce::Colour accent = Theme::neonPink;
    bool showBackground = false;

private:
    Icons::Type type;
    juce::String text;
};

} // namespace vsx
