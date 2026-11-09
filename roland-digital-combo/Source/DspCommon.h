#pragma once
// Shared, original DSP building blocks. No third-party code.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dc
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;
constexpr int kControlRate = 16; // samples between control (envelope->coefficient) updates

inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

// Exponential time curve shared by both engines: 0 -> 1 ms, 50 -> 100 ms, 100 -> 10 s.
inline float timeFromValue (float v0to100) noexcept { return 0.001f * std::pow (10.0f, clampf (v0to100, 0.0f, 100.0f) * 0.04f); }

// LFO rate curve: 0 -> 0.05 Hz, 50 -> ~0.9 Hz, 100 -> ~15.8 Hz
inline float lfoRateHz (float v0to100) noexcept { return 0.05f * std::pow (10.0f, clampf (v0to100, 0.0f, 100.0f) / 40.0f); }

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// A mono sample with an optional sustain loop [loopStart, loopEnd). Used for the generated
// substitute waves and for waves the user imports from their own WAV/AIFF files.
struct SampleWave
{
    std::vector<float> data;
    bool loop = false;
    int loopStart = 0, loopEnd = 0;   // loopEnd == 0 means "end of data"
    float rootHz = 261.6256f;         // pitch of the recording at its own sample rate
    float rate = 32000.0f;            // sample rate of data

    bool empty() const noexcept { return data.size() < 2; }
    int end() const noexcept { return loopEnd > loopStart && loopEnd <= (int) data.size() ? loopEnd : (int) data.size(); }

    // Linear-interpolated read; advances pos by step. Returns 0 after a one-shot ends.
    float read (float& pos, float step) const noexcept
    {
        const int len = (int) data.size();
        if (len < 2) return 0.0f;
        if (loop)
        {
            const int e = end();
            const int ls = std::min (std::max (0, loopStart), e - 1);
            const int i0 = std::min ((int) pos, e - 1);
            const float f = pos - float (i0);
            const int i1 = i0 + 1 >= e ? ls : i0 + 1;
            const float x = data[(size_t) i0] + f * (data[(size_t) i1] - data[(size_t) i0]);
            pos += step;
            const float span = float (e - ls);
            while (pos >= float (e)) pos -= span;
            return x;
        }
        if (pos >= float (len - 1)) return 0.0f;
        const int i0 = (int) pos;
        const float f = pos - float (i0);
        const float x = data[(size_t) i0] + f * (data[(size_t) i0 + 1] - data[(size_t) i0]);
        pos += step;
        return x;
    }
};

struct Rng
{
    uint32_t state = 0x1234567u;
    float next() noexcept // -1..1
    {
        state = state * 1664525u + 1013904223u;
        return float (int32_t (state)) * (1.0f / 2147483648.0f);
    }
};

// Topology-preserving state-variable filter (trapezoidal integration).
struct Svf
{
    float ic1 = 0, ic2 = 0, k = 2, a1 = 0, a2 = 0, a3 = 0;

    void set (float cutoffHz, float q, float sampleRate) noexcept
    {
        cutoffHz = clampf (cutoffHz, 10.0f, sampleRate * 0.45f);
        const float g = std::tan (kPi * cutoffHz / sampleRate);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0; }
    inline void process (float v0, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float lowpass (float x) noexcept { float l, b, h; process (x, l, b, h); return l; }
};

// RBJ biquad (direct form I) used for tone EQ.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() noexcept { x1 = x2 = y1 = y2 = 0; }
    void setLowShelf (float hz, float gainDb, float sr) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * clampf (hz, 10.0f, sr * 0.45f) / sr;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / 2.0f * std::sqrt (2.0f);
        const float sa = 2.0f * std::sqrt (A) * alpha;
        const float a0 = (A + 1) + (A - 1) * cw + sa;
        b0 = A * ((A + 1) - (A - 1) * cw + sa) / a0;
        b1 = 2 * A * ((A - 1) - (A + 1) * cw) / a0;
        b2 = A * ((A + 1) - (A - 1) * cw - sa) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * cw) / a0;
        a2 = ((A + 1) + (A - 1) * cw - sa) / a0;
    }
    void setHighShelf (float hz, float gainDb, float sr) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * clampf (hz, 10.0f, sr * 0.45f) / sr;
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw / 2.0f * std::sqrt (2.0f);
        const float sa = 2.0f * std::sqrt (A) * alpha;
        const float a0 = (A + 1) - (A - 1) * cw + sa;
        b0 = A * ((A + 1) + (A - 1) * cw + sa) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * cw) / a0;
        b2 = A * ((A + 1) + (A - 1) * cw - sa) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * cw) / a0;
        a2 = ((A + 1) - (A - 1) * cw - sa) / a0;
    }
    void setHighPass (float hz, float q, float sr) noexcept
    {
        const float w = kTwoPi * clampf (hz, 10.0f, sr * 0.45f) / sr;
        const float alpha = std::sin (w) / (2.0f * std::max (0.1f, q));
        const float cw = std::cos (w);
        const float a0 = 1 + alpha;
        b0 = (1 + cw) / 2 / a0; b1 = -(1 + cw) / a0; b2 = b0;
        a1 = -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
    void setPeak (float hz, float q, float gainDb, float sr) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * clampf (hz, 10.0f, sr * 0.45f) / sr;
        const float alpha = std::sin (w) / (2.0f * std::max (0.1f, q));
        const float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0;
        b1 = -2 * std::cos (w) / a0;
        b2 = (1 - alpha * A) / a0;
        a1 = b1;
        a2 = (1 - alpha / A) / a0;
    }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

