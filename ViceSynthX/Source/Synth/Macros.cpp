#include "Macros.h"

namespace vsx::Macros
{
using namespace pid;

namespace
{
    std::vector<std::vector<Target>> build()
    {
        std::vector<std::vector<Target>> t (kNumMacros);
        auto add = [&] (int m, int param, float amount, bool enable = false, bool motion = false) { t[(size_t) m].push_back ({ param, amount, enable, motion }); };
        // 1 Mood
        add (0, F (0, flt::Cutoff), -0.22f);
        add (0, G (global::OscDrift), 0.4f);
        add (0, FX (FxTube, fx::On), 0.0f, true);
        add (0, FX (FxTube, fx::Mix), 0.45f);
        add (0, FX (FxReverb, reverb::Size), 0.3f);
        // 2 Filter Sweep
        add (1, F (0, flt::Cutoff), 0.7f);
        add (1, F (1, flt::Cutoff), 0.7f);
        add (1, F (0, flt::Resonance), 0.15f);
        // 3 Motion (scaled by the internal motion signal)
        for (int o = 0; o < kNumOscs; ++o) add (2, O (o, osc::Position), 0.25f, false, true);
        add (2, F (0, flt::Cutoff), 0.12f, false, true);
        for (int o = 0; o < kNumOscs; ++o) add (2, O (o, osc::Pan), (o % 2 == 0 ? 0.3f : -0.3f), false, true);
        // 4 Texture
        for (int o = 0; o < kNumOscs; ++o) add (3, O (o, osc::Fold), 0.35f);
        add (3, NOISE (noise::On), 0.0f, true);
        add (3, NOISE (noise::Level), 0.0f); // level handled below (absolute)
        // 5 Chorus
        add (4, FX (FxChorus, fx::On), 0.0f, true);
        add (4, FX (FxChorus, fx::Mix), 0.6f);
        // 6 Space
        add (5, FX (FxReverb, fx::On), 0.0f, true);
        add (5, FX (FxReverb, fx::Mix), 0.5f);
        add (5, FX (FxReverb, reverb::Size), 0.3f);
        add (5, FX (FxReverb, reverb::Decay), 0.25f);
        // 7 Drive
        add (6, F (0, flt::Drive), 0.6f);
        add (6, FX (FxDistortion, fx::On), 0.0f, true);
        add (6, FX (FxDistortion, fx::Mix), 0.45f);
        // 8 Mix
        add (7, FX (FxStereoDelay, fx::On), 0.0f, true);
        add (7, FX (FxStereoDelay, fx::Mix), 0.45f);
        add (7, FX (FxReverb, fx::Mix), 0.2f);
        add (7, FX (FxChorus, fx::Mix), 0.2f);
        add (7, FX (FxTapeDelay, fx::Mix), 0.25f);
        add (7, FX (FxDigitalDelay, fx::Mix), 0.25f);
        return t;
    }
}

const std::vector<Target>& targets (int macro)
{
    static const auto all = build();
    return all[(size_t) juce::jlimit (0, kNumMacros - 1, macro)];
}

juce::String name (int m)
{
    static const char* names[] = { "Mood", "Filter Sweep", "Motion", "Texture", "Chorus", "Space", "Drive", "Mix" };
    return names[juce::jlimit (0, 7, m)];
}

juce::String describe (int m)
{
    const auto& reg = ParamRegistry::get();
    juce::StringArray lines;
    for (const auto& t : targets (m))
    {
        if (t.param == NOISE (noise::Level)) { lines.add ("Noise layer level up to 25 %"); continue; }
        if (t.enable) lines.add ("Switches on: " + reg[t.param].name.upToLastOccurrenceOf (" On", false, false));
        else lines.add (reg[t.param].name + (t.motion ? " (slow random motion " : " (") + (t.amount >= 0 ? "+" : "") + juce::String ((int) std::round (t.amount * 100.0f)) + " %)");
    }
    return "Macro " + juce::String (m + 1) + ": " + name (m) + "\n" + lines.joinIntoString ("\n");
}

void apply (const ParamSnapshot& base, ParamSnapshot& out, float motionA, float motionB)
{
    out.value = base.value;
    const auto& reg = ParamRegistry::get();
    for (int m = 0; m < kNumMacros; ++m)
    {
        const float v = base.value[(size_t) MACRO (m)];
        if (v <= 1.0e-4f) continue;
        for (const auto& t : targets (m))
        {
            const auto& d = reg[t.param];
            float& dst = out.value[(size_t) t.param];
            if (t.enable)
            {
                if (v > 0.01f) dst = 1.0f;
                continue;
            }
            if (t.param == NOISE (noise::Level))
            {
                dst = std::max (dst * (base.value[(size_t) NOISE (noise::On)] > 0.5f ? 1.0f : 0.0f), 0.25f * v);
                continue;
            }
            const float amt = t.motion ? t.amount * v * (t.param >= kOscBase && t.param < kSubBase && (t.param - kOscBase) % osc::Count == osc::Pan ? motionB : motionA)
                                       : t.amount * v;
            dst = d.fromNormalised (d.toNormalised (dst) + amt);
        }
    }
}

} // namespace vsx::Macros
