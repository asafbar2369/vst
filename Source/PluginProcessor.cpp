#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float sqrt8inv = 0.3535533905932737622f;

float dbToGain (float db) noexcept
{
    if (! std::isfinite (db)) return 1.0f;
    return std::pow (10.0f, juce::jlimit (-96.0f, 24.0f, db) / 20.0f);
}

float finiteSample (float x) noexcept
{
    // A single NaN/Inf from a host or upstream plug-in must not poison the
    // delay network for the remainder of the session.
    return std::isfinite (x) ? juce::jlimit (-16.0f, 16.0f, x) : 0.0f;
}

// Approximation of the original 12-bit ADC plus 2-bit gain-ranging front end.
// The original encoder chose gain x1/x2/x4/x8 and stored a 16-bit linear word.
float gainRangedAdc (float x) noexcept
{
    if (! std::isfinite (x)) return 0.0f;
    const float a = std::abs (x);
    const float gain = a < 0.125f ? 8.0f : (a < 0.25f ? 4.0f : (a < 0.5f ? 2.0f : 1.0f));
    const float scaled = juce::jlimit (-0.9995f, 0.9995f, x * gain);
    const float twelveBit = std::round (scaled * 2047.0f) / 2047.0f;
    return twelveBit / gain;
}

// Q1.27-style rounding to mimic the documented 28-bit arithmetic word path.
float quantise28 (float x) noexcept
{
    constexpr double scale = 134217727.0;
    return (float) (std::round (juce::jlimit (-1.0f, 1.0f, x) * scale) / scale);
}

// A bounded circular delay with fractional-delay reads. All memory is allocated
// in prepare(), never in processBlock().
class RingDelay
{
public:
    void prepare (int capacity)
    {
        data.assign ((size_t) juce::jmax (capacity, 8), 0.0f);
        writePos = 0;
    }
    void clear() noexcept
    {
        std::fill (data.begin(), data.end(), 0.0f);
        writePos = 0;
    }
    float readSamples (float delaySamples) const noexcept
    {
        if (data.empty()) return 0.0f;
        const float maxDelay = (float) data.size() - 2.0f;
        delaySamples = juce::jlimit (1.0f, maxDelay, delaySamples);
        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) data.size();
        const auto i0 = (int) readPos;
        const auto i1 = (i0 + 1) % (int) data.size();
        const float frac = readPos - (float) i0;
        return data[(size_t) i0] + frac * (data[(size_t) i1] - data[(size_t) i0]);
    }
    float readMs (float ms, double sr) const noexcept { return readSamples (ms * 0.001f * (float) sr); }
    void write (float x) noexcept
    {
        if (data.empty()) return;
        data[(size_t) writePos] = x;
        if (++writePos >= (int) data.size()) writePos = 0;
    }
private:
    std::vector<float> data;
    int writePos = 0;
};

struct EarlyTap { float ms; float gain; float pan; };

// Independent plate-style network: four serial input diffusers feed two
// cross-coupled, damped delay loops. This is a new implementation based on
// established Schroeder/Dattorro plate topology, not recovered AMS firmware.
class PlateTank
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : diffusers) d.prepare ((int) (sr * 0.025) + 8);
        for (auto& d : tank) d.prepare ((int) (sr * 0.14) + 8);
        clear();
    }

    void clear() noexcept
    {
        for (auto& d : diffusers) d.clear();
        for (auto& d : tank) d.clear();
        lowState.fill (0.0f);
        highState.fill (0.0f);
    }

    void process (float input, float decay, int high, int low,
                  float& left, float& right) noexcept
    {
        // Four serial all-pass stages create the high initial diffusion expected
        // from Plate A1 and quickly smooth transient/percussive input.
        static constexpr float diffuserMs[4] = { 4.7f, 6.3f, 8.1f, 10.9f };
        static constexpr float diffuserGain[4] = { 0.72f, 0.70f, 0.66f, 0.62f };
        float x = input;
        for (int i = 0; i < 4; ++i)
        {
            auto& line = diffusers[(size_t) i];
            const float delayed = line.readMs (diffuserMs[i], sr);
            const float y = delayed - diffuserGain[i] * x;
            line.write (x + diffuserGain[i] * y);
            x = y;
        }

        // Four mutually coupled delay lines make a denser, more even tail than
        // the former two-delay approximation. The orthogonal mixing matrix
        // redistributes energy without simply summing identical feedback paths.
        static constexpr float tankMs[4] = { 29.7f, 33.1f, 37.1f, 41.1f };
        static constexpr float outL[4] = { 0.5f, 0.5f, -0.5f, -0.5f };
        static constexpr float outR[4] = { 0.5f, -0.5f, 0.5f, -0.5f };
        std::array<float, 4> v {};
        for (int i = 0; i < 4; ++i)
            v[(size_t) i] = tank[(size_t) i].readMs (tankMs[i], sr);

        const float hf = highCutoff (high);
        const float hfAlpha = 1.0f - std::exp (-2.0f * pi * hf / (float) sr);
        const float lfAlpha = 1.0f - std::exp (-2.0f * pi * 350.0f / (float) sr);
        const float rt60 = 0.15f + juce::jlimit (0.0f, 9.9f, decay) * 0.90f;
        const float baseFeedback = juce::jlimit (0.0f, 0.9995f,
            std::pow (10.0f, -3.0f * 0.041f / juce::jmax (0.1f, rt60)));
        const float lowTimeRatio = std::pow (4.0f, (float) juce::jlimit (-9, 9, low) / 9.0f);
        const float lowGain = std::pow (baseFeedback, 1.0f / lowTimeRatio);
        const float highTimeRatio = std::pow (10.0f, (float) juce::jlimit (-9, 0, high) / 9.0f);
        const float highGain = std::pow (baseFeedback, 1.0f / highTimeRatio);

        std::array<float, 4> shaped {};
        for (int i = 0; i < 4; ++i)
        {
            const float raw = v[(size_t) i];
            lowState[(size_t) i] += lfAlpha * (raw - lowState[(size_t) i]);
            highState[(size_t) i] += hfAlpha * (raw - highState[(size_t) i]);
            const float lowBand = lowState[(size_t) i];
            const float midBand = highState[(size_t) i] - lowBand;
            const float highBand = raw - highState[(size_t) i];
            shaped[(size_t) i] = lowBand * lowGain + midBand * baseFeedback + highBand * highGain;
        }
        for (int i = 0; i < 4; ++i)
        {
            // 4x4 normalized Hadamard feedback transform after per-line damping.
            float mixed = 0.0f;
            for (int j = 0; j < 4; ++j)
            {
                const unsigned parity = (unsigned) (i & j);
                const unsigned bitParity = (parity ^ (parity >> 1) ^ (parity >> 2)) & 1u;
                mixed += (bitParity == 0u ? 1.0f : -1.0f) * shaped[(size_t) j] * 0.5f;
            }
            const float injection = x * (((i & 1) == 0) ? 0.30f : -0.30f);
            tank[(size_t) i].write (juce::jlimit (-1.0f, 1.0f, injection + mixed));
        }

        left = right = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            left += v[(size_t) i] * outL[i];
            right += v[(size_t) i] * outR[i];
        }
        left *= 0.95f;
        right *= 0.95f;
    }

