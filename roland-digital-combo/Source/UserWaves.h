#pragma once
// Waves the user supplies from their own WAV/AIFF files. They replace the generated substitutes
// for individual D-50 PCM numbers (1-100) or JD-800 waveform numbers (1-108). Nothing is
// bundled: the plug-in only plays what the user loads from their own disk.
//
// Plain C++ (no JUCE) so the engines can use it; the file loader lives in UserWaveLoader.*.

#include "DspCommon.h"

namespace dc
{
struct UserWaveSet
{
    static constexpr int kNumD50 = 100, kNumJd = 108;
    SampleWave d50[kNumD50];
    SampleWave jd[kNumJd];

    const SampleWave* d50Wave (int index0) const noexcept
    {
        return index0 >= 0 && index0 < kNumD50 && ! d50[index0].empty() ? &d50[index0] : nullptr;
    }
    const SampleWave* jdWave (int index0) const noexcept
    {
        return index0 >= 0 && index0 < kNumJd && ! jd[index0].empty() ? &jd[index0] : nullptr;
    }
    int countD50() const noexcept { int n = 0; for (auto& w : d50) n += w.empty() ? 0 : 1; return n; }
    int countJd() const noexcept  { int n = 0; for (auto& w : jd) n += w.empty() ? 0 : 1; return n; }
};
} // namespace dc
