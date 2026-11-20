#pragma once

#include "../Core/DspUtils.h"
#include <array>

namespace vsx
{
enum class FilterModel { MgLadder = 0, DiodeLadder, Acid303, MsSallenKey, JpOta, JnOta, P5Cascade, SemSvf, AnalogSvf, Count };
enum class FilterMode { LP24 = 0, LP12, BP, HP, Notch, Morph };

/**
    Analog circuit-modelled filter (one instance per voice, stereo).

    All models use zero-delay-feedback / topology-preserving trapezoidal integration with
    bilinear pre-warping. Nonlinear behaviour is model specific:
      - MG Ladder: transistor-pair tanh on every stage input and state, saturating global feedback,
        passband loss with resonance (adjustable compensation), self-oscillation at k ~ 4.
      - Diode Ladder / Acid 303: coupled RC-ladder network (stages load each other), solved as an
        implicit 4x4 system per sample with diode-pair conductances linearised from the previous
        sample (secant method). Calibrated so k_osc = 17 (diode) / 21.5 (303 stage ratio).
      - MS Sallen-Key: Korg35-style two-pole with nonlinear (clipping) feedback, K up to 2.
      - JP / JN / P5: four-stage OTA cascades (tanh of the input difference per stage) with
        different feedback saturation, compensation, asymmetry and taps.
      - SEM / Analog SVF: TPT state-variable filters with saturating integrators / nonlinear damping.
*/
class AnalogFilter
{
public:
    void prepare (double sampleRate);
    void reset();
    void setModel (int model, int mode);

    /** Per-voice component tolerance: random deviation of stage cutoffs, feedback gain and offsets. */
    void setTolerance (float amount, uint32_t seed);

    /** Control-rate update. numSamples > 0 ramps the cutoff smoothly over the tick. */
    void update (float cutoffHz, float resonance, float drive, float character, int numSamples);

    void process (float& left, float& right) noexcept;

    int getModel() const noexcept { return (int) model; }

private:
    struct Channel
    {
        std::array<float, 8> s {};
        float xPrev = 0.0f;
        float hpz = 0.0f, hpz2 = 0.0f, fbHp = 0.0f, dc = 0.0f;
        float nlState = 0.0f;
    };

    float processChannel (Channel& c, float x, int ch) noexcept;
    float ladder (Channel& c, float x) noexcept;
    float diode (Channel& c, float x, bool acid) noexcept;
    float korg35 (Channel& c, float x) noexcept;
    float otaCascade (Channel& c, float x, int variant) noexcept;
    float svf (Channel& c, float x, bool sem) noexcept;
    float tapMix (float y0, float y1, float y2, float y3, float y4) const noexcept;

    std::array<float, 4> otaG {}, otaInv {}; // per-sample OTA stage coefficients shared by both channels
    double sr = 88200.0;
    FilterModel model = FilterModel::MgLadder;
    FilterMode mode = FilterMode::LP24;

    // smoothed coefficients
    float gCur = 0.1f, gStep = 0.0f;
    float res = 0.0f, k = 0.0f, inGain = 1.0f, outGain = 1.0f, character = 0.5f;
    float cutoff = 1000.0f;
    int junoHpPos = 1;
    float junoHpG = 0.0f;

    // tolerance
    std::array<float, 4> stageTol { 1.0f, 1.0f, 1.0f, 1.0f };
    float kTol = 1.0f, offsetTol = 0.0f;

    std::array<Channel, 2> chans;
    Rng noise;
};

/** Measures the small-signal magnitude response of a filter configuration (used by GUI and tests). */
void measureFilterResponse (int model, int mode, float cutoff, float resonance, float drive, float character,
                            double sampleRate, const float* freqs, float* magsDb, int numFreqs);

} // namespace vsx