private:
    static float highCutoff (int setting) noexcept
    {
        static constexpr float cuts[10] = { 18000, 15000, 10000, 8000, 6500, 5000, 3500, 2000, 1500, 1000 };
        return cuts[(size_t) -juce::jlimit (-9, 0, setting)];
    }
    double sr = 44100.0;
    std::array<RingDelay, 4> diffusers;
    std::array<RingDelay, 4> tank;
    std::array<float, 4> lowState {};
    std::array<float, 4> highState {};
};}

class RMX16AudioProcessor::Engine
{
public:
    void prepare (double newRate)
    {
        sr = juce::jlimit (22050.0, 192000.0, newRate);
        const int cap = (int) std::ceil (sr * 1.35) + 16;
        early.prepare (cap);
        predelayLine.prepare (cap);
        chorus.prepare ((int) std::ceil (sr * 0.32) + 16);
        plate.prepare (sr);
        echoL.prepare (cap);
        echoR.prepare (cap);
        for (auto& d : fdn) d.prepare ((int) std::ceil (sr * 0.18) + 16);
        inputLP = outputLP_L = outputLP_R = 0.0f;
        delaySmoothingAlpha = 1.0f - std::exp (-1.0f / (0.020f * (float) sr));
        delayControlsInitialised = false;
        const float inputBandwidth = juce::jmin (18000.0f, (float) sr * 0.45f);
        bandAlpha = 1.0f - std::exp (-2.0f * pi * inputBandwidth / (float) sr);
        reset();
        updateDelayLengths (0);
    }

    void reset() noexcept
    {
        early.clear(); predelayLine.clear(); chorus.clear(); echoL.clear(); echoR.clear();
        plate.clear();
        for (auto& d : fdn) d.clear();
        dampState.fill (0.0f);
        lowState.fill (0.0f);
        chorusLP.fill (0.0f);
        echoLowState.fill (0.0f);
        echoHighState.fill (0.0f);
        chorusRandom.fill (0.0f);
        chorusRandomTarget.fill (0.0f);
        chorusRandomCounter.fill (0);
        chorusRng = { 0x13579bdfu, 0x2468ace1u, 0x9e3779b9u, 0x7f4a7c15u, 0xa341316cu };
        inputLP = outputLP_L = outputLP_R = 0.0f;
        smoothedPredelay = smoothedEchoBPredelay = 0.0f;
        delayControlsInitialised = false;
        gateEnv = 0.0f;
        gateHold = 0;
        gateInputEnv = gatePrevInputEnv = 0.0f;
        gateReleaseCoeff = std::exp (-1.0f / (0.004f * (float) sr));
        reverseEnv = 0.0f;
        reversePhase = 0;
        reversePrevInput = 0.0f;
        reverseInputEnv = 0.0f;
        lfoPhase.fill (0.0f);
        prevProgram = -1;
        // prepareToPlay() may be called again at a different host sample rate.
        // Invalidate every coefficient cache so updateControlCache() recomputes
        // all sample-rate-dependent filters and feedback gains on the next block.
        cacheProgram = -1;
        cacheDecay = cacheEchoBDecay = -1.0f;
        cacheHi = cacheLo = cacheEchoBHi = cacheEchoBLo = 999;
    }

