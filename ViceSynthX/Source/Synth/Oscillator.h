#pragma once

#include "../Core/DspUtils.h"
#include "../Core/Params.h"
#include "SampleBank.h"
#include "Wavetable.h"
#include "FmVoice.h"
#include <array>

namespace vsx
{
enum class OscMode { Wavetable = 0, Analog, Sample, Multisample, Granular, Spectral, FM };
enum class WarpMode { Off = 0, Sync, BendPlus, BendMinus, Pwm, Mirror, Asym, Quantize };
enum class XModMode { Off = 0, FM, PM, RM, AM, HardSync };

/** Modulated per-control-tick oscillator settings (all values in real units). */
struct OscTick
{
    bool on = false;
    int mode = 0, source = 0;
    float level = 0.75f, pan = 0.0f;
    float pitchSemis = 60.0f;      // absolute MIDI pitch incl. transpose, bend, glide, drift
    float position = 0.0f;
    int unison = 1;
    float detune = 0.2f, blend = 0.75f, spread = 0.5f, phase = 0.0f, phaseRand = 1.0f;
    int warpMode = 0; float warpAmount = 0.0f;
    int xmodMode = 0, xmodSource = 0; float xmodAmount = 0.0f;
    float fold = 0.0f, pulseWidth = 0.5f;
    int loopMode = 0; float loopStart = 0.0f, loopEnd = 1.0f, loopXfade = 0.02f; int rootNote = 60;
    float sampleEnd = 1.0f; bool rootAuto = false;
    float fmAmount = 1.0f;
    float grainSize = 80.0f, grainDensity = 30.0f, grainSpray = 0.1f, grainPitchRand = 0.0f; int grainWindow = 0; float grainSpread = 0.5f;
    float specTilt = 0.0f, specOddEven = 0.0f, specStretch = 0.0f, specFormant = 0.0f; int specPartials = 64; float specSpeed = 1.0f;
};

struct OscResourcesView
{
    const Wavetable* wavetable = nullptr;
    const SampleData* sample = nullptr;
    const MultiSample* multi = nullptr;
    const fm::Patch* fm = nullptr;
};

/** Per-voice oscillator state and renderer. */
class OscVoice
{
public:
    static constexpr int kMaxGrains = 32;
    static constexpr int kPartials = 64;

    void prepare (double sampleRateOs);
    void noteOn (const OscTick& t, int note, int velocity, const OscResourcesView& res, uint32_t seed);
    void beginTick (const OscTick& t, int numSamples, const OscResourcesView& res);
    void noteOff() noexcept { fmRenderer.noteOff(); }

    /** Renders one sample. xmod = value of the selected modulation source, syncFrac >= 0 if the
        sync source wrapped during this sample (fraction of the sample after the wrap). */
    void render (float xmod, float syncFrac, float& outL, float& outR) noexcept;

    float lastMono = 0.0f;
    float wrapFrac = -1.0f; // >= 0 if the centre voice wrapped this sample (for hard sync of others)
    bool active = false;

private:
    float renderWavetable (int u, float inc, float xmodPhase) noexcept;
    /** Per-sample values shared by every unison voice of the analog oscillator (computed once per sample). */
    struct AnalogSetup { float dtMul = 1.0f, pw = 0.5f; bool pwm = false, warp = false; int seg = 0; float t = 0.0f; };
    AnalogSetup analogSetup() const noexcept;
    float renderAnalog (int u, float inc, float xmodPhase, const AnalogSetup& a) noexcept;
    void renderSampleVoices (float rateMul, float& l, float& r) noexcept;
    void renderGranular (float& l, float& r) noexcept;
    void renderSpectral (float fmMul, float& l, float& r) noexcept;
    void spawnGrain() noexcept;
    float warpPhase (float p, float& harmonicMult) const noexcept;
    void computeUnisonLayout (int n, float detune, float blend, float spread) noexcept;
    void resolveZone() noexcept;
    float readSample (const SampleData& s, int level, int ch, double pos) const noexcept;

    double sr = 88200.0;
    OscTick tick;
    OscResourcesView res;
    double playEnd = 1.0e12; // sample-mode end of playback (samples)
    const SampleZone* zone = nullptr;
    const MultiSample* zoneOwner = nullptr;
    int note = 60, velocity = 100;
    Rng rng;

    // unison
    int numUnison = 1;
    std::array<float, kMaxUnison> phase {}, inc {}, incStep {}, detuneRatio {}, gainL {}, gainR {};
    float baseIncCur = 0.0f, baseIncStep = 0.0f;

    // wavetable read state
    int frameA = 0, frameB = 0; float frameFrac = 0.0f; int level = 0;
    const Wavetable* vaTables = nullptr;

    // sample state
    std::array<double, kMaxUnison> spos {};
    std::array<int, kMaxUnison> sdir {};
    std::array<bool, kMaxUnison> sended {};
    double sampleRateRatio = 1.0; // source sr / output sr
    float zoneGain = 1.0f, zonePan = 0.0f;
    int activeLoopMode = 0; double loopS = 0.0, loopE = 0.0, xfadeLen = 0.0;

    // granular
    struct Grain
    {
        bool active = false;
        double pos = 0.0, rate = 1.0;
        float phase = 0.0f, phaseInc = 0.0f; // wavetable-source grains
        int frame = 0;
        int age = 0, length = 1;
        float panL = 0.7f, panR = 0.7f;
    };
    std::array<Grain, kMaxGrains> grains {};
    double grainTimer = 0.0;
    int grainCounter = 0;

    // spectral
    std::array<std::array<float, kPartials>, kMaxUnison> pphase {};
    std::array<float, kPartials> ampCur {}, ampStep {}, ratio {};
    double specTime = 0.0;
    int activePartials = 0;

    // six-operator FM
    fm::FmRenderer fmRenderer;
    bool fmStarted = false;
    uint32_t fmSeed = 1;

    // smoothing of level/pan
    Ramp levelRamp, panRamp;
};

/** Sub oscillator (one per voice). */
class SubOsc
{
public:
    void prepare (double s) { sr = s; }
    void noteOn (uint32_t seed) { Rng r (seed); phase = r.uni(); }
    float render (int shape, float freq) noexcept;
    float phase = 0.0f;
    float wrapFrac = -1.0f;
private:
    double sr = 88200.0;
};

/** Noise oscillator (one per voice, stereo). */
class NoiseOsc
{
public:
    void prepare (double s);
    void noteOn (uint32_t seed);
    void render (int type, float color, float width, float rateHz, float& l, float& r) noexcept;
private:
    double sr = 88200.0;
    Rng rngL, rngR;
    float pinkL[7] {}, pinkR[7] {};
    float brownL = 0, brownR = 0;
    float holdL = 0, holdR = 0, holdPhase = 0;
    float crackle = 0;
    OnePoleLP tiltLpL, tiltLpR;
    float lastColor = 99.0f;
    float pink (float* b, float white) noexcept;
};

} // namespace vsx
