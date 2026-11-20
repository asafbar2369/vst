#pragma once

#include "Effects.h"
#include "../Core/PatchState.h"

namespace vsx
{
/**
    Reorderable effects rack.
    - Order comes from PatchState::fxOrder (validated as a permutation every block).
    - Serial: every enabled effect in order. Parallel A|B: bus A and bus B chains each process
      the dry input and are blended with "FX A/B Balance".
    - Per-effect enable is smoothed (10 ms) so bypassing is click-free; disabled effects are
      skipped entirely (and reset) once faded out.
*/
class EffectsRack
{
public:
    EffectsRack();
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const FxContext& ctx, const PatchState& patch) noexcept;

    Effect* getEffect (int type) { return effects[(size_t) type].get(); }

    /** Message thread: keeps the convolution IR in sync with its parameters. */
    void updateConvolution (const ParamSnapshot& p, const juce::String& userIrPath);

private:
    void runChain (float* l, float* r, int n, const FxContext& ctx, const std::array<int, pid::NumFx>& order, int bus, bool parallel) noexcept;

    std::array<std::unique_ptr<Effect>, pid::NumFx> effects;
    std::array<float, pid::NumFx> enableGain {};
    std::array<bool, pid::NumFx> wasActive {};
    std::array<int, pid::NumFx> lastOrder {};
    juce::AudioBuffer<float> dry, busA, busB;
    double sr = 44100.0;
    float enableStep = 0.001f;
};

} // namespace vsx
