#pragma once

#include <cmath>
#include <vector>

namespace vsx
{
/**
    YIN fundamental-frequency estimator (de Cheveigne & Kawahara, 2002):
    difference function -> cumulative mean normalised difference -> absolute threshold
    -> parabolic interpolation. Returns the period in samples, or 0 if unvoiced.
*/
inline float yinPeriod (const float* x, int n, double sampleRate, float minHz = 30.0f, float maxHz = 2000.0f,
                        float threshold = 0.15f, float* confidenceOut = nullptr)
{
    const int maxTau = std::min (n / 2, (int) (sampleRate / minHz));
    const int minTau = std::max (2, (int) (sampleRate / maxHz));
    if (maxTau <= minTau + 2)
        return 0.0f;

    const int w = n - maxTau;
    std::vector<float> d ((size_t) maxTau + 1, 0.0f);
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < w; ++j)
        {
            const float diff = x[j] - x[j + tau];
            sum += (double) diff * diff;
        }
        d[(size_t) tau] = (float) sum;
    }

    std::vector<float> cmnd ((size_t) maxTau + 1, 1.0f);
    double running = 0.0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        running += d[(size_t) tau];
        cmnd[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }

    int best = -1;
    for (int tau = minTau; tau < maxTau; ++tau)
    {
        if (cmnd[(size_t) tau] < threshold)
        {
            while (tau + 1 < maxTau && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        // fall back to the global minimum if it is reasonably periodic
        float minVal = 1.0f;
        for (int tau = minTau; tau < maxTau; ++tau)
            if (cmnd[(size_t) tau] < minVal) { minVal = cmnd[(size_t) tau]; best = tau; }
        if (minVal > 0.35f)
        {
            if (confidenceOut) *confidenceOut = 1.0f - minVal;
            return 0.0f;
        }
    }

    if (confidenceOut) *confidenceOut = 1.0f - cmnd[(size_t) best];

    float period = (float) best;
    if (best > 1 && best < maxTau)
    {
        const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const float denom = a + c - 2.0f * b;
        if (std::abs (denom) > 1.0e-9f)
            period += 0.5f * (a - c) / denom;
    }
    return period;
}
} // namespace vsx
