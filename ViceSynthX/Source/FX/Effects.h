#pragma once

#include "../Core/DspUtils.h"
#include "../Synth/Modulation.h"
#include <juce_dsp/juce_dsp.h>

namespace vsx
{
struct FxContext
{
    const ParamSnapshot* params = nullptr;
    const ModOffsets* mods = nullptr;
    double bpm = 120.0;
};

/** RBJ biquad (transposed direct form II). */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    enum Type { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf, AllPass };
    void set (Type t, double sr, float freq, float q, float gainDb = 0.0f);
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
};

/** Power-of-two circular delay line with fractional (cubic) reads. */
class DelayLine
{
public:
    void prepare (int maxSamples);
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) noexcept { buf[(size_t) (write = (write + 1) & mask)] = x; }
    float read (float delaySamples) const noexcept;
    float readInt (int delaySamples) const noexcept { return buf[(size_t) ((write - delaySamples) & mask)]; }
    int maxDelay() const noexcept { return mask - 4; }

private:
    std::vector<float> buf;
    int mask = 0, write = 0;
};

class Effect
{
public:
    explicit Effect (int fxType) : type (fxType) {}
    virtual ~Effect() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    /** Processes in place, producing the fully wet signal (the rack applies dry/wet mix). */
    virtual void process (float* l, float* r, int n, const FxContext& c) noexcept = 0;

    const int type;

protected:
    float get (const FxContext& c, int p) const noexcept { return c.mods->get (*c.params, pid::FX (type, p)); }
    int geti (const FxContext& c, int p) const noexcept { return c.params->geti (pid::FX (type, p)); }
    double sr = 44100.0;
};

std::unique_ptr<Effect> createEffect (int fxType);

/** Convolution reverb exposes IR loading for the message thread. */
class ConvolutionReverb;
void convolutionLoadImpulse (Effect& e, int choice, float lengthFraction, const juce::String& userPath);
juce::AudioBuffer<float> generateImpulse (int choice, double sampleRate);

} // namespace vsx
