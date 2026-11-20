#pragma once

#include "AudioFeatures.h"
#include "../FX/EffectsRack.h"
#include "../Presets/PresetFormat.h"
#include "../Synth/SynthEngine.h"

namespace vsx
{
/** Renders a PatchSpec offline (own engine instance; never touches the live audio thread). */
class OfflineRenderer
{
public:
    explicit OfflineRenderer (double sampleRate = 44100.0);
    juce::AudioBuffer<float> render (const PatchSpec& spec, int note, int velocity, double noteSeconds, double tailSeconds,
                                     int quality = 1);
    double getSampleRate() const { return sr; }

private:
    double sr;
    PatchState state;
    SynthEngine engine { state };
    EffectsRack rack;
};

struct MatchResult
{
    PatchSpec best;
    float initialSimilarity = 0.0f, finalSimilarity = 0.0f;
    int renders = 0;
    juce::String report;
    bool cancelled = false;
};

/**
    Intelligent sound matching:
      1. initialGuess(): converts measured features into a patch (harmonic spectrum -> custom
         wavetable, envelope, brightness -> filter, vibrato/tremolo -> LFOs, noise, width).
      2. optimise(): renders candidates and minimises the perceptual feature distance with an
         adaptive (1+1) evolution strategy over the most influential continuous parameters.
      3. refineDescriptors(): verifies text goals ("darker", "wider", "softer attack") by
         rendering before/after and iterates until the measured change is achieved.
*/
class SoundMatcher
{
public:
    using Progress = std::function<bool (float progress, float similarity)>; // return false to cancel

    static PatchSpec initialGuess (const AudioFeatures& ref, int& noteOut);
    static MatchResult optimise (const AudioFeatures& ref, const PatchSpec& start, int note, int iterations, OfflineRenderer& r,
                                 const Progress& progress);
    static PatchSpec refineDescriptors (const PatchSpec& before, const PatchSpec& after, const juce::StringArray& descriptors,
                                        OfflineRenderer& r, juce::String& log);
    static juce::String limitationsFor (const AudioFeatures& ref);
};

} // namespace vsx
