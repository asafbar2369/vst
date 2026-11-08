#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "D50SysEx.h"
#include "JD800SysEx.h"

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
void writeInitD50Tone (uint8_t* raw448) noexcept;   // our own neutral tone (not a Roland factory sound)

// A host-automatable parameter stored inside the JD-800 patch (384 bytes).
struct JdBinding
{
    enum Kind { Byte, Waveform, LayerBit };
    juce::String id;
    juce::String name;
    int offset;       // patch offset (Waveform: offset of the MSB byte, LSB follows)
    Kind kind = Byte;
    int bit = 0;      // LayerBit: tone index 0..3 inside "Layer tone"
    int minValue = 0, maxValue = 0;
};
const std::vector<JdBinding>& jdBindings();
juce::String formatJdValue (int patchOffset, int value);
void writeInitJdPatch (uint8_t* raw384) noexcept;   // our own neutral patch (not a Roland factory sound)

// Per-tone JD parameter suffixes (ID = "jd_" + tone letter + "_" + suffix)
const juce::StringArray& jdToneSuffixes();
juce::String jdToneId (int tone, const juce::String& suffix);
const juce::StringArray& jdCommonIds();
const juce::StringArray& jdEffectIds();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

namespace id
{
    inline const juce::String mode { "mode" }, splitPoint { "splitPoint" }, splitSwap { "splitSwap" },
        layerBalance { "layerBalance" }, masterVolume { "masterVolume" }, sysexRx { "sysexRx" },
        sysexChannel { "sysexChannel" }, jdUnit { "jdUnit" };
}

// Built-in presets: original sounds made for this project.
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values; // param id -> plain (denormalised) value
};
const std::vector<Preset>& presets();
} // namespace params