// One-pole DC blocker (about 5 Hz): the analog outputs of the originals are AC coupled.
struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9993f;
    void prepare (float sr) noexcept { r = 1.0f - kTwoPi * 5.0f / sr; x1 = y1 = 0; }
    void reset() noexcept { x1 = y1 = 0; }
    inline float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

// Multi-segment envelope: up to 4 attack/decay segments, a held sustain, and one release segment.
struct SegmentEnvelope
{
    static constexpr int kMaxSegments = 4;
    float times[kMaxSegments] {};   // seconds
    float levels[kMaxSegments] {};  // target levels; the last used one is the sustain level
    int numSegments = 4;
    float releaseTime = 0.1f;
    float releaseLevel = 0.0f;
    float startLevel = 0.0f;

    float value = 0.0f;
    int stage = -1;        // -1 idle, 0..numSegments-1 attack/decay, numSegments sustain, numSegments+1 release
    float inc = 0.0f;
    float target = 0.0f;
    int remaining = 0;
    float sampleRate = 44100.0f;

    bool isActive() const noexcept { return stage >= 0; }
    bool isReleasing() const noexcept { return stage == numSegments + 1; }

    void noteOn() noexcept
    {
        value = startLevel;
        stage = 0;
        beginSegment (times[0], levels[0]);
    }
    void noteOff() noexcept
    {
        if (stage < 0 || stage == numSegments + 1)
            return;
        stage = numSegments + 1;
        beginSegment (releaseTime, releaseLevel);
    }
    void kill() noexcept { stage = -1; value = 0.0f; }

    // Advances by n samples and returns the value.
    float advance (int n) noexcept
    {
        while (n > 0 && stage >= 0)
        {
            if (stage == numSegments) // sustain: follow sustain level changes smoothly
            {
                value = levels[numSegments - 1];
                return value;
            }
            const int step = std::min (n, remaining);
            value += inc * float (step);
            remaining -= step;
            n -= step;
            if (remaining <= 0)
            {
                value = target;
                if (stage < numSegments - 1)
                {
                    ++stage;
                    beginSegment (times[stage], levels[stage]);
                }
                else if (stage == numSegments - 1)
                {
                    stage = numSegments;
                }
                else // release finished
                {
                    stage = -1;
                    return value;
                }
            }
        }
        return value;
    }

private:
    void beginSegment (float seconds, float level) noexcept
    {
        target = level;
        remaining = std::max (1, int (seconds * sampleRate));
        inc = (target - value) / float (remaining);
    }
};

// Simple LFO: 0 tri, 1 saw, 2 square, 3 random (sample & hold)
struct Lfo
{
    float phase = 0.0f, held = 0.0f;
    int wave = 0;
    Rng rng;

    float advance (float hz, float seconds) noexcept
    {
        phase += hz * seconds;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rng.next();
        }
        switch (wave)
        {
            case 1:  return 2.0f * phase - 1.0f;
            case 2:  return phase < 0.5f ? 1.0f : -1.0f;
            case 3:  return held;
            default: return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
        }
    }
};

// Fractional delay line
struct DelayLine
{
    std::vector<float> buf;
    int write = 0;
    void prepare (int maxSamples) { buf.assign ((size_t) std::max (4, maxSamples), 0.0f); write = 0; }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); write = 0; }
    inline void push (float x) noexcept
    {
        buf[(size_t) write] = x;
        if (++write >= (int) buf.size()) write = 0;
    }
    inline float read (float delaySamples) const noexcept
    {
        const int size = (int) buf.size();
        delaySamples = clampf (delaySamples, 1.0f, float (size - 2));
        // Integer/fraction split keeps the index exact (a float wrap could round up to `size`).
        const int whole = int (delaySamples);
        const float frac = delaySamples - float (whole);
        int i1 = write - whole;          // sample `whole` steps back
        if (i1 < 0) i1 += size;
        int i0 = i1 - 1;                 // one more step back for the fractional part
        if (i0 < 0) i0 += size;
        const float f = 1.0f - frac;     // interpolate between i0 (older) and i1 (newer)
        return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
};

// Stereo chorus / flanger / tremolo in one structure.
struct Chorus
{
    DelayLine l, r;
    float phase = 0.0f, sampleRate = 44100.0f, fbL = 0, fbR = 0;

