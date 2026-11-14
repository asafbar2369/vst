#pragma once
// Waves the user supplies from their own WAV/AIFF files. They replace the generated substitutes
// for individual D-50 PCM numbers (1-100) or JD-800 waveform numbers (1-108). Nothing is
// bundled: the plug-in only plays what the user loads from their own disk.
//
// Plain C++ (no JUCE) so the engines can use it; the file loader lives in UserWaveLoader.*.

#include <memory>
#include <vector>
#include "DspCommon.h"

namespace dc
{
struct UserWaveSet
{
    static constexpr int kNumD50 = 100, kNumJd = 108;
    SampleWave d50[kNumD50];
    SampleWave jd[kNumJd];

    // Multisampled waves read from the user's own JD-800 PCM ROM dumps (see JdRomImport.h): each
    // waveform is a list of key zones; a zone plays for keys up to and including highKey.
    struct Zone { int highKey = 127; const SampleWave* wave = nullptr; };
    std::vector<Zone> jdZones[kNumJd];
    std::vector<std::unique_ptr<SampleWave>> romSamples;   // owns the zone samples (shared between waves)

    const SampleWave* d50Wave (int index0) const noexcept
    {
        return index0 >= 0 && index0 < kNumD50 && ! d50[index0].empty() ? &d50[index0] : nullptr;
    }
    // A wave file the user loaded for this waveform wins over the ROM; otherwise the ROM zone for `key`.
    const SampleWave* jdWave (int index0, int key = 60) const noexcept
    {
        if (index0 < 0 || index0 >= kNumJd) return nullptr;
        if (! jd[index0].empty()) return &jd[index0];
        const auto& z = jdZones[index0];
        for (const auto& zone : z)
            if (key <= zone.highKey) return zone.wave;
        return z.empty() ? nullptr : z.back().wave;
    }
    bool hasRomJd (int index0) const noexcept { return index0 >= 0 && index0 < kNumJd && ! jdZones[index0].empty(); }
    int countRomJd() const noexcept { int n = 0; for (auto& z : jdZones) n += z.empty() ? 0 : 1; return n; }
    int countD50() const noexcept { int n = 0; for (auto& w : d50) n += w.empty() ? 0 : 1; return n; }
    int countJd() const noexcept  { int n = 0; for (int i = 0; i < kNumJd; ++i) n += jd[i].empty() ? 0 : 1; return n; }   // wave files only
};
} // namespace dc
