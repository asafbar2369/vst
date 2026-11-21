#pragma once

#include "Widgets.h"

namespace vsx
{
/**
    Direct waveform / source chooser opened from the small arrow next to every oscillator's mode selector.
    Analog shapes (sine, triangle, saw, square, pulse), factory and user wavetables, built-in samples and
    multisamples, and LOAD SAMPLE... in one menu. Choosing an entry also switches the oscillator to the matching
    mode. The current choice is ticked.
*/
namespace OscSourceMenu
{
    void show (ViceSynthProcessor& proc, int osc, juce::Component& target, std::unique_ptr<juce::FileChooser>& chooser);

    /** Applies one of the analog shapes directly (0 sine, 1 triangle, 2 saw, 3 square, 4 pulse 25 %). Used by tests. */
    void applyAnalogShape (ViceSynthProcessor& proc, int osc, int shape);
}

/** The small dropdown arrow itself. */
class OscSourceArrow : public juce::Button
{
public:
    OscSourceArrow (ViceSynthProcessor& p, int osc);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void clicked() override;
private:
    ViceSynthProcessor& proc;
    int osc;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace vsx
