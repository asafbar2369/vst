#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

/**
    True-peak meter (inter-sample peaks), in the manner of ITU-R BS.1770-4 Annex 2: 4x oversampling with a
    windowed-sinc polyphase interpolator (24 taps per phase, Blackman-Harris window), maximum absolute value over all
    phases and channels. Used by the factory-library calibration and AUTO GAIN so that the polyphonic headroom rule
    holds after reconstruction (D/A conversion, sample-rate conversion, lossy encoding), not only at sample instants.
    Offline only (not used on the audio thread).
*/
namespace vsx
{
class TruePeak
{
public:
    static constexpr int kOversample = 4, kHalf = 12, kTaps = 2 * kHalf;

    TruePeak()
    {
        for (int ph = 0; ph < kOversample; ++ph)
            for (int k = 0; k < kTaps; ++k)
            {
                // tap k multiplies x[i + kHalf - k]; t = that sample's distance from the output point i + ph / 4
                const double t = (double) (kHalf - k) - (double) ph / kOversample;
                const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
                const double u = (t + kHalf) / (2.0 * kHalf); // 0..1 across the window
                const double w = u <= 0.0 || u >= 1.0 ? 0.0
                               : 0.35875 - 0.48829 * std::cos (2.0 * juce::MathConstants<double>::pi * u)
                                 + 0.14128 * std::cos (4.0 * juce::MathConstants<double>::pi * u) - 0.01168 * std::cos (6.0 * juce::MathConstants<double>::pi * u);
                coeff[(size_t) ph][(size_t) k] = (float) (sinc * w);
            }
    }

    /** Linear true peak of the buffer (all channels). */
    float measure (const juce::AudioBuffer<float>& b) const
    {
        float peak = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            const float* x = b.getReadPointer (c);
            const int n = b.getNumSamples();
            for (int i = 0; i < n; ++i)
            {
                peak = std::max (peak, std::abs (x[i]));
                for (int ph = 1; ph < kOversample; ++ph)
                {
                    float acc = 0.0f;
                    for (int k = 0; k < kTaps; ++k)
                    {
                        const int idx = i + kHalf - k; // output point lies between x[i] and x[i + 1]
                        if (idx >= 0 && idx < n) acc += coeff[(size_t) ph][(size_t) k] * x[idx];
                    }
                    peak = std::max (peak, std::abs (acc));
                }
            }
        }
        return peak;
    }

    float measureDb (const juce::AudioBuffer<float>& b) const { return juce::Decibels::gainToDecibels (measure (b), -150.0f); }

private:
    std::array<std::array<float, kTaps>, kOversample> coeff {};
};
} // namespace vsx
