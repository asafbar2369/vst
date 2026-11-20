#pragma once

#include "Modulation.h"

namespace vsx
{
/**
    Engine-level macro semantics (the eight performance macros on the front panel).

    Each macro (parameter macro1..macro8, 0..1) is applied to the block-level parameter
    snapshot before the voices and effects read it, so every macro works with every preset:
      1 Mood         darker, warmer and more spacious (cutoff down, drift, tube warmth, reverb size)
      2 Filter Sweep both filter cutoffs up, a little resonance
      3 Motion       slow internal smooth-random movement of wavetable position, cutoff and pan
      4 Texture      wavefolding and a noise layer
      5 Chorus       enables the analog chorus and raises its mix
      6 Space        enables the algorithmic reverb and raises mix / size / decay
      7 Drive        filter drive plus the distortion stage
      8 Mix          raises the wet level of the time-based effects and blends in the stereo delay
    The macros remain available as modulation-matrix sources as well.
*/
namespace Macros
{
    struct Target
    {
        int param;
        float amount;   // normalised offset at macro = 1 (ignored for enable targets)
        bool enable;    // boolean target: switched on while the macro is above 1 %
        bool motion;    // scaled by the internal motion signal instead of the macro value
    };

    const std::vector<Target>& targets (int macro);
    juce::String name (int macro);
    juce::String describe (int macro); // tooltip text listing the real targets

    /** out = base with the macro offsets applied. motionA/B are the internal motion signals (-1..1). */
    void apply (const ParamSnapshot& base, ParamSnapshot& out, float motionA, float motionB);
}

} // namespace vsx
