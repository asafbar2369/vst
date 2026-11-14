#pragma once
// Test-only type shim. Not an actual JUCE implementation and NOT for plugin builds.
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace juce
{
template <typename T> inline T jlimit (T lo, T hi, T x) { return std::min (hi, std::max (lo, x)); }
class RangedAudioParameter
{
public:
    virtual ~RangedAudioParameter() = default;
    virtual float convertTo0to1 (float x) const { return x; }
    virtual void setValueNotifyingHost (float) {}
};
}