    void process (float input, int program, float predelayMs, float decay, int hi, int lo,
                  float echoBPredelay, float echoBDecay, int echoBHi, int echoBLo,
                  float& left, float& right) noexcept
    {
        if (program != prevProgram)
        {
            updateDelayLengths (program);
            prevProgram = program;
        }

        // Predelay bounds follow the Issue 3 manual where an explicit value is given.
        // Hall C1 is conservatively capped at 200 ms (later RMX16-family documentation);
        // the Issue 3 original manual does not print a numeric maximum for C1.
        // Echo B is independently limited to 400 ms below.
        const float maxPd = (program == 0 || program == 2 || program == 4 ? 200.0f :
                             (program == 5 ? 50.0f : (program == 8 ? 100.0f :
                             (program == 6 ? 1200.0f : 300.0f))));
        const float targetPd = juce::jlimit (0.0f, maxPd, predelayMs);
        const float targetEchoBPredelay = juce::jlimit (0.0f, 400.0f, echoBPredelay);
        if (! delayControlsInitialised)
        {
            smoothedPredelay = targetPd;
            smoothedEchoBPredelay = targetEchoBPredelay;
            delayControlsInitialised = true;
        }
        else
        {
            smoothedPredelay += delaySmoothingAlpha * (targetPd - smoothedPredelay);
            smoothedEchoBPredelay += delaySmoothingAlpha * (targetEchoBPredelay - smoothedEchoBPredelay);
        }
        const float pd = smoothedPredelay;
        const float echoBPd = smoothedEchoBPredelay;
        const float rt = juce::jlimit (0.0f, 9.9f, decay);
        updateControlCache (program, rt, hi, lo, echoBDecay, echoBHi, echoBLo);
        inputLP += bandAlpha * (input - inputLP);
        const float x = inputLP;
        // The original predelay offsets the wet excitation before the reverb
        // network. The early-reflection line below applies the same offset to
        // its tap reads. Chorus and Echo use predelay as their native delay control.
        const float wetInput = (program == 5 || program == 6 || pd < 0.5f)
                                 ? x : predelayLine.readMs (pd, sr);
        predelayLine.write (x);
        // The RMX16 manual specifies a deliberately different early-reflection
        // signature for each program. These taps are independent estimates, not
        // recovered coefficients from the proprietary EPROM algorithms.
        float erL = 0.0f, erR = 0.0f;
        const auto taps = getEarlyTaps (program);
        for (const auto& tap : taps)
        {
            const float s = early.readMs (tap.ms + pd, sr) * tap.gain;
            erL += s * (tap.pan <= 0.0f ? 1.0f : 1.0f - tap.pan);
            erR += s * (tap.pan >= 0.0f ? 1.0f : 1.0f + tap.pan);
        }

        float wetL = 0.0f, wetR = 0.0f;
        if (program == 5) // five-voice chorus
        {
            // The manual specifies five panned voices, with up to 50 ms
            // separation between adjacent voices (four gaps = 200 ms total).
            // The decay control changes the smooth random delay/pitch wander.
            const float wanderDepth = 0.10f + (rt / 9.9f) * 4.5f;
            static constexpr float offsets[5] = { 0.0f, 0.25f, 0.50f, 0.75f, 1.0f };
            static constexpr float rates[5] = { 0.071f, 0.093f, 0.117f, 0.143f, 0.179f };
            static constexpr float pans[5] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
            float voices[5] {};
            const float filterA = chorusFilterAlpha;
            for (int i = 0; i < 5; ++i)
            {
                lfoPhase[(size_t) i] += 2.0f * pi * rates[i] / (float) sr;
                if (lfoPhase[(size_t) i] > 2.0f * pi) lfoPhase[(size_t) i] -= 2.0f * pi;
                if (chorusRandomCounter[(size_t) i] <= 0)
                {
                    auto& rng = chorusRng[(size_t) i];
                    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                    chorusRandomTarget[(size_t) i] = ((float) (rng & 0xffffu) / 32767.5f) - 1.0f;
                    chorusRandomCounter[(size_t) i] = (int) (sr * (0.11f + 0.055f * (float) i));
                }
                chorusRandom[(size_t) i] += (chorusRandomTarget[(size_t) i] - chorusRandom[(size_t) i]) /
                                            (float) juce::jmax (1, chorusRandomCounter[(size_t) i]);
                --chorusRandomCounter[(size_t) i];
                const float slowWander = chorusRandom[(size_t) i]
                                        + 0.12f * std::sin (lfoPhase[(size_t) i]);
                const float delay = juce::jlimit (1.0f, 300.0f,
                    pd * 4.0f * offsets[i] + wanderDepth * slowWander);
                voices[i] = chorus.readMs (delay, sr);
                chorusLP[(size_t) i] += filterA * (voices[i] - chorusLP[(size_t) i]);
                voices[i] = chorusLP[(size_t) i];
                const float pan = pans[i];
                wetL += voices[i] * (pan <= 0.0f ? 1.0f : 1.0f - pan);
                wetR += voices[i] * (pan >= 0.0f ? 1.0f : 1.0f + pan);
            }
            wetL *= 0.29f; wetR *= 0.29f;
            chorus.write (x);
        }
        else if (program == 3) // dedicated plate diffusion and cross-coupled tank
        {
            plate.process (wetInput, rt, hi, lo, wetL, wetR);
        }
        else if (program == 6) // two independently timed echo returns
        {
            const float dA = juce::jlimit (1.0f, 1200.0f, pd);
            const float dB = juce::jlimit (1.0f, 400.0f, echoBPd);
            const float a = echoL.readMs (dA, sr);
            const float b = echoR.readMs (dB, sr);
            // Filtering belongs in each regenerative path, not just on the
            // audible output, so every repeat progressively changes spectrum.
            const float fbA = shapeEchoFeedback (a, 0);
            const float fbB = shapeEchoFeedback (b, 1);
            echoL.write (quantise28 (juce::jlimit (-1.0f, 1.0f, x + fbA)));
            echoR.write (quantise28 (juce::jlimit (-1.0f, 1.0f, x + fbB)));
            wetL = a;
            wetR = b;
        }
        else
        {
            // Eight-line feedback delay network. A normalized Hadamard matrix
            // produces a dense, energy-preserving feedback field. Original AMS
            // code/coefficients are not public; program-specific delay lengths,
            // excitation, reflection patterns and decay responses are modeled.
            std::array<float, 8> v {};
            for (int i = 0; i < 8; ++i)
                v[(size_t) i] = fdn[(size_t) i].readSamples (lineDelaySamples[(size_t) i]);

            std::array<float, 8> h {};
            for (int row = 0; row < 8; ++row)
            {
                float sum = 0.0f;
                for (int col = 0; col < 8; ++col)
                {
                    unsigned bits = (unsigned) (row & col);
                    bits ^= (bits >> 1); bits ^= (bits >> 2);
                    sum += (((bits & 1u) == 0u) ? 1.0f : -1.0f) * v[(size_t) col];
                }
                h[(size_t) row] = quantise28 (sum * sqrt8inv);
            }

            const float inputScale = (program == 3 ? 0.11f : (program == 2 ? 0.09f : 0.12f));
            for (int i = 0; i < 8; ++i)
            {
                const float raw = h[(size_t) i];
                dampState[(size_t) i] += fdnHiAlpha * (raw - dampState[(size_t) i]); // <= HF half-time corner
                lowState[(size_t) i] += fdnLowAlpha * (raw - lowState[(size_t) i]); // 350 Hz LF time control
                const float lowBand = lowState[(size_t) i];
                const float midBand = dampState[(size_t) i] - lowBand;
                const float highBand = raw - dampState[(size_t) i];
                // The manual describes the selected HF corner as the point at
                // which decay is half the nominal time. The cache stores the
                // three band-specific gains for this delay line.
                const float shaped = lowBand * fdnLowFeedback[(size_t) i]
                                   + midBand * fdnMidFeedback[(size_t) i]
                                   + highBand * fdnHighFeedback[(size_t) i];
                const float injection = wetInput * inputScale * (((i & 1) == 0) ? 1.0f : -0.83f);
                fdn[(size_t) i].write (quantise28 (juce::jlimit (-1.0f, 1.0f, shaped + injection)));
            }

            static constexpr float outL[8] = { 1, 1, -1, -1, 1, -1, 1, -1 };
            static constexpr float outR[8] = { 1, -1, 1, -1, -1, 1, 1, -1 };
            for (int i = 0; i < 8; ++i)
            {
                wetL += v[(size_t) i] * outL[i];
                wetR += v[(size_t) i] * outR[i];
            }
            wetL *= 0.24f; wetR *= 0.24f;
            float earlyGain = 0.18f;
            if (program == 2) earlyGain = 0.48f; // Hall C1: pronounced initial reflections
            if (program == 4) earlyGain = 0.10f; // Hall B3: lower-energy early reflections
            if (program == 1) earlyGain = 0.23f;
            if (program == 3) earlyGain = 0.08f; // Plate: dense tail dominates
            if (program == 7) earlyGain = 0.16f;
            if (program == 8) earlyGain = 0.20f;
            wetL += erL * earlyGain;
            wetR += erR * earlyGain;

            if (program == 7) // Nonlin 2: nearly level sustain, then an abrupt decay.
            {
                // Trigger once per input event, not on every non-zero sample. The
                // previous implementation continually reloaded gateHold during
                // sustained notes, preventing the documented drop-out entirely.
                const float magnitude = std::abs (wetInput);
                const float attack = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
                const float release = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
                const float coeff = magnitude > gateInputEnv ? attack : release;
                gateInputEnv += coeff * (magnitude - gateInputEnv);
                if (gateInputEnv > 0.0015f && gatePrevInputEnv <= 0.0015f)
                {
                    gateHold = (int64_t) (sr * (0.04f + (rt / 9.9f) * 0.95f));
                    gateEnv = 1.0f;
                }
                gatePrevInputEnv = gateInputEnv;
                if (gateHold > 0)
                    --gateHold;
                else
                    gateEnv *= gateReleaseCoeff;
                wetL *= gateEnv;
                wetR *= gateEnv;
            }
            else if (program == 8) // Reverse 1: reverse-style build-up envelope followed by a hard stop.
            {
                // Detect note/transient onsets with an envelope follower rather than
                // the raw waveform. Triggering on raw magnitude caused every zero
                // crossing of a sustained tone to restart the build-up.
                const float inputMagnitude = std::abs (wetInput);
                const float detectorCoeff = inputMagnitude > reverseInputEnv
                    ? (1.0f - std::exp (-1.0f / ((float) sr * 0.005f)))
                    : (1.0f - std::exp (-1.0f / ((float) sr * 0.080f)));
                reverseInputEnv += detectorCoeff * (inputMagnitude - reverseInputEnv);
                if (reverseInputEnv > 0.0015f && reversePrevInput <= 0.0015f)
                {
                    reversePhase = 0;
                    reverseEnv = 0.0f;
                }
                reversePrevInput = reverseInputEnv;
                ++reversePhase;
                const float riseSamples = (float) sr * (0.18f + 0.72f * (rt / 9.9f));
                if ((float) reversePhase < riseSamples)
                    reverseEnv = juce::jmin (1.0f, (float) reversePhase / juce::jmax (1.0f, riseSamples));
                else
                    reverseEnv = 0.0f;
                wetL *= reverseEnv;
                wetR *= reverseEnv;
            }
        }

        early.write (x);
        if (program == 1) // Room A1's conspicuous room coloration / standing-wave character
        {
            lfoPhase[6] += 2.0f * pi * 0.07f / (float) sr;
            const float colour = std::sin (lfoPhase[6] + 0.73f) * 0.025f;
            wetL *= 1.0f + colour;
            wetR *= 1.0f - colour;
        }
        outputLP_L += bandAlpha * (wetL - outputLP_L);
        outputLP_R += bandAlpha * (wetR - outputLP_R);
        left = outputLP_L;
        right = outputLP_R;
    }

private:
    double sr = 44100.0;
    RingDelay early, predelayLine, chorus, echoL, echoR;
    PlateTank plate;
    std::array<RingDelay, 8> fdn;
    std::array<float, 8> lineDelaySamples {};
    std::array<float, 8> dampState {}, lowState {}, lfoPhase {};
    std::array<float, 2> echoLowState {}, echoHighState {};
    int prevProgram = -1;
    int64_t gateHold = 0, reversePhase = 0;
    float gateEnv = 0.0f, gateInputEnv = 0.0f, gatePrevInputEnv = 0.0f;
    float gateReleaseCoeff = 0.99f;
    float reverseEnv = 0.0f, reversePrevInput = 0.0f, reverseInputEnv = 0.0f;
    float inputLP = 0.0f, outputLP_L = 0.0f, outputLP_R = 0.0f, bandAlpha = 0.5f;
    float smoothedPredelay = 0.0f, smoothedEchoBPredelay = 0.0f, delaySmoothingAlpha = 0.01f;
    bool delayControlsInitialised = false;
    std::array<float, 5> chorusLP {};
    std::array<float, 5> chorusRandom {}, chorusRandomTarget {};
    std::array<int, 5> chorusRandomCounter {};
    std::array<uint32_t, 5> chorusRng {};
    std::array<float, 8> fdnLowFeedback {}, fdnMidFeedback {}, fdnHighFeedback {};
    std::array<float, 2> echoLowFeedback {}, echoMidFeedback {}, echoHighFeedback {}, echoHiAlpha {};
    float fdnHiAlpha = 0.5f, fdnLowAlpha = 0.05f, chorusFilterAlpha = 0.5f;
    float cacheDecay = -1.0f, cacheEchoBDecay = -1.0f;
    int cacheProgram = -1, cacheHi = 999, cacheLo = 999, cacheEchoBHi = 999, cacheEchoBLo = 999;

