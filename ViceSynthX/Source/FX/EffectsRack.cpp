#include "EffectsRack.h"

namespace vsx
{
void EffectsRack::mixLaw (int type, float mix, float& dryG, float& wetG) noexcept
{
    // Insert effects output the processed dry signal (measured dry/wet correlation 0.95-1.0): a linear crossfade, so
    // 50 % is the average of dry and processed instead of a coherent +6 dB sum. Modulation, delay and reverb effects
    // add a partly decorrelated signal: an equal-power crossfade. (The previous law kept both at unity at 50 %,
    // measured +5.7 to +8.1 dB for insert effects; see Tests/TestFxLaws.cpp.)
    using namespace pid;
    switch (type)
    {
        case FxDistortion: case FxTube: case FxTape: case FxEq: case FxMbComp: case FxStereo: case FxLimiter:
            dryG = 1.0f - mix;
            wetG = mix;
            break;
        default:
            dryG = std::cos (mix * 1.5707963f);
            wetG = std::sin (mix * 1.5707963f);
            break;
    }
}

using namespace pid;

EffectsRack::EffectsRack()
{
    for (int t = 0; t < NumFx; ++t)
    {
        effects[(size_t) t] = createEffect (t);
        lastOrder[(size_t) t] = t;
    }
}

void EffectsRack::prepare (double s, int maxBlock)
{
    sr = s;
    for (auto& e : effects) e->prepare (sr, maxBlock);
    dry.setSize (2, maxBlock);
    busA.setSize (2, maxBlock);
    busB.setSize (2, maxBlock);
    enableStep = (float) (1.0 / (0.01 * sr));
    reset();
}

void EffectsRack::reset()
{
    for (auto& e : effects) e->reset();
    enableGain.fill (0.0f);
    wasActive.fill (false);
}

void EffectsRack::updateConvolution (const ParamSnapshot& p, const juce::String& userIrPath)
{
    convolutionLoadImpulse (*effects[FxConvolution], p.geti (FX (FxConvolution, conv::Impulse)),
                            p.get (FX (FxConvolution, conv::Length)), userIrPath);
}

void EffectsRack::runChain (float* l, float* r, int n, const FxContext& ctx, const std::array<int, NumFx>& order, int bus,
                            bool parallel) noexcept
{
    const auto& p = *ctx.params;
    float* dl = dry.getWritePointer (0);
    float* dr = dry.getWritePointer (1);
    for (int k = 0; k < NumFx; ++k)
    {
        const int t = order[(size_t) k];
        if (parallel && p.geti (FX (t, fx::Bus)) != bus)
            continue;
        const bool on = p.getb (FX (t, fx::On));
        float& g = enableGain[(size_t) t];
        if (! on && g <= 0.0f)
        {
            if (wasActive[(size_t) t]) { effects[(size_t) t]->reset(); wasActive[(size_t) t] = false; }
            continue;
        }
        wasActive[(size_t) t] = true;

        std::copy (l, l + n, dl);
        std::copy (r, r + n, dr);
        effects[(size_t) t]->process (l, r, n, ctx);

        const float mix = juce::jlimit (0.0f, 1.0f, ctx.mods->get (p, FX (t, fx::Mix)));
        float dryG, wetG;
        mixLaw (t, mix, dryG, wetG);
        for (int i = 0; i < n; ++i)
        {
            g = on ? std::min (1.0f, g + enableStep) : std::max (0.0f, g - enableStep);
            const float dw = 1.0f + (dryG - 1.0f) * g, ww = wetG * g;
            float yl = dl[i] * dw + l[i] * ww;
            float yr = dr[i] * dw + r[i] * ww;
            if (! std::isfinite (yl) || ! std::isfinite (yr))
            {
                // protect the chain: drop the effect state and pass the dry signal
                effects[(size_t) t]->reset();
                yl = dl[i]; yr = dr[i];
            }
            l[i] = yl;
            r[i] = yr;
        }
    }
}

void EffectsRack::process (float* l, float* r, int n, const FxContext& ctx, const PatchState& patch) noexcept
{
    // validated rack order
    std::array<int, NumFx> order {};
    std::array<bool, NumFx> seen {};
    bool valid = true;
    for (int i = 0; i < NumFx; ++i)
    {
        const int t = patch.fxOrder[(size_t) i].load (std::memory_order_relaxed);
        if (t < 0 || t >= NumFx || seen[(size_t) t]) { valid = false; break; }
        seen[(size_t) t] = true;
        order[(size_t) i] = t;
    }
    if (valid) lastOrder = order;
    else order = lastOrder;

    const bool parallel = ctx.params->geti (G (global::FxRouting)) == 1;
    if (! parallel)
    {
        runChain (l, r, n, ctx, order, 0, false);
        return;
    }

    float* al = busA.getWritePointer (0); float* ar = busA.getWritePointer (1);
    float* bl = busB.getWritePointer (0); float* br = busB.getWritePointer (1);
    std::copy (l, l + n, al); std::copy (r, r + n, ar);
    std::copy (l, l + n, bl); std::copy (r, r + n, br);
    runChain (al, ar, n, ctx, order, 0, true);
    runChain (bl, br, n, ctx, order, 1, true);
    const float bal = ctx.mods->get (*ctx.params, G (global::FxBalance));
    for (int i = 0; i < n; ++i)
    {
        l[i] = al[i] * (1.0f - bal) + bl[i] * bal;
        r[i] = ar[i] * (1.0f - bal) + br[i] * bal;
    }
}

} // namespace vsx
