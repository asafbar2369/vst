#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace vsx
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr double kPiD = 3.14159265358979323846;

inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float hzToMidi (float hz) noexcept { return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0e-3f) / 440.0f); }

/** Rational tanh approximation, accurate to ~1e-4 in [-3, 3] and saturating smoothly outside. */
inline float fastTanh (float x) noexcept
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

/** Polynomial sine on [-pi, pi] (max error ~1e-6 after one refinement step). */
inline float fastSin (float x) noexcept
{
    // wrap into [-pi, pi]
    x = x - kTwoPi * std::floor ((x + kPi) * (1.0f / kTwoPi));
    const float B = 4.0f / kPi;
    const float C = -4.0f / (kPi * kPi);
    float y = B * x + C * x * std::abs (x);
    return 0.225f * (y * std::abs (y) - y) + y;
}

/** Sine of a normalised phase in [0,1). */
inline float sinPhase (float p) noexcept { return std::sin (kTwoPi * p); }

inline float flushDenormal (float x) noexcept
{
    return (std::abs (x) < 1.0e-15f) ? 0.0f : x;
}

inline bool isFiniteSample (float x) noexcept { return std::isfinite (x); }

/** PolyBLEP residual for a discontinuity, t = phase, dt = phase increment. */
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/** PolyBLAMP residual (integrated BLEP) for slope discontinuities. */
inline float polyBlamp (float t, float dt) noexcept
{
    if (t < dt)
    {
        t = t / dt - 1.0f;
        return -1.0f / 3.0f * t * t * t;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt + 1.0f;
        return 1.0f / 3.0f * t * t * t;
    }
    return 0.0f;
}

/** xorshift32 PRNG: real-time safe, deterministic. */
struct Rng
{
    uint32_t state = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 0x9E3779B9u) {}
    void seed (uint32_t s) noexcept { state = s ? s : 0x9E3779B9u; }
    uint32_t next() noexcept
    {
        uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        state = x;
        return x;
    }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float bi() noexcept { return uni() * 2.0f - 1.0f; }
};

inline uint32_t hash32 (uint32_t x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

inline float hashToBi (uint32_t x) noexcept { return (float) (hash32 (x) >> 8) * (2.0f / 16777216.0f) - 1.0f; }

/** One-pole smoother suitable for parameters, per sample. */
struct Smoother
{
    float current = 0.0f, target = 0.0f, coeff = 0.0f;
    void setTime (float seconds, double sr) noexcept
    {
        coeff = seconds <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (seconds * sr));
    }
    void reset (float v) noexcept { current = target = v; }
    void setTarget (float t) noexcept { target = t; }
    float next() noexcept
    {
        current = target + (current - target) * coeff;
        return current;
    }
};

/** Linear ramp used for control-rate to audio-rate interpolation. */
struct Ramp
{
    float value = 0.0f, step = 0.0f;
    void set (float target, int numSteps) noexcept
    {
        step = numSteps > 0 ? (target - value) / (float) numSteps : 0.0f;
        if (numSteps <= 0) value = target;
    }
    void jump (float v) noexcept { value = v; step = 0.0f; }
    float next() noexcept { value += step; return value; }
};

/** Simple one-pole filters. */
struct OnePoleLP
{
    float z = 0.0f, a = 0.0f;
    void setCutoff (float hz, double sr) noexcept
    {
        a = (float) std::exp (-2.0 * kPiD * std::min ((double) hz, sr * 0.49) / sr);
    }
    float process (float x) noexcept { z = x + (z - x) * a; return z; }
    void reset (float v = 0.0f) noexcept { z = v; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, double sr) noexcept { lp.setCutoff (hz, sr); }
    float process (float x) noexcept { return x - lp.process (x); }
    void reset() noexcept { lp.reset(); }
};

/** DC blocker (leaky differentiator). */
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void prepare (double sr) noexcept { r = (float) std::exp (-2.0 * kPiD * 10.0 / sr); }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = flushDenormal (y);
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

/**
    Lock-free hand-over of immutable resources (wavetables, samples, curves) from the
    message thread to the audio thread. The audio thread only performs an atomic load.
    Replaced objects are kept alive in a graveyard for a grace period, so the audio
    thread never frees memory.
*/
template <typename T>
class RtShared
{
public:
    RtShared() = default;
    ~RtShared() = default;

    /** Message / worker thread only. */
    void set (std::shared_ptr<const T> obj)
    {
        std::lock_guard<std::mutex> lock (mutex);
        purgeLocked (false);
        if (current != nullptr)
            graveyard.push_back ({ current, juce::Time::getMillisecondCounterHiRes() });
        current = std::move (obj);
        ptr.store (current.get(), std::memory_order_release);
    }

    /** Audio thread: returns a raw pointer valid for at least the grace period. */
    const T* get() const noexcept { return ptr.load (std::memory_order_acquire); }

    /** Message thread: strong reference for non-real-time readers. */
    std::shared_ptr<const T> getShared() const
    {
        std::lock_guard<std::mutex> lock (mutex);
        return current;
    }

    /** Releases objects that were replaced more than graceMs ago. */
    void collectGarbage (bool force = false)
    {
        std::lock_guard<std::mutex> lock (mutex);
        purgeLocked (force);
    }

private:
    struct Dead { std::shared_ptr<const T> obj; double time; };

    void purgeLocked (bool force)
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                         [&] (const Dead& d) { return force || now - d.time > graceMs; }),
                         graveyard.end());
    }

    static constexpr double graceMs = 3000.0;
    mutable std::mutex mutex;
    std::shared_ptr<const T> current;
    std::atomic<const T*> ptr { nullptr };
    std::vector<Dead> graveyard;
};

} // namespace vsx