    void updateDelayLengths (int program) noexcept
    {
        static constexpr float ambience[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 61.7f, 71.9f, 79.3f };
        static constexpr float room[8]     = { 13.1f, 19.7f, 23.9f, 31.1f, 37.7f, 43.3f, 53.9f, 67.1f };
        static constexpr float hall[8]     = { 31.3f, 37.9f, 43.1f, 47.9f, 59.3f, 67.7f, 73.9f, 83.9f };
        static constexpr float plateDelays[8] = { 17.3f, 19.9f, 23.3f, 26.9f, 29.9f, 32.3f, 35.9f, 39.7f };
        const float* src = ambience;
        if (program == 1) src = room;
        if (program == 2 || program == 4) src = hall;
        if (program == 3) src = plateDelays;
        if (program == 7) src = plateDelays;
        if (program == 8) src = hall;
        for (int i = 0; i < 8; ++i) lineDelaySamples[(size_t) i] = src[i] * 0.001f * (float) sr;
    }

    static float highCutoffForSetting (int setting) noexcept
    {
        static constexpr float cuts[10] = { 18000, 15000, 10000, 8000, 6500, 5000, 3500, 2000, 1500, 1000 };
        const int s = juce::jlimit (-9, 0, setting);
        return juce::jmin (18000.0f, cuts[(size_t) -s]);
    }

