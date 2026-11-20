#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace vsx
{
/**
    Perceptually relevant measurements of a sound, used for reference matching, the
    text-to-measurable-target refinement loop and the automated tests.
*/
struct AudioFeatures
{
    static constexpr int kBands = 24;
    static constexpr int kHarmonics = 32;

    double sampleRate = 44100.0;
    double duration = 0.0;
    bool valid = false;

    // dynamics
    float rmsDb = -120.0f, peakDb = -120.0f, crestDb = 0.0f, dynamicRangeDb = 0.0f;

    // pitch
    float f0 = 0.0f;                 // median fundamental (Hz), 0 = unpitched
    float pitchConfidence = 0.0f;
    float pitchGlideSemis = 0.0f;    // pitch change from first to last voiced frame
    float vibratoRate = 0.0f, vibratoDepthCents = 0.0f;

    // spectrum
    std::array<float, kHarmonics> harmonicsDb {}; // relative to the strongest harmonic
    std::array<float, kBands> bandsDb {};          // log-spaced 40 Hz .. 18 kHz, normalised to max = 0 dB
    float centroid = 0.0f, rolloff = 0.0f, flatness = 0.0f;
    float centroidStart = 0.0f, centroidEnd = 0.0f; // brightness evolution

    // envelope
    std::vector<float> envelope;     // RMS at 100 Hz, linear, normalised to peak 1
    float attack = 0.0f, decay = 0.0f, sustain = 0.0f, release = 0.0f;
    float tremoloRate = 0.0f, tremoloDepth = 0.0f;
    int onsets = 0;

    // stereo
    float stereoWidth = 0.0f;        // side/mid RMS ratio
    float correlation = 1.0f;

    static AudioFeatures analyse (const juce::AudioBuffer<float>& audio, double sampleRate, double noteOffSeconds = -1.0);

    juce::String describe() const;

    /** Weighted perceptual distance (0 = identical). */
    static float distance (const AudioFeatures& a, const AudioFeatures& b);
    static float similarity (const AudioFeatures& a, const AudioFeatures& b) { return std::exp (-distance (a, b)); }
};

} // namespace vsx
