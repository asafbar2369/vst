#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/**
    Microtuning with Scala files (.scl scales and .kbm keyboard mappings), written from the published Scala file-format
    description (Manuel Op de Coul, "Scala scale file format" / "Keyboard mapping"). Original code.

    The result is a table of 128 pitches in MIDI-semitone units (60.0 = C4 in 12-TET, fractional values allowed) that the
    voices play instead of the key number; kUnmapped marks keys the mapping leaves silent.
    Without a .kbm: key 60 is degree 0 at 261.6256 Hz (C4 unchanged), linear mapping, the period from the scale.
*/
namespace vsx::tuning
{
constexpr float kUnmapped = -1000.0f;
using Table = std::array<float, 128>;

struct Scale
{
    juce::String description;
    std::vector<double> cents; // degrees 1..N (cents above degree 0); the last entry is the period (e.g. 1200)
};

struct KeyMap
{
    int size = 0;                 // 0 = linear mapping (every key is the next degree)
    int first = 0, last = 127;    // keys outside this range keep 12-TET pitch
    int middle = 60;              // key where mapping entry 0 (degree 0) sits
    int refNote = 60;             // key whose frequency is given
    double refFreq = 261.6255653; // Hz
    int octaveDegree = 0;         // degree that repeats the mapping (0 = the scale size)
    std::vector<int> map;         // per mapping entry: scale degree, or -1 = unmapped ("x")
};

bool parseScl (const juce::String& text, Scale& out, juce::String& error);
bool parseKbm (const juce::String& text, KeyMap& out, juce::String& error);
Table build (const Scale& scale, const KeyMap* map); // map == nullptr: default mapping
Table standard();                                     // 12-TET identity
} // namespace vsx::tuning