    static std::array<EarlyTap, 12> getEarlyTaps (int program) noexcept
    {
        static constexpr std::array<EarlyTap, 12> ambience = {{{ 7, .18f, -.7f }, { 13, -.14f, .5f }, { 21, .13f, -.2f }, { 29, .11f, .8f }, { 37, -.09f, -.8f }, { 49, .08f, .4f }, { 61, .07f, -.4f }, { 73, -.06f, .9f }, { 91, .05f, -.9f }, { 117, .04f, .2f }, { 143, -.035f, -.3f }, { 177, .03f, .6f } }};
        static constexpr std::array<EarlyTap, 12> room = {{{ 5, .27f, -.8f }, { 11, -.22f, .7f }, { 17, .19f, -.5f }, { 23, -.17f, .9f }, { 31, .15f, -.9f }, { 41, -.12f, .4f }, { 53, .10f, -.3f }, { 67, -.09f, .8f }, { 83, .07f, -.6f }, { 101, -.06f, .5f }, { 127, .05f, -.2f }, { 151, -.04f, .2f } }};
        static constexpr std::array<EarlyTap, 12> hall = {{{ 12, .25f, -.8f }, { 19, -.21f, .8f }, { 28, .18f, -.6f }, { 39, .16f, .7f }, { 52, -.14f, -.5f }, { 68, .12f, .5f }, { 87, -.10f, -.4f }, { 109, .08f, .4f }, { 133, -.07f, -.3f }, { 159, .06f, .3f }, { 187, -.05f, -.2f }, { 211, .04f, .2f } }};
        static constexpr std::array<EarlyTap, 12> plate = {{{ 3, .12f, -.5f }, { 7, -.11f, .5f }, { 12, .10f, -.4f }, { 17, -.09f, .4f }, { 23, .08f, -.3f }, { 29, -.075f, .3f }, { 37, .07f, -.2f }, { 43, -.065f, .2f }, { 51, .06f, -.1f }, { 59, -.055f, .1f }, { 67, .05f, -.05f }, { 73, -.045f, .05f } }};
        if (program == 1) return room;
        if (program == 2 || program == 4) return hall;
        if (program == 3) return plate;
        return ambience;
    }

