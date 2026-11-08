#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "D50SysEx.h"
#include "JD800Engine.h"

namespace params
{
// Play modes requested for the combo
enum Mode { ModeJD800 = 0, ModeD50 = 1, ModeLayer = 2, ModeSplit = 3 };
juce::StringArray modeNames();

// A host-automatable parameter that is stored inside the D-50 temporary area.
struct D50Binding
{
    juce::String id;
    juce::String name;
    int tempOffset;   // 0..447, linear offset inside the temporary area
};
const std::vector<D50Binding>& d50Bindings();

juce::String formatD50Value (int tempOffset, int value);

// Our own neutral starting tone (not a Roland factory sound).
void writeInitD50Tone (uint8_t* raw448) noexcept;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// IDs of global/JD parameters
namespace id
{
    inline const juce::String mode { "mode" }, splitPoint { "splitPoint" }, splitSwap { "splitSwap" },
        layerBalance { "layerBalance" }, masterVolume { "masterVolume" }, sysexRx { "sysexRx" },
        sysexChannel { "sysexChannel" },
        jdLfoRate { "jd_lfoRate" }, jdLfoDepth { "jd_lfoDepth" }, jdBend { "jd_bendRange" },
        jdChorusMix { "jd_chorusMix" }, jdChorusRate { "jd_chorusRate" }, jdDelayTime { "jd_delayTime" },
        jdDelayFb { "jd_delayFeedback" }, jdDelayMix { "jd_delayMix" }, jdRevSize { "jd_reverbSize" },
        jdRevMix { "jd_reverbMix" }, jdVolume { "jd_volume" };
}
// Per-tone JD parameter suffixes (ID = "jd_" + tone letter + "_" + suffix)
const juce::StringArray& jdToneSuffixes();
juce::String jdToneId (int tone, const juce::String& suffix);

// Built-in presets: original sounds made for this project.
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values; // param id -> plain (denormalised) value
};
const std::vector<Preset>& presets();
} // namespace params
