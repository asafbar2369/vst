#pragma once

#include "Effects.h"

namespace vsx
{
/**
    Algorithmic reverb with eight programs (original design).

    HALL / PLATE / ROOM / CHAMBER / AMBIENT and GATED use a 16-line feedback delay network:
      - line lengths spread geometrically over a program-specific range (about 3.2:1 to 4.4:1) and rounded to primes,
        so no two loop lengths share small common factors (the main cause of metallic / "boingy" colouration);
      - an all-pass diffuser inside every loop (echo density builds up within tens of milliseconds);
      - two-band absorption per line (Jot): the low band decays with the Decay time, the high band with Decay times
        an HF ratio set by Damping and the program, so the tail darkens naturally instead of ringing;
      - a fast 16 x 16 Walsh-Hadamard feedback matrix (lossless before absorption);
      - independent smoothed-random modulation of every line (no common LFO, no audible chorus);
      - program-specific early reflections, stereo input diffusion, decorrelated orthogonal output taps.
    GATED keys a hold-and-release gate from the dry input.
    NONLINEAR and REVERSE are shaped, dense multi-tap responses (flat / rising envelope with an abrupt end).
    Everything is allocated in prepare(); process() is real-time safe.
*/
class ReverbEngine
{
public:
    enum Program { Hall = 0, Plate, Room, Chamber, Ambient, Nonlinear, Gated, Reverse, NumPrograms };

    struct Settings
    {
        int program = Hall;
        float size = 0.6f, decay = 3.0f, predelayMs = 20.0f, damping = 0.4f, diffusion = 0.75f, modulation = 0.3f,
              width = 1.0f, lowCut = 120.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, const Settings& s) noexcept;

    static constexpr int kLines = 16;
    static constexpr int kTaps = 48;

    /** Loop lengths (samples) the network is heading for with the given settings; for tests / measurement. */
    std::array<float, kLines> targetLengths (const Settings& s) const;

private:
    struct ProgramDef
    {
        float minMs, maxMs;      // loop-length range at size 0.5
        float erLevel, erMinMs, erMaxMs;
        float loopDiffusion;     // in-loop all-pass coefficient at full Diffusion
        float inputDiffusion;    // input diffuser coefficient scale
        float modMs;             // modulation depth at full Modulation
        float hfFactor;          // extra high-band decay factor (air absorption)
        float highCut;           // output high cut at zero damping
        float gain;              // loudness calibration (equal IR energy at the default settings)
    };
    static const ProgramDef& def (int program);

    void updateNetwork (const Settings& s, const ProgramDef& d) noexcept;
    void updateTaps (const Settings& s) noexcept;
    float processFdn (float inL, float inR, float& outL, float& outR, const ProgramDef& d) noexcept;
    static float allpass (DelayLine& line, float x, int len, float g) noexcept
    {
        const float del = line.readInt (std::max (1, len));
        const float v = x + g * del;
        line.push (flushDenormal (v));
        return del - g * v;
    }

    struct SmoothRandom
    {
        float current = 0.0f, from = 0.0f, to = 0.0f, phase = 1.0f, inc = 0.0f;
        uint32_t state = 1;
        float uniform() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return (float) (state & 0xffffff) / 8388608.0f - 1.0f; }
        float next() noexcept
        {
            phase += inc;
            if (phase >= 1.0f) { phase -= 1.0f; from = to; to = uniform(); }
            const float t = 0.5f - 0.5f * std::cos (kPi * phase);
            return current = from + (to - from) * t;
        }
    };

    double sr = 48000.0;
    int lastProgram = -1;
    float lastTapSize = -1.0f, lastTapDecay = -1.0f;

    std::array<DelayLine, kLines> lines, loopAp;
    std::array<float, kLines> lenCur {}, lenTarget {}, apLen {}, gLow {}, gHigh {};
    std::array<OnePoleLP, kLines> absorb;
    std::array<SmoothRandom, kLines> mod;
    std::array<float, kLines> modDepth {};
    std::array<DelayLine, 8> inDiff;
    std::array<int, 8> inDiffLen {};
    DelayLine preL, preR, erL, erR, tapL, tapR;
    std::array<float, 14> erTime {}, erGain {};
    std::array<float, kTaps> tapTimeL {}, tapTimeR {}, tapGain {};
    std::array<DelayLine, 4> tapDiff;
    std::array<Biquad, 2> lowCutF, highCutF;
    OnePoleLP erTone[2];
    float lenSmooth = 0.001f;
    // gate
    float gateEnv = 0.0f, gateGain = 0.0f, gateHold = 0.0f;
};

} // namespace vsx