    float shapeEchoFeedback (float x, int channel) noexcept
    {
        const size_t ch = (size_t) juce::jlimit (0, 1, channel);
        echoHighState[ch] += echoHiAlpha[ch] * (x - echoHighState[ch]);
        echoLowState[ch] += fdnLowAlpha * (x - echoLowState[ch]);
        const float low = echoLowState[ch];
        const float mid = echoHighState[ch] - low;
        const float high = x - echoHighState[ch];
        return low * echoLowFeedback[ch] + mid * echoMidFeedback[ch] + high * echoHighFeedback[ch];
    }

    void updateControlCache (int program, float decay, int hi, int lo,
                             float echoBDecay, int echoBHi, int echoBLo) noexcept
    {
        if (program == cacheProgram && decay == cacheDecay && hi == cacheHi && lo == cacheLo
            && echoBDecay == cacheEchoBDecay && echoBHi == cacheEchoBHi && echoBLo == cacheEchoBLo)
            return;

        cacheProgram = program; cacheDecay = decay; cacheHi = hi; cacheLo = lo;
        cacheEchoBDecay = echoBDecay; cacheEchoBHi = echoBHi; cacheEchoBLo = echoBLo;

        const int effectiveHi = (program == 8 ? 0 : hi); // Reverse 1 has no documented decay-filter control.
        const int effectiveLow = (program == 1 || program == 5 || program == 7 || program == 8) ? 0 : lo;
        const float hiCut = highCutoffForSetting (effectiveHi);
        fdnHiAlpha = 1.0f - std::exp (-2.0f * pi * hiCut / (float) sr);
        fdnLowAlpha = 1.0f - std::exp (-2.0f * pi * 350.0f / (float) sr);
        chorusFilterAlpha = 1.0f - std::exp (-2.0f * pi * highCutoffForSetting (hi) / (float) sr);

        float rt60 = 0.1f + (decay / 9.9f) * 8.7f;
        if (program == 1) rt60 = 0.15f + (decay / 9.9f) * 9.7f;
        if (program == 2) rt60 = 0.08f + (decay / 9.9f) * 3.5f;
        if (program == 3) rt60 = 0.15f + (decay / 9.9f) * 9.0f;
        if (program == 4) rt60 = 0.10f + (decay / 9.9f) * 6.0f;
        if (program == 7) rt60 = 0.10f + (decay / 9.9f) * 1.2f;
        if (program == 8) rt60 = 0.10f + (decay / 9.9f) * 2.0f;

        const bool freeze = (program == 0 && decay >= 9.85f && hi == 0 && lo == 0);
        const float lowDecayRatio = std::pow (4.0f, (float) effectiveLow / 9.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float delaySeconds = lineDelaySamples[(size_t) i] / (float) sr;
            const float base = freeze ? 0.99996f :
                juce::jlimit (0.0f, 0.99997f, std::pow (10.0f, -3.0f * delaySeconds / juce::jmax (0.1f, rt60)));
            const float low = freeze ? 0.99996f :
                juce::jlimit (0.0f, 0.99997f, std::pow (10.0f, -3.0f * delaySeconds /
                                                          juce::jmax (0.1f, rt60 * lowDecayRatio)));
            fdnMidFeedback[(size_t) i] = base;
            fdnLowFeedback[(size_t) i] = low;
            fdnHighFeedback[(size_t) i] = (freeze || effectiveHi == 0) ? base : base * base;
        }

        const float feedback[2] = {
            0.93f * std::pow (juce::jlimit (0.0f, 1.0f, decay / 9.9f), 1.25f),
            0.93f * std::pow (juce::jlimit (0.0f, 1.0f, echoBDecay / 9.9f), 1.25f)
        };
        const int echoHi[2] = { hi, echoBHi };
        const int echoLo[2] = { lo, echoBLo };
        for (int ch = 0; ch < 2; ++ch)
        {
            echoHiAlpha[(size_t) ch] = 1.0f - std::exp (-2.0f * pi * highCutoffForSetting (echoHi[ch]) / (float) sr);
            const float ratio = std::pow (4.0f, (float) juce::jlimit (-9, 9, echoLo[ch]) / 9.0f);
            echoLowFeedback[(size_t) ch] = std::pow (feedback[ch], 1.0f / ratio);
            echoMidFeedback[(size_t) ch] = feedback[ch];
            echoHighFeedback[(size_t) ch] = echoHi[ch] == 0 ? feedback[ch] : feedback[ch] * feedback[ch];
        }
    }
};

const juce::StringArray RMX16AudioProcessor::programNames = {
    "1  AMBIENCE", "2  ROOM A1", "3  HALL C1", "4  PLATE A1", "5  HALL B3",
    "6  CHORUS", "7  ECHO", "8  NONLIN 2", "9  REVERSE 1"
};