    void prepare (float sr) { sampleRate = sr; l.prepare (int (sr * 0.06f)); r.prepare (int (sr * 0.06f)); phase = 0; fbL = fbR = 0; }
    void reset() noexcept { l.reset(); r.reset(); fbL = fbR = 0; }

    // baseMs/depthMs: delay centre and modulation; feedback -0.9..0.9; tremolo 0..1; mix 0..1
    void process (const float* in, float* outL, float* outR, int n, float rateHz, float baseMs, float depthMs,
                  float feedback, float tremolo, float mix) noexcept
    {
        const float inc = rateHz / sampleRate;
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            const float s = std::sin (kTwoPi * phase);
            const float c = std::cos (kTwoPi * phase);
            const float x = in[i];
            l.push (x + fbL * feedback);
            r.push (x + fbR * feedback);
            const float dl = (baseMs + depthMs * 0.5f * (1.0f + s)) * 0.001f * sampleRate;
            const float dr = (baseMs + depthMs * 0.5f * (1.0f + c)) * 0.001f * sampleRate;
            fbL = l.read (dl);
            fbR = r.read (dr);
            const float trem = 1.0f - tremolo * 0.5f * (1.0f + s);
            outL[i] = (x * (1.0f - mix) + fbL * mix) * trem;
            outR[i] = (x * (1.0f - mix) + fbR * mix) * (tremolo > 0 ? 1.0f - tremolo * 0.5f * (1.0f - s) : 1.0f);
        }
    }
};

// 8-line feedback delay network reverb (original design).
struct Reverb
{
    static constexpr int N = 8;
    DelayLine lines[N];
    DelayLine pre;
    float lengths[N] {};
    float damp[N] {};
    float sampleRate = 44100.0f;

    void prepare (float sr)
    {
        sampleRate = sr;
        for (auto& d : lines) d.prepare (int (sr * 0.25f));
        pre.prepare (int (sr * 0.2f));
    }
    void reset() noexcept { for (auto& d : lines) d.reset(); pre.reset(); std::fill (std::begin (damp), std::end (damp), 0.0f); }

    // size 0..1, decaySeconds, dampHz, preDelayMs
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  float size, float decaySeconds, float dampHz, float preDelayMs) noexcept
    {
        static constexpr float base[N] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0533f, 0.0617f, 0.0683f, 0.0751f };
        float g[N];
        for (int k = 0; k < N; ++k)
        {
            lengths[k] = base[k] * (0.35f + 1.65f * clampf (size, 0.0f, 1.0f)) * sampleRate;
            const float t = lengths[k] / sampleRate;
            g[k] = std::pow (10.0f, -3.0f * t / std::max (0.05f, decaySeconds));
        }
        const float dc = std::exp (-kTwoPi * clampf (dampHz, 200.0f, sampleRate * 0.45f) / sampleRate);
        const float pd = std::max (1.0f, preDelayMs * 0.001f * sampleRate);

        for (int i = 0; i < n; ++i)
        {
            pre.push (0.5f * (inL[i] + inR[i]));
            const float x = pre.read (pd);
            float o[N];
            for (int k = 0; k < N; ++k)
            {
                o[k] = lines[k].read (lengths[k]);
                damp[k] = o[k] * (1.0f - dc) + damp[k] * dc; // one-pole lowpass in the loop
            }
            // Hadamard-like mixing (fast Walsh-Hadamard transform, normalised)
            float h[N];
            for (int k = 0; k < N; ++k) h[k] = damp[k];
            for (int len = 1; len < N; len <<= 1)
                for (int s = 0; s < N; s += len << 1)
                    for (int j = s; j < s + len; ++j)
                    {
                        const float a = h[j], b = h[j + len];
                        h[j] = a + b; h[j + len] = a - b;
                    }
            const float norm = 0.35355339f; // 1/sqrt(8)
            for (int k = 0; k < N; ++k)
                lines[k].push (x + h[k] * norm * g[k]);

            outL[i] = 0.5f * (o[0] - o[2] + o[4] - o[6]);
            outR[i] = 0.5f * (o[1] - o[3] + o[5] - o[7]);
        }
    }
};

// Stereo feedback delay
struct StereoDelay
{
    DelayLine l, r;
    float sampleRate = 44100.0f;
    void prepare (float sr) { sampleRate = sr; l.prepare (int (sr * 1.1f)); r.prepare (int (sr * 1.1f)); }
    void reset() noexcept { l.reset(); r.reset(); }
    void process (float* L, float* R, int n, float timeMs, float feedback, float mix) noexcept
    {
        const float d = clampf (timeMs, 1.0f, 1000.0f) * 0.001f * sampleRate;
        for (int i = 0; i < n; ++i)
        {
            const float yl = l.read (d), yr = r.read (d * 0.75f + 1.0f);
            l.push (L[i] + yr * feedback);   // ping-pong cross feedback
            r.push (R[i] + yl * feedback);
            L[i] += yl * mix;
            R[i] += yr * mix;
        }
    }
};
} // namespace dc
