#pragma once

#include "../Core/DspUtils.h"
#include <array>
#include <vector>
#include <cstdint>

/**
    Additional synthesis engines (2026-11-21), each one oscillator mode. All are original implementations of
    published, textbook techniques; no third-party code.

    - StringModel  (mode "Physical"): single-loop digital waveguide string (Karplus-Strong / Jaffe-Smith extensions):
      pluck / hammer / bow excitation, pick-position comb, first-order Thiran fractional delay for tuning, one-pole
      loss filter, an allpass dispersion chain for stiffness (stretched partials), and a decay time (T60) control.
    - ModalModel   (mode "Modal"): a bank of up to 32 two-pole resonators with mode-frequency ratios of real objects
      (ideal string, free bar, marimba and vibraphone bars, wine glass, church bell, circular membrane, clamped
      tine) taken from the acoustics literature (Fletcher & Rossing, "The Physics of Musical Instruments").
    - PhaseDistortion (mode "Phase Dist"): a cosine read through a warped phase, in the style of mid-1980s
      phase-distortion synthesizers: saw, square, pulse, double sine, saw-pulse and three windowed resonance shapes,
      with an Amount (DCW) control from pure sine to the full shape.
    - AdditiveBank (mode "Additive"): up to 64 sine partials with an analytic spectrum (tilt, odd / even,
      stretch, formant), partial-dependent decay (higher partials die away faster) and slow per-partial motion.

    Controls shared by Physical and Modal: Excite, Decay, Brightness, Damping, Inharmonicity, Material, and the
    oscillator Position (pick / strike position). Real-time safe: memory is allocated in prepare() only.
*/
namespace vsx
{
struct PhysParams
{
    int excite = 0;          // 0 pluck, 1 hammer / mallet, 2 bow (sustained noise excitation)
    float decay = 2.0f;      // seconds to -60 dB at the fundamental
    float bright = 0.6f;     // excitation brightness 0..1
    float damp = 0.3f;       // frequency-dependent loss 0..1 (higher partials decay faster)
    float inharm = 0.0f;     // string: stiffness 0..1; modal: stretch of the mode ratios -1..1
    int material = 0;        // modal material
    float position = 0.2f;   // pick / strike position 0..1 (0.5 = centre)
};

class StringModel
{
public:
    void prepare (double sampleRate);
    void noteOn (float freq, int velocity, const PhysParams& p, uint32_t seed) noexcept;
    void setParams (float freq, const PhysParams& p) noexcept; // once per control tick
    float render() noexcept;
    static constexpr float kMinFreq = 16.0f;

private:
    void retune (float freq) noexcept;
    double sr = 48000.0;
    std::vector<float> line;          // circular delay line
    std::vector<float> excitation;    // pre-computed pluck / hammer excitation, injected over one period
    int writePos = 0, mask = 0;
    int dInt = 2;
    float apCoef = 0.0f, apState = 0.0f, apIn = 0.0f;  // Thiran fractional delay
    static constexpr int kDisp = 4;
    std::array<float, kDisp> dispX {}, dispY {};
    float dispCoef = 0.0f;
    float lpCoef = 0.0f, lpState = 0.0f, loopGain = 0.99f;
    int excPos = 0, excLen = 0;
    float bowLevel = 0.0f, bowLp = 0.0f, bowBright = 0.5f;
    float dcX = 0.0f, dcY = 0.0f, dcR = 0.999f, outGain = 1.0f;
    float curFreq = 220.0f;
    PhysParams params;
    Rng rng;
};

class ModalModel
{
public:
    static constexpr int kModes = 32;
    static constexpr int kMaterials = 8;
    void prepare (double sampleRate);
    void noteOn (float freq, int velocity, const PhysParams& p, uint32_t seed) noexcept;
    void setParams (float freq, const PhysParams& p) noexcept;
    float render() noexcept;
    /** Mode-frequency ratios of material m (relative to the first mode); returns the count. */
    static int ratios (int material, const float*& out) noexcept;

private:
    void update (float freq, const PhysParams& p) noexcept;
    double sr = 48000.0;
    int active = 0;
    std::array<float, kModes> a1 {}, a2 {}, gain {}, y1 {}, y2 {};
    int excPos = 0, excLen = 0;
    float excAmp = 1.0f, hammerWidth = 1.0f;
    float bowLevel = 0.0f, bowLp = 0.0f, bowBright = 0.5f;
    float curFreq = 220.0f, outGain = 0.3f;
    PhysParams params;
    Rng rng;
};

class PhaseDistortion
{
public:
    static constexpr int kShapes = 8;
    void prepare (double sampleRate) { sr = sampleRate; }
    void noteOn (float startPhase) noexcept { phase = startPhase; }
    /** One sample; inc = cycles per sample. */
    float render (float inc, int shape, float amount) noexcept;
    /** Stateless (tests, display). minEdge: the shortest fast segment of the phase map as a fraction of the cycle
        (2.5 x the phase increment when playing, so the steep edge spans at least 2.5 samples: band-limiting by
        construction; 0 = the nominal shape). */
    static float shapeValue (float p, int shape, float amount, float minEdge = 0.0f) noexcept;

private:
    double sr = 48000.0;
    float phase = 0.0f;
};

class AdditiveBank
{
public:
    static constexpr int kMaxPartials = 64;
    struct Params { int partials = 32; float tilt = 0.0f, oddEven = 0.0f, stretch = 0.0f, formant = 0.0f, decay = 0.0f, motion = 0.0f; };
    void prepare (double sampleRate);
    void noteOn (float freq, const Params& p, uint32_t seed) noexcept;
    void setParams (float freq, const Params& p) noexcept; // once per control tick (samples = tick length)
    float render() noexcept;

private:
    double sr = 48000.0;
    int count = 0;
    std::array<uint32_t, kMaxPartials> ph {}, inc {};
    std::array<float, kMaxPartials> amp {}, target {}, step {}, decayMul {}, envelope {}, wobble {}, wobbleRate {};
    float freqNow = 220.0f, norm = 1.0f;
    Params prm;
    Rng rng;
    void computeTargets (float freq) noexcept;
};
} // namespace vsx