juce::AudioProcessorValueTreeState::ParameterLayout RMX16AudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using API = juce::AudioParameterInt;
    using APC = juce::AudioParameterChoice;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<APC> ("program", "Program", programNames, 0));
    p.push_back (std::make_unique<APF> ("predelay", "Pre-delay", juce::NormalisableRange<float> (0.0f, 1200.0f, 1.0f), 40.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<APF> ("decay", "Decay Time", juce::NormalisableRange<float> (0.0f, 9.9f, 0.1f), 2.4f, juce::AudioParameterFloatAttributes().withLabel ("s")));
    p.push_back (std::make_unique<API> ("high", "High Filter", -9, 0, 0));
    p.push_back (std::make_unique<API> ("low", "Low Filter", -9, 9, 0));
    p.push_back (std::make_unique<APF> ("input", "Input Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<APF> ("output", "Output Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<APF> ("mix", "Wet / Dry", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    // Echo B has its own time, regeneration and decay filtering, just as the
    // original Echo A/B operating mode allowed the two outputs to be programmed separately.
    p.push_back (std::make_unique<APF> ("echoBPredelay", "Echo B Pre-delay", juce::NormalisableRange<float> (0.0f, 400.0f, 1.0f), 150.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<APF> ("echoBDecay", "Echo B Feedback", juce::NormalisableRange<float> (0.0f, 9.9f, 0.1f), 2.4f, juce::AudioParameterFloatAttributes().withLabel ("s")));
    p.push_back (std::make_unique<API> ("echoBHigh", "Echo B High Filter", -9, 0, 0));
    p.push_back (std::make_unique<API> ("echoBLow", "Echo B Low Filter", -9, 9, 0));
    return { p.begin(), p.end() };
}

RMX16AudioProcessor::RMX16AudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "RMX16_STATE", createParameterLayout()),
      engine (std::make_unique<Engine>())
{}

void RMX16AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine->prepare (sampleRate);
    inputGainSmooth.reset (sampleRate, 0.015);
    outputGainSmooth.reset (sampleRate, 0.015);
    mixSmooth.reset (sampleRate, 0.015);
    inputGainSmooth.setCurrentAndTargetValue (dbToGain (parameters.getRawParameterValue ("input")->load()));
    outputGainSmooth.setCurrentAndTargetValue (dbToGain (parameters.getRawParameterValue ("output")->load()));
    mixSmooth.setCurrentAndTargetValue (parameters.getRawParameterValue ("mix")->load() * 0.01f);
}
void RMX16AudioProcessor::releaseResources() { engine->reset(); }

bool RMX16AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void RMX16AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    // A zero-length block needs no work. For a temporarily invalid host layout,
    // clear any available output instead of returning stale samples to the host.
    if (n <= 0)
        return;
    if (buffer.getNumChannels() < 2 || engine == nullptr)
    {
        buffer.clear();
        return;
    }
    const int inCh = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch) buffer.clear (ch, 0, n);

    const int program = (int) parameters.getRawParameterValue ("program")->load();
    const float pd = parameters.getRawParameterValue ("predelay")->load();
    const float decay = parameters.getRawParameterValue ("decay")->load();
    const int hi = (int) parameters.getRawParameterValue ("high")->load();
    const int lo = (int) parameters.getRawParameterValue ("low")->load();
    inputGainSmooth.setTargetValue (dbToGain (parameters.getRawParameterValue ("input")->load()));
    outputGainSmooth.setTargetValue (dbToGain (parameters.getRawParameterValue ("output")->load()));
    mixSmooth.setTargetValue (parameters.getRawParameterValue ("mix")->load() * 0.01f);
    const float echoBPredelay = parameters.getRawParameterValue ("echoBPredelay")->load();
    const float echoBDecay = parameters.getRawParameterValue ("echoBDecay")->load();
    const int echoBHi = (int) parameters.getRawParameterValue ("echoBHigh")->load();
    const int echoBLo = (int) parameters.getRawParameterValue ("echoBLow")->load();

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = inCh > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float dryL = finiteSample (inL[i]);
        const float dryR = inR != nullptr ? finiteSample (inR[i]) : dryL;
        const float inGain = inputGainSmooth.getNextValue();
        const float outGain = outputGainSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();
        const float mono = (inCh > 1 ? 0.5f * (dryL + dryR) : dryL) * inGain;

        // Approximate original converter constraints: 16-bit fixed-word rounding,
        // 18 kHz bandwidth ceiling and a conservative overload guard. The original
        // converter front end used gain-ranging; this is not a circuit-level model.
        const float quantised = gainRangedAdc (mono);
        float wetL = 0.0f, wetR = 0.0f;
        engine->process (quantised, program, pd, decay, hi, lo, echoBPredelay, echoBDecay, echoBHi, echoBLo, wetL, wetR);
        const float yL = finiteSample ((dryL * (1.0f - mix) + wetL * mix) * outGain);
        const float yR = finiteSample ((dryR * (1.0f - mix) + wetR * mix) * outGain);
        // The original output word is modeled as a signed 16-bit conversion.
        // Clamp only at this final modeled converter boundary.
        left[i] = std::round (juce::jlimit (-1.0f, 1.0f, yL) * 32767.0f) / 32767.0f;
        right[i] = std::round (juce::jlimit (-1.0f, 1.0f, yR) * 32767.0f) / 32767.0f;
    }
}

