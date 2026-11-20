#pragma once

#include "Theme.h"

namespace vsx
{
/** Original vector artwork: Miami sunset skyline with palms (no third-party imagery). */
namespace MiamiArtwork
{
    void draw (juce::Graphics& g, juce::Rectangle<float> area, float animPhase = 0.0f, float sunX = 0.5f, float horizonFrac = 0.68f, bool palms = true);
    void drawViceScript (juce::Graphics& g, juce::Rectangle<float> area);
    void drawLogo (juce::Graphics& g, juce::Rectangle<float> area);
    void drawPalm (juce::Graphics& g, juce::Point<float> base, float height, float lean, juce::Colour c);
}

/** Cached, resolution-aware header background. */
class MiamiHeader : public juce::Component
{
public:
    MiamiHeader (float sunXFrac = 0.5f, float horizon = 0.68f, float darken = 0.0f) : sunX (sunXFrac), horizonFrac (horizon), darkenAmount (darken) {}
    void paint (juce::Graphics& g) override;
    void resized() override { cache = {}; }
    float sunX, horizonFrac, darkenAmount;
private:
    juce::Image cache;
    float cacheScale = 0.0f;
};

} // namespace vsx