juce::AudioProcessorEditor* RMX16AudioProcessor::createEditor() { return new RMX16AudioProcessorEditor (*this); }
int RMX16AudioProcessor::getCurrentProgram() { return (int) parameters.getRawParameterValue ("program")->load(); }
void RMX16AudioProcessor::setCurrentProgram (int index) { parameters.getParameter ("program")->setValueNotifyingHost ((float) juce::jlimit (0, 8, index) / 8.0f); }
const juce::String RMX16AudioProcessor::getProgramName (int index) { return programNames[juce::jlimit (0, 8, index)]; }
void RMX16AudioProcessor::changeProgramName (int, const juce::String&) {}
void RMX16AudioProcessor::storeUserPreset()
{
    storedUserPreset = juce::ValueTree (juce::Identifier ("RMX16_USER_PRESET"));
    storedUserPreset.addChild (parameters.copyState(), -1, nullptr);
}

void RMX16AudioProcessor::recallUserPreset()
{
    if (! storedUserPreset.isValid() || storedUserPreset.getNumChildren() != 1)
        return;

    auto restored = storedUserPreset.getChild (0);
    if (! restored.isValid() || ! restored.hasType (parameters.state.getType()))
        return;

    // Restore individual, validated parameters instead of replacing APVTS's
    // ValueTree wholesale. This avoids host crashes on stale/foreign state trees.
    static const char* const ids[] = {
        "program", "predelay", "decay", "high", "low", "input", "output",
        "mix", "echoBPredelay", "echoBDecay", "echoBHigh", "echoBLow"
    };
    for (const auto* id : ids)
    {
        auto* parameter = parameters.getParameter (id);
        if (parameter == nullptr) continue;
        for (int i = 0; i < restored.getNumChildren(); ++i)
        {
            const auto child = restored.getChild (i);
            if (child.isValid() && child.getProperty ("id").toString() == id)
            {
                const auto value = child.getProperty ("value");
                if (! value.isVoid())
                {
                    const double numeric = value.toString().getDoubleValue();
                    if (std::isfinite (numeric))
                    {
                        const auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
                        if (ranged != nullptr)
                            parameter->setValueNotifyingHost (ranged->convertTo0to1 (
                                (float) juce::jlimit ((double) ranged->getNormalisableRange().start,
                                                      (double) ranged->getNormalisableRange().end,
                                                      numeric)));
                    }
                }
                break;
            }
        }
    }
}

bool RMX16AudioProcessor::hasStoredUserPreset() const noexcept
{
    return storedUserPreset.isValid();
}

void RMX16AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    if (storedUserPreset.isValid())
        state.addChild (storedUserPreset.createCopy(), -1, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}
void RMX16AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Hosts may hand a plug-in an empty, truncated, or stale state block.
    // Never pass unvalidated XML/ValueTrees into APVTS: malformed trees can
    // assert during parameter-tree replacement inside the host's wrapper.
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > 1024 * 1024)
        return;

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType().toString()))
        return;

    auto restored = juce::ValueTree::fromXml (*xml);
    if (! restored.isValid() || ! restored.hasType (parameters.state.getType()))
        return;

    // Extract the optional user preset separately. Accept it only if it
    // contains exactly one parameter-state tree with the expected root type.
    auto preset = restored.getChildWithName (juce::Identifier ("RMX16_USER_PRESET"));
    juce::ValueTree safePreset;
    if (preset.isValid() && preset.getNumChildren() == 1)
    {
        auto candidate = preset.getChild (0);
        if (candidate.isValid() && candidate.hasType (parameters.state.getType()))
            safePreset = candidate.createCopy();
    }
    if (preset.isValid())
        restored.removeChild (preset, nullptr);

    // Require every current parameter exactly once, but tolerate extra state
    // nodes so future versions can extend the tree without breaking old hosts.
    static const char* const ids[] = {
        "program", "predelay", "decay", "high", "low", "input", "output",
        "mix", "echoBPredelay", "echoBDecay", "echoBHigh", "echoBLow"
    };

    for (const auto* id : ids)
    {
        int matches = 0;
        for (int i = 0; i < restored.getNumChildren(); ++i)
        {
            const auto child = restored.getChild (i);
            if (! child.isValid() || child.getProperty ("id").toString() != id)
                continue;

            ++matches;
            const auto value = child.getProperty ("value");
            // APVTS stores numeric vars. Reject strings such as "abc", which
            // JUCE may otherwise parse as zero and silently accept.
            const bool numericType = value.isDouble() || value.isInt() || value.isInt64();
            if (! numericType || ! std::isfinite ((double) value))
                return;
        }

        if (matches != 1)
            return;
    }

    // Apply only recognised, finite, range-clamped parameter values.
    // Avoid APVTS::replaceState on host-provided XML: some hosts restore older
    // parameter trees and malformed ValueTrees can destabilise the wrapper.
    for (const auto* id : ids)
    {
        auto* parameter = parameters.getParameter (id);
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        if (ranged == nullptr) continue;
        for (int i = 0; i < restored.getNumChildren(); ++i)
        {
            const auto child = restored.getChild (i);
            if (child.isValid() && child.getProperty ("id").toString() == id)
            {
                const auto value = child.getProperty ("value");
                if (! value.isVoid())
                {
                    const double numeric = value.toString().getDoubleValue();
                    if (std::isfinite (numeric))
                    {
                        const auto& range = ranged->getNormalisableRange();
                        const float bounded = juce::jlimit (range.start, range.end, (float) numeric);
                        parameter->setValueNotifyingHost (ranged->convertTo0to1 (bounded));
                    }
                }
                break;
            }
        }
    }
    storedUserPreset = safePreset;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RMX16AudioProcessor(); }
