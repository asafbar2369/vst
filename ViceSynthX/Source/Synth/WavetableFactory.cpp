// Original, procedurally designed factory wavetables for VICE SYNTH X.
// Every table below is computed from mathematical descriptions written for this project.

#include "Wavetable.h"
#include "../Core/DspUtils.h"
#include <map>

namespace vsx::WavetableFactory
{
namespace
{
    constexpr int N = Wavetable::kSize;
    using Frame = std::vector<float>;
    using Frames = std::vector<Frame>;

    /** Frame from harmonic amplitudes (index 0 = fundamental) and optional phases (radians, cosine-referenced). */
    Frame harm (const std::vector<float>& amps, const std::vector<float>* phases = nullptr)
    {
        static thread_local juce::dsp::FFT fft (11);
        std::vector<float> buf ((size_t) N * 2, 0.0f);
        for (size_t k = 0; k < amps.size() && k + 1 < (size_t) N / 2; ++k)
        {
            const size_t h = k + 1;
            const float ph = phases != nullptr && k < phases->size() ? (*phases)[k] : -kPi * 0.5f; // default sine
            buf[2 * h] = (float) N * 0.5f * amps[k] * std::cos (ph);
            buf[2 * h + 1] = (float) N * 0.5f * amps[k] * std::sin (ph);
        }
        fft.performRealOnlyInverseTransform (buf.data());
        return Frame (buf.begin(), buf.begin() + N);
    }

    Frame timeDomain (const std::function<float (float)>& fn)
    {
        Frame f ((size_t) N);
        for (int i = 0; i < N; ++i)
            f[(size_t) i] = fn ((float) i / (float) N);
        return f;
    }

    Frame mix (const Frame& a, const Frame& b, float t)
    {
        Frame r (a.size());
        for (size_t i = 0; i < a.size(); ++i) r[i] = a[i] + (b[i] - a[i]) * t;
        return r;
    }

    std::vector<float> sawAmps (int count, float rolloff = 1.0f)
    {
        std::vector<float> a ((size_t) count);
        for (int k = 1; k <= count; ++k) a[(size_t) k - 1] = 1.0f / std::pow ((float) k, rolloff);
        return a;
    }

    std::vector<float> squareAmps (int count)
    {
        std::vector<float> a ((size_t) count, 0.0f);
        for (int k = 1; k <= count; k += 2) a[(size_t) k - 1] = 1.0f / (float) k;
        return a;
    }

    std::vector<float> triAmps (int count)
    {
        std::vector<float> a ((size_t) count, 0.0f);
        for (int k = 1; k <= count; k += 2) a[(size_t) k - 1] = ((k / 2) % 2 == 0 ? 1.0f : -1.0f) / (float) (k * k);
        return a;
    }

    std::vector<float> pulseAmps (int count, float width)
    {
        std::vector<float> a ((size_t) count);
        for (int k = 1; k <= count; ++k) a[(size_t) k - 1] = 2.0f / ((float) k * kPi) * std::sin ((float) k * kPi * width);
        return a;
    }

    Frame pulseFrame (float width)
    {
        // cosine-referenced pulse series centred at phase 0
        std::vector<float> amps = pulseAmps (1000, width);
        std::vector<float> ph ((size_t) amps.size(), 0.0f);
        for (size_t k = 0; k < amps.size(); ++k) ph[k] = -kPi * (float) (k + 1) * width;
        return harm (amps, &ph);
    }

    float resonance (float freq, float centre, float bw)
    {
        const float d = (freq - centre) / bw;
        return 1.0f / (1.0f + d * d);
    }

    struct Vowel { float f1, f2, f3; };
    const Vowel vowels[] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };

    std::vector<float> vowelAmps (const Vowel& v, float f0, float tilt, int count)
    {
        std::vector<float> a ((size_t) count);
        for (int k = 1; k <= count; ++k)
        {
            const float f = f0 * (float) k;
            const float env = 0.08f + resonance (f, v.f1, 90.0f) + 0.7f * resonance (f, v.f2, 110.0f) + 0.4f * resonance (f, v.f3, 160.0f);
            a[(size_t) k - 1] = env / std::pow ((float) k, tilt);
        }
        return a;
    }

    Vowel lerpVowel (const Vowel& a, const Vowel& b, float t)
    {
        return { lerp (a.f1, b.f1, t), lerp (a.f2, b.f2, t), lerp (a.f3, b.f3, t) };
    }

    float foldTri (float x)
    {
        // triangle-shaped wavefolder into [-1, 1]
        x = (x + 1.0f) * 0.25f;
        x = x - std::floor (x);
        return 1.0f - 4.0f * std::abs (x - 0.5f);
    }

    // ---------------------------------------------------------------------------------
    Frames basicShapes()
    {
        const Frame sine = harm ({ 1.0f });
        const Frame tri = harm (triAmps (999));
        const Frame saw = harm (sawAmps (1000));
        const Frame sq = harm (squareAmps (1000));
        const Frame pulse = pulseFrame (0.12f);
        const Frame keys[] = { sine, tri, saw, sq, pulse };
        Frames out;
        const int numFrames = 64;
        for (int f = 0; f < numFrames; ++f)
        {
            const float pos = (float) f / (numFrames - 1) * 4.0f;
            const int i = std::min (3, (int) pos);
            out.push_back (mix (keys[i], keys[i + 1], pos - (float) i));
        }
        return out;
    }

    Frames pwmSweep()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
            out.push_back (pulseFrame (lerp (0.5f, 0.02f, (float) f / 47.0f)));
        return out;
    }

    Frames harmonicSweep()
    {
        Frames out;
        for (int f = 0; f < 64; ++f)
        {
            const int count = 1 + (int) std::round (std::pow ((float) f, 1.55f));
            std::vector<float> a ((size_t) std::min (count, 600));
            for (size_t k = 0; k < a.size(); ++k) a[k] = 1.0f / std::sqrt ((float) k + 1.0f);
            out.push_back (harm (a));
        }
        return out;
    }

    Frames formantVowels (float tilt, float breath)
    {
        Frames out;
        const int numFrames = 64;
        for (int f = 0; f < numFrames; ++f)
        {
            const float pos = (float) f / (numFrames - 1) * 4.0f;
            const int i = std::min (3, (int) pos);
            const auto v = lerpVowel (vowels[i], vowels[i + 1], pos - (float) i);
            auto a = vowelAmps (v, 110.0f, tilt, 200);
            if (breath > 0.0f)
                for (size_t k = 8; k < a.size(); ++k) a[k] += breath * 0.02f / std::sqrt ((float) k);
            out.push_back (harm (a));
        }
        return out;
    }

    Frames digitalFm()
    {
        Frames out;
        for (int f = 0; f < 64; ++f)
        {
            const float index = 8.0f * std::pow ((float) f / 63.0f, 1.3f);
            out.push_back (timeDomain ([index] (float p) { return std::sin (kTwoPi * p + index * std::sin (kTwoPi * p)); }));
        }
        return out;
    }

    Frames fmRatios()
    {
        Frames keys;
        const int ratios[] = { 1, 2, 3, 4, 5, 7 };
        for (int r : ratios)
            keys.push_back (timeDomain ([r] (float p) { return std::sin (kTwoPi * p + 2.2f * std::sin (kTwoPi * (float) r * p)); }));
        Frames out;
        const int numFrames = 64;
        for (int f = 0; f < numFrames; ++f)
        {
            const float pos = (float) f / (numFrames - 1) * (float) (keys.size() - 1);
            const int i = std::min ((int) keys.size() - 2, (int) pos);
            out.push_back (mix (keys[(size_t) i], keys[(size_t) i + 1], pos - (float) i));
        }
        return out;
    }

    Frames hardSync()
    {
        Frames out;
        for (int f = 0; f < 64; ++f)
        {
            const float ratio = 1.0f + 7.0f * (float) f / 63.0f;
            out.push_back (timeDomain ([ratio] (float p)
            {
                const float q = p * ratio;
                const float s = 2.0f * (q - std::floor (q)) - 1.0f;
                return s * (1.0f - 0.15f * p); // slight amplitude slope for character
            }));
        }
        return out;
    }

    Frames wavefold()
    {
        Frames out;
        for (int f = 0; f < 64; ++f)
        {
            const float g = 1.0f + 7.0f * (float) f / 63.0f;
            out.push_back (timeDomain ([g] (float p) { return foldTri (g * std::sin (kTwoPi * p)); }));
        }
        return out;
    }

    Frames drawbarOrgan()
    {
        // harmonic numbers relative to the 16' fundamental for 9 drawbars
        const int harmonics[] = { 1, 3, 2, 4, 6, 8, 10, 12, 16 };
        const float regs[][9] = {
            { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 8, 8, 0, 0, 0, 0, 0 }, { 8, 0, 8, 0, 0, 0, 0, 0, 8 },
            { 8, 6, 8, 6, 4, 4, 0, 0, 0 }, { 6, 8, 8, 6, 0, 0, 0, 0, 6 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 },
            { 0, 0, 8, 8, 8, 0, 0, 0, 0 }, { 8, 3, 8, 2, 0, 0, 0, 6, 8 } };
        Frames keys;
        for (const auto& reg : regs)
        {
            std::vector<float> a (16, 0.0f);
            for (int d = 0; d < 9; ++d)
                a[(size_t) harmonics[d] - 1] += std::pow (10.0f, (reg[d] - 8.0f) * 3.0f / 20.0f) * (reg[d] > 0 ? 1.0f : 0.0f);
            keys.push_back (harm (a));
        }
        Frames out;
        const int numFrames = 57;
        for (int f = 0; f < numFrames; ++f)
        {
            const float pos = (float) f / (numFrames - 1) * (float) (keys.size() - 1);
            const int i = std::min ((int) keys.size() - 2, (int) pos);
            out.push_back (mix (keys[(size_t) i], keys[(size_t) i + 1], pos - (float) i));
        }
        return out;
    }

    Frames glass()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float centre = 3.0f + 14.0f * (float) f / 47.0f;
            std::vector<float> a (64, 0.0f);
            for (int k = 1; k <= 64; k += 2)
                a[(size_t) k - 1] = 0.25f / (float) k + resonance ((float) k, centre, 2.5f) * 0.6f;
            a[0] = 1.0f;
            out.push_back (harm (a));
        }
        return out;
    }

    Frames hollowSquare()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float even = (float) f / 47.0f;
            std::vector<float> a (1000);
            for (int k = 1; k <= 1000; ++k)
                a[(size_t) k - 1] = (k % 2 == 1 ? 1.0f : even * 0.8f) / (float) k * (k == 2 ? (1.0f - even * 0.5f) : 1.0f);
            out.push_back (harm (a));
        }
        return out;
    }

    Frames bitcrushSaw()
    {
        Frames out;
        for (int f = 0; f < 40; ++f)
        {
            const float bits = lerp (10.0f, 1.6f, (float) f / 39.0f);
            const float levels = std::pow (2.0f, bits);
            out.push_back (timeDomain ([levels] (float p)
            {
                const float s = 2.0f * p - 1.0f;
                return std::round (s * levels * 0.5f) / (levels * 0.5f);
            }));
        }
        return out;
    }

    Frames sawPhase()
    {
        Frames out;
        const Frame saw = harm (sawAmps (1000));
        for (int f = 0; f < 48; ++f)
        {
            const int shift = (int) ((float) f / 47.0f * N * 0.5f);
            Frame fr ((size_t) N);
            for (int i = 0; i < N; ++i)
                fr[(size_t) i] = saw[(size_t) i] - 0.8f * saw[(size_t) ((i + shift) % N)];
            out.push_back (fr);
        }
        return out;
    }

    Frames combSweep()
    {
        Frames out;
        for (int f = 0; f < 64; ++f)
        {
            const float spacing = 2.0f + 14.0f * (float) f / 63.0f;
            std::vector<float> a (400);
            for (int k = 1; k <= 400; ++k)
                a[(size_t) k - 1] = (1.0f / (float) k) * (0.5f + 0.5f * std::cos (kTwoPi * (float) k / spacing));
            out.push_back (harm (a));
        }
        return out;
    }

    Frames growl()
    {
        Frames out;
        for (int f = 0; f < 64; ++f)
        {
            const float t = (float) f / 63.0f;
            const auto v = lerpVowel (vowels[3], vowels[0], t);
            auto base = harm (vowelAmps (v, 80.0f, 0.6f, 300));
            float peak = 1.0e-6f;
            for (float s : base) peak = std::max (peak, std::abs (s));
            const float drive = 1.0f + 5.0f * t;
            for (auto& s : base) s = std::tanh (drive * s / peak);
            out.push_back (base);
        }
        return out;
    }

    Frames steppedDigital()
    {
        Frames out;
        Rng rng (1234567u);
        std::vector<float> stepsA (16), stepsB (16);
        for (auto& s : stepsA) s = rng.bi();
        for (int f = 0; f < 32; ++f)
        {
            if (f % 8 == 0)
            {
                stepsB = stepsA;
                for (auto& s : stepsA) s = rng.bi();
            }
            const float t = (float) (f % 8) / 8.0f;
            out.push_back (timeDomain ([&] (float p)
            {
                const int i = (int) (p * 16.0f) % 16;
                return lerp (stepsB[(size_t) i], stepsA[(size_t) i], t);
            }));
        }
        return out;
    }

    Frames stringsEnsemble()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float bright = 0.4f + 1.2f * (float) f / 47.0f;
            std::vector<float> a (300);
            for (int k = 1; k <= 300; ++k)
            {
                const float freq = 196.0f * (float) k;
                const float body = 0.5f + resonance (freq, 290.0f, 60.0f) + 0.8f * resonance (freq, 1000.0f, 250.0f)
                                 + 0.6f * resonance (freq, 2800.0f, 600.0f) * bright;
                a[(size_t) k - 1] = body / std::pow ((float) k, 1.35f - 0.35f * bright);
            }
            out.push_back (harm (a));
        }
        return out;
    }

    Frames brass()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float b = 2.0f + 22.0f * std::pow ((float) f / 47.0f, 1.5f);
            std::vector<float> a (200);
            for (int k = 1; k <= 200; ++k)
            {
                const float freq = 175.0f * (float) k;
                a[(size_t) k - 1] = std::exp (-(float) k / b) * (1.0f + 1.5f * resonance (freq, 1200.0f, 400.0f));
            }
            out.push_back (harm (a));
        }
        return out;
    }

    Frames softPad()
    {
        Frames out;
        for (int f = 0; f < 32; ++f)
        {
            const float t = (float) f / 31.0f;
            std::vector<float> a (24);
            for (int k = 1; k <= 24; ++k)
                a[(size_t) k - 1] = 1.0f / std::pow ((float) k, 2.6f - 1.2f * t) * (1.0f + 0.3f * std::sin ((float) k * 1.7f + t * 6.0f));
            out.push_back (harm (a));
        }
        return out;
    }

    Frames electricTine()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float idx = 3.2f * std::exp (-3.0f * (float) f / 47.0f);
            const float bell = 0.25f * std::exp (-6.0f * (float) f / 47.0f);
            out.push_back (timeDomain ([idx, bell] (float p)
            {
                return std::sin (kTwoPi * p + idx * std::sin (kTwoPi * p)) + bell * std::sin (kTwoPi * 14.0f * p);
            }));
        }
        return out;
    }

    Frames noiseSpectra()
    {
        Frames keys;
        Rng rng (987654u);
        for (int i = 0; i < 8; ++i)
        {
            std::vector<float> a (256);
            for (int k = 1; k <= 256; ++k) a[(size_t) k - 1] = rng.uni() / std::pow ((float) k, 0.7f);
            keys.push_back (harm (a));
        }
        Frames out;
        const int numFrames = 57;
        for (int f = 0; f < numFrames; ++f)
        {
            const float pos = (float) f / (numFrames - 1) * (float) (keys.size() - 1);
            const int i = std::min ((int) keys.size() - 2, (int) pos);
            out.push_back (mix (keys[(size_t) i], keys[(size_t) i + 1], pos - (float) i));
        }
        return out;
    }

    Frames bassWeight()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float t = (float) f / 47.0f;
            out.push_back (timeDomain ([t] (float p)
            {
                const float s1 = std::sin (kTwoPi * p);
                const float s2 = std::sin (kTwoPi * 2.0f * p + t * 2.0f);
                const float s3 = std::sin (kTwoPi * 3.0f * p + t * 4.0f);
                return std::tanh ((1.5f + 2.5f * t) * (s1 + 0.5f * t * s2 + 0.25f * t * s3));
            }));
        }
        return out;
    }

    Frames analogDriftSaw()
    {
        Frames out;
        Rng rng (424242u);
        for (int f = 0; f < 32; ++f)
        {
            std::vector<float> a (1000), ph (1000);
            for (int k = 1; k <= 1000; ++k)
            {
                a[(size_t) k - 1] = (1.0f / (float) k) * (1.0f + 0.12f * rng.bi() * (float) f / 31.0f);
                ph[(size_t) k - 1] = -kPi * 0.5f + 0.4f * rng.bi() * (float) f / 31.0f;
            }
            out.push_back (harm (a, &ph));
        }
        return out;
    }

    Frames mirrorSine()
    {
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float m = lerp (0.5f, 0.03f, (float) f / 47.0f);
            out.push_back (timeDomain ([m] (float p)
            {
                const float q = p < m ? p / m * 0.5f : 0.5f + (p - m) / (1.0f - m) * 0.5f;
                return std::cos (kTwoPi * q);
            }));
        }
        return out;
    }

    Frames chipPulse()
    {
        Frames keys;
        keys.push_back (pulseFrame (0.125f));
        keys.push_back (pulseFrame (0.25f));
        keys.push_back (pulseFrame (0.5f));
        keys.push_back (timeDomain ([] (float p)
        {
            const float tri = 1.0f - 4.0f * std::abs (p - 0.5f);
            return std::round (tri * 7.5f) / 7.5f; // 4-bit stepped triangle
        }));
        Frames out;
        const int numFrames = 31;
        for (int f = 0; f < numFrames; ++f)
        {
            const float pos = (float) f / (numFrames - 1) * 3.0f;
            const int i = std::min (2, (int) pos);
            out.push_back (mix (keys[(size_t) i], keys[(size_t) i + 1], pos - (float) i));
        }
        return out;
    }

    Frames sawStackMorph()
    {
        // two detuned-in-harmonic-space saws: emulates a static "stacked" saw timbre
        Frames out;
        for (int f = 0; f < 48; ++f)
        {
            const float t = (float) f / 47.0f;
            std::vector<float> a (1000);
            for (int k = 1; k <= 1000; ++k)
                a[(size_t) k - 1] = (1.0f / (float) k) * (0.6f + 0.4f * std::cos ((float) k * t * 0.35f));
            out.push_back (harm (a));
        }
        return out;
    }
}


// =================================================================================================================
// Generated families (2026-11-22): parameterised generators, each combination a named table. Every table is
// computed from first principles here (harmonic series, FM, formant resonances, folding, sync, phase warps); no
// sampled or third-party wavetable data. Distinctness is checked by Tests/TestWavetableLibrary.cpp.
namespace
{
    constexpr int kFamFrames = 32;

    Frame normalised (Frame f)
    {
        float mean = 0.0f;
        for (float v : f) mean += v;
        mean /= (float) f.size();
        float pk = 0.0f;
        for (auto& v : f) { v -= mean; pk = std::max (pk, std::abs (v)); }
        if (pk > 1.0e-9f) for (auto& v : f) v /= pk;
        return f;
    }

    template <typename Fn> Frames sweep (Fn&& frameAt)
    {
        Frames out;
        for (int i = 0; i < kFamFrames; ++i) out.push_back (normalised (frameAt ((float) i / (float) (kFamFrames - 1))));
        return out;
    }

    // ---- Spectra: additive envelopes (rolloff x harmonic set x motion) -------------------------------------------
    const char* const kRolloffNames[] = { "Bright", "Clear", "Warm", "Round", "Dark" };
    const float kRolloffs[] = { 0.6f, 0.9f, 1.2f, 1.6f, 2.2f };
    const char* const kSetNames[] = { "Full", "Odd", "Even", "Gapped", "Thirds" };
    const char* const kSpectraMotion[] = { "Open", "Tilt", "Peak" };

    Frames spectra (int r, int set, int motion)
    {
        return sweep ([=] (float t)
        {
            const int count = 256;
            std::vector<float> a ((size_t) count);
            for (int k = 1; k <= count; ++k)
            {
                float g = 1.0f / std::pow ((float) k, kRolloffs[r]);
                if (set == 1 && k % 2 == 0) g = 0.0f;                    // odd only
                if (set == 2 && k % 2 == 0) g *= 4.0f;                   // even emphasis
                if (set == 3 && k % 3 == 0) g = 0.0f;                    // every third removed
                if (set == 4 && k % 3 == 0) g *= 5.0f;                   // every third emphasised
                if (motion == 0) g *= k <= 6 + (int) (t * t * 120.0f) ? 1.0f : 0.0f;          // opening low-pass
                if (motion == 1) g *= std::pow ((float) k, 1.4f * t - 0.7f);                  // tilt from dark to bright
                if (motion == 2) g *= 0.15f + 4.0f * resonance ((float) k, 2.0f + 3.0f * (float) r + t * (38.0f - 6.0f * (float) r), 1.0f + 0.6f * (float) r + 2.0f * t); // moving peak (range per rolloff)
                a[(size_t) k - 1] = g;
            }
            return harm (a);
        });
    }

    // ---- FM: two operators with integer ratios (periodic in the frame) x index shape ------------------------------
    const int kFmRatios[][2] = { { 1, 1 }, { 1, 2 }, { 2, 1 }, { 1, 3 }, { 3, 1 }, { 2, 3 }, { 3, 2 }, { 1, 4 }, { 4, 1 }, { 3, 4 }, { 1, 5 }, { 5, 2 } };
    const char* const kFmShapes[] = { "Rise", "Curve", "Swell", "Deep" };

    Frames fmPair (int ratio, int shape)
    {
        const float c = (float) kFmRatios[ratio][0], m = (float) kFmRatios[ratio][1];
        return sweep ([=] (float t)
        {
            const float idx = shape == 0 ? 8.0f * t : shape == 1 ? 8.0f * t * t * t : shape == 2 ? 6.0f * std::sin (kPi * t) : 16.0f * t;
            return timeDomain ([=] (float p) { return std::sin (kTwoPi * c * p + idx * std::sin (kTwoPi * m * p)); });
        });
    }

    // ---- Formant: vowel paths x voice register ---------------------------------------------------------------
    const char* const kVowelNames[] = { "A", "E", "I", "O", "U" };
    const char* const kRegisterNames[] = { "Low", "Mid", "High", "Child" };
    const float kRegisterF0[] = { 98.0f, 165.0f, 262.0f, 440.0f };
    const float kRegisterScale[] = { 0.92f, 1.0f, 1.12f, 1.35f };

    Frames formantPath (int from, int to, int reg)
    {
        return sweep ([=] (float t)
        {
            auto v = lerpVowel (vowels[from], vowels[to], 0.5f - 0.5f * std::cos (kPi * t));
            v = { v.f1 * kRegisterScale[reg], v.f2 * kRegisterScale[reg], v.f3 * kRegisterScale[reg] };
            // bandwidths grow with the register so a formant always spans the neighbouring harmonics (with narrow
            // bandwidths, high registers lost the vowel: measured near-duplicates)
            const float f0 = kRegisterF0[reg];
            const float b1 = std::max (90.0f, 0.5f * f0), b2 = std::max (110.0f, 0.6f * f0), b3 = std::max (160.0f, 0.7f * f0);
            std::vector<float> a (160);
            for (int k = 1; k <= 160; ++k)
            {
                const float f = f0 * (float) k;
                const float env = 0.04f + resonance (f, v.f1, b1) + 0.7f * resonance (f, v.f2, b2) + 0.4f * resonance (f, v.f3, b3);
                a[(size_t) k - 1] = env / std::pow ((float) k, 0.7f);
            }
            return harm (a);
        });
    }

    // ---- Fold: wavefolder over three inputs x drive x offset ---------------------------------------------------
    const char* const kFoldInputs[] = { "Sine", "Triangle", "Ramp" };
    const float kFoldDrives[] = { 2.0f, 4.0f, 8.0f, 16.0f };
    const char* const kFoldOffsets[] = { "Centred", "Offset", "Skewed" };

    Frames fold (int input, int drive, int offset)
    {
        return sweep ([=] (float t)
        {
            const float g = 1.0f + (kFoldDrives[drive] - 1.0f) * t;
            const float bias = offset == 0 ? 0.0f : offset == 1 ? 0.3f * t : 0.6f * t;
            return timeDomain ([=] (float p)
            {
                float x = input == 0 ? std::sin (kTwoPi * p) : input == 1 ? 1.0f - 4.0f * std::abs (p - 0.5f)
                        : std::sin (kPi * (2.0f * p - 1.0f) * 0.5f);   // smooth ramp (no step at the wrap)
                return foldTri (x * g + bias);
            });
        });
    }

    // ---- Sync: hard-synced slave x ratio range x window --------------------------------------------------------
    const char* const kSyncSlaves[] = { "Saw", "Sine", "Square" };
    const float kSyncTop[] = { 2.0f, 4.0f, 8.0f, 16.0f };
    const float kSyncBottom[] = { 1.0f, 1.0f, 1.0f, 2.0f };
    const char* const kSyncWindows[] = { "Raw", "Faded", "Half" };

    Frames syncSweep (int slave, int range, int window)
    {
        return sweep ([=] (float t)
        {
            const float ratio = kSyncBottom[range] + (kSyncTop[range] - kSyncBottom[range]) * t;
            return timeDomain ([=] (float p)
            {
                const float q = p * ratio - std::floor (p * ratio);
                float y = slave == 0 ? 2.0f * q - 1.0f : slave == 1 ? std::sin (kTwoPi * q) : (q < 0.5f ? 1.0f : -1.0f);
                if (window == 1) y *= 0.5f - 0.5f * std::cos (kTwoPi * p);
                if (window == 2) y *= p < 0.5f ? 1.0f : 0.0f;
                return y;
            });
        });
    }

    // ---- Pulse: duty paths x edge x single / dual ---------------------------------------------------------------
    const char* const kPulsePaths[] = { "Narrowing", "Widening", "Swing", "Extreme" };
    const char* const kPulseEdges[] = { "Hard", "Soft", "Rounded" };

    Frames pulseFamily (int path, int edge, bool dual)
    {
        return sweep ([=] (float t)
        {
            const float w = path == 0 ? 0.5f - 0.45f * t : path == 1 ? 0.05f + 0.45f * t : path == 2 ? 0.25f + 0.5f * t : 0.05f + 0.9f * t;
            std::vector<float> a ((size_t) 512);
            for (int k = 1; k <= 512; ++k)
            {
                float g = 2.0f / ((float) k * kPi) * std::sin ((float) k * kPi * w);
                if (dual) g *= std::cos ((float) k * kPi * 0.5f);          // a second pulse half a cycle later
                if (edge == 1) g /= std::pow ((float) k, 0.3f);
                if (edge == 2) g *= std::exp (-(float) k / 24.0f);
                a[(size_t) k - 1] = g;
            }
            return harm (a);
        });
    }

    // ---- Warp: phase maps on three bases -----------------------------------------------------------------------
    const char* const kWarpBases[] = { "Sine", "Triangle", "Saw" };
    const char* const kWarpTypes[] = { "Bend Up", "Bend Down", "Mirror", "Asym", "Quantise", "Sine Warp", "Pinch", "Stretch" };

    Frames warp (int base, int type)
    {
        const Frame saw = harm (sawAmps (512));
        return sweep ([=] (float t)
        {
            return timeDomain ([=, &saw] (float p)
            {
                float q = p;
                switch (type)
                {
                    case 0: q = std::pow (p, 1.0f / (1.0f + 4.0f * t)); break;
                    case 1: q = std::pow (p, 1.0f + 4.0f * t); break;
                    case 2: q = p < 0.5f ? p * (1.0f + t) : 1.0f - (1.0f - p) * (1.0f + t); q -= std::floor (q); break;
                    case 3: { const float k = 0.5f + 0.45f * t; q = p < k ? 0.5f * p / k : 0.5f + 0.5f * (p - k) / (1.0f - k); break; }
                    case 4: { const float steps = 64.0f - 60.0f * t; q = std::floor (p * steps) / steps; break; }
                    case 5: q = p + 0.15f * t * std::sin (kTwoPi * 2.0f * p); break;
                    case 6: { const float d = p - 0.5f; q = 0.5f + d * (1.0f - t * 0.9f * (1.0f - 4.0f * d * d)); break; }
                    default: q = 0.5f + (p - 0.5f) * (1.0f + 2.0f * t); q = juce::jlimit (0.0f, 1.0f, q); break;
                }
                q -= std::floor (q);
                if (base == 0) return std::sin (kTwoPi * q);
                if (base == 1) return 1.0f - 4.0f * std::abs (q - 0.5f);
                return saw[(size_t) std::min (N - 1, (int) (q * (float) N))];
            });
        });
    }

    // ---- Stepped: bit depth and sample-and-hold reduction --------------------------------------------------------
    const char* const kStepBases[] = { "Sine", "Triangle", "Saw" };
    const char* const kStepModes[] = { "Bits", "Hold", "Both" };

    Frames stepped (int base, int mode, bool smooth)
    {
        return sweep ([=] (float t)
        {
            const float levels = std::exp2 (8.0f - 6.0f * t), holds = std::exp2 (7.0f - 5.0f * t);
            Frame f = timeDomain ([=] (float p)
            {
                if (mode != 0) p = std::floor (p * holds) / holds;
                float y = base == 0 ? std::sin (kTwoPi * p) : base == 1 ? 1.0f - 4.0f * std::abs (p - 0.5f) : 2.0f * p - 1.0f;
                if (mode != 1) y = std::round (y * levels) / levels;
                return y;
            });
            if (smooth)
            {
                // 48-tap circular moving average: a real low-pass on the steps (first null near harmonic 43)
                const int taps = 48, n = (int) f.size();
                Frame g (f.size());
                float acc = 0.0f;
                for (int i = -taps / 2; i < taps / 2; ++i) acc += f[(size_t) ((i + n) % n)];
                for (int i = 0; i < n; ++i)
                {
                    g[(size_t) i] = acc / (float) taps;
                    acc += f[(size_t) ((i + taps / 2) % n)] - f[(size_t) ((i - taps / 2 + n) % n)];
                }
                return g;
            }
            return f;
        });
    }

    // ---- Comb: notches in a saw / square spectrum -----------------------------------------------------------------
    const char* const kCombTypes[] = { "Spread", "Three", "Five", "Phaser" };

    Frames comb (int type, bool square, bool full)
    {
        return sweep ([=] (float t)
        {
            std::vector<float> a ((size_t) 400);
            for (int k = 1; k <= 400; ++k)
            {
                float g = square ? (k % 2 == 1 ? 1.0f / (float) k : 0.0f) : 1.0f / (float) k;
                const float depth = full ? 1.0f : 0.8f;
                float notch;
                if (type == 0) notch = std::cos (kPi * (float) k / (2.0f + 10.0f * t));
                else if (type == 1) notch = std::cos (kPi * (float) k / 3.0f + kPi * t);
                else if (type == 2) notch = std::cos (kPi * (float) k / 5.0f + kPi * t);
                else notch = std::cos (kPi * (float) k / (4.0f + 20.0f * t)) * std::cos (kPi * (float) k / (9.0f + 9.0f * t));
                g *= 1.0f - depth * (0.5f + 0.5f * notch);
                a[(size_t) k - 1] = g;
            }
            return harm (a);
        });
    }

    // ---- Organ: drawbar registrations morphing (harmonics 1, 2, 3, 4, 5, 6, 8) ----------------------------------
    const int kBarHarm[] = { 1, 2, 3, 4, 5, 6, 8 };
    const int kRegistrations[][7] = { { 8, 8, 8, 0, 0, 0, 0 }, { 8, 0, 8, 0, 0, 0, 8 }, { 8, 8, 8, 8, 8, 8, 8 }, { 8, 4, 0, 0, 0, 0, 0 },
                                      { 6, 8, 8, 6, 0, 0, 0 }, { 8, 0, 0, 0, 8, 0, 0 }, { 0, 8, 0, 8, 0, 8, 8 }, { 8, 6, 4, 2, 2, 2, 2 },
                                      { 4, 8, 0, 0, 0, 0, 0 }, { 8, 8, 0, 6, 0, 4, 2 }, { 8, 0, 6, 0, 4, 0, 2 }, { 2, 4, 6, 8, 6, 4, 2 },
                                      { 8, 3, 8, 3, 0, 0, 0 } };
    const char* const kRegNames[] = { "Gospel", "Hollow", "Full", "Soft", "Jazz", "Thirds", "Upper", "Fading", "Flute", "Rock",
                                      "Odd", "Arch", "Reed" };

    Frames organ (int from, int to, bool click)
    {
        return sweep ([=] (float t)
        {
            std::vector<float> a ((size_t) 20, 0.0f);
            for (int b = 0; b < 7; ++b)
            {
                const float lvl = lerp ((float) kRegistrations[from][b], (float) kRegistrations[to][b], t) / 8.0f;
                a[(size_t) kBarHarm[b] - 1] += lvl * lvl;
            }
            if (click) for (int k = 10; k <= 20; ++k) a[(size_t) k - 1] += 0.12f * (1.0f - t) / (float) (k - 9);
            return harm (a);
        });
    }

    // ---- Cluster: seeded groups of neighbouring harmonics moving through four key spectra ------------------------
    Frames cluster (int seed)
    {
        Rng rng ((uint32_t) (0x9E3779B9u * (uint32_t) (seed + 1)));
        std::vector<std::vector<float>> keys (4, std::vector<float> (96, 0.0f));
        for (auto& key : keys)
        {
            const int groups = 2 + (int) (rng.uni() * 4.0f);
            for (int gI = 0; gI < groups; ++gI)
            {
                const int centre = 1 + (int) (std::pow (rng.uni(), 1.6f) * 60.0f), width = 1 + (int) (rng.uni() * 5.0f);
                const float level = 0.3f + 0.7f * rng.uni();
                for (int k = std::max (1, centre - width); k <= std::min (96, centre + width); ++k)
                    key[(size_t) k - 1] += level / (1.0f + std::abs ((float) (k - centre))) / std::sqrt ((float) k);
            }
            key[0] += 0.4f; // keep a fundamental
        }
        return sweep ([&keys] (float t)
        {
            const float pos = t * 3.0f;
            const int i = std::min (2, (int) pos);
            const float f = pos - (float) i, s = 0.5f - 0.5f * std::cos (kPi * f);
            std::vector<float> a (96);
            for (size_t k = 0; k < 96; ++k) a[k] = lerp (keys[(size_t) i][k], keys[(size_t) i + 1][k], s);
            return harm (a);
        });
    }

    // ---- Texture: seeded random phases on a tilted spectrum (static timbre, moving phase) ------------------------
    const char* const kTextureTilts[] = { "Airy", "Grainy", "Dusky" };

    Frames texture (int seed, int tilt)
    {
        Rng rng ((uint32_t) (0x51ED27u + 977u * (uint32_t) seed + 31u * (uint32_t) tilt));
        std::vector<float> ph0 (300), ph1 (300);
        for (int k = 0; k < 300; ++k) { ph0[(size_t) k] = kTwoPi * rng.uni(); ph1[(size_t) k] = kTwoPi * rng.uni(); }
        const float roll = tilt == 0 ? 0.5f : tilt == 1 ? 0.9f : 1.5f;
        const int bands[] = { 4, 12, 24, 44 };
        const int band = bands[seed % 4];
        return sweep ([=] (float t)
        {
            std::vector<float> a (300), ph (300);
            for (int k = 1; k <= 300; ++k)
            {
                a[(size_t) k - 1] = (0.05f + resonance ((float) k, (float) band, (float) band * 0.4f)) / std::pow ((float) k, roll);
                ph[(size_t) k - 1] = lerp (ph0[(size_t) k - 1], ph1[(size_t) k - 1], t);
            }
            return harm (a, &ph);
        });
    }

    // ---- Partials: one harmonic gliding into another, and a fundamental joined by a rising harmonic ---------------
    Frames partialGlide (int from, int to)
    {
        return sweep ([=] (float t)
        {
            std::vector<float> a ((size_t) to, 0.0f);
            a[(size_t) from - 1] = std::cos (0.5f * kPi * t);
            a[(size_t) to - 1] += std::sin (0.5f * kPi * t);
            return harm (a);
        });
    }

    // ---- Ring: harmonic a multiplied by harmonic b (sum and difference partials), depth sweep ------------------------
    Frames ring (int a, int b)
    {
        return sweep ([=] (float t)
        {
            std::vector<float> amps ((size_t) (a + b), 0.0f);
            amps[(size_t) a - 1] += 1.0f - t;
            amps[(size_t) (b - a) - 1] += 0.5f * t;
            amps[(size_t) (a + b) - 1] += 0.5f * t;
            return harm (amps);
        });
    }

    Frames duo (int k)
    {
        return sweep ([=] (float t)
        {
            std::vector<float> a ((size_t) k, 0.0f);
            a[0] = 1.0f;
            a[(size_t) k - 1] = 1.5f * t;
            return harm (a);
        });
    }

    struct Family { juce::String name, category; std::function<Frames()> make; };

    const std::vector<Family>& families()
    {
        static const std::vector<Family> list = []
        {
            std::vector<Family> f;
            for (int r = 0; r < 5; ++r)
                for (int set = 0; set < 5; ++set)
                    for (int m = 0; m < 3; ++m)
                        f.push_back ({ juce::String ("Spectra ") + kRolloffNames[r] + " " + kSetNames[set] + " " + kSpectraMotion[m], "Spectra",
                                       [=] { return spectra (r, set, m); } });
            for (int r = 0; r < 12; ++r)
                for (int s = 0; s < 4; ++s)
                    if (r != 0 || s != 0) // 1:1 Rise is the classic "Digital FM"
                    f.push_back ({ "FM " + juce::String (kFmRatios[r][0]) + ":" + juce::String (kFmRatios[r][1]) + " " + kFmShapes[s], "FM",
                                   [=] { return fmPair (r, s); } });
            const int kPathVowels[] = { 0, 1, 2, 4 }; // A, E, I, U (O is close to A and U: those paths were near-duplicates)
            for (int reg = 0; reg < 3; ++reg) // Low, Mid, High (at 440 Hz "Child" the harmonics are too sparse to keep the vowels apart)
                for (int ia : kPathVowels)
                    for (int ib : kPathVowels)
                        if (const int a = ia, b = ib; a != b)
                            f.push_back ({ juce::String ("Formant ") + kVowelNames[a] + "-" + kVowelNames[b] + " " + kRegisterNames[reg], "Formant",
                                           [=] { return formantPath (a, b, reg); } });
            for (int in = 0; in < 3; ++in)
                for (int d = 0; d < 4; ++d)
                    for (int o = 0; o < (in == 2 ? 1 : 3); ++o) // Ramp: offsets measured as near-duplicates
                        f.push_back ({ juce::String ("Fold ") + kFoldInputs[in] + " " + juce::String ((int) kFoldDrives[d]) + "x " + kFoldOffsets[o], "Fold",
                                       [=] { return fold (in, d, o); } });
            for (int sl = 0; sl < 3; ++sl)
                for (int rg = 0; rg < 4; ++rg)
                    for (int w = 0; w < 3; ++w)
                        f.push_back ({ juce::String ("Sync ") + kSyncSlaves[sl] + " " + juce::String ((int) kSyncBottom[rg]) + "-" + juce::String ((int) kSyncTop[rg])
                                           + " " + kSyncWindows[w], "Sync", [=] { return syncSweep (sl, rg, w); } });
            for (int pa = 0; pa < 4; ++pa)
                for (int e = 0; e < 3; ++e)
                    for (int dual = 0; dual < 2; ++dual)
                        f.push_back ({ juce::String ("Pulse ") + kPulsePaths[pa] + " " + kPulseEdges[e] + (dual ? " Dual" : ""), "Pulse",
                                       [=] { return pulseFamily (pa, e, dual != 0); } });
            for (int b = 0; b < 2; ++b) // Sine, Triangle (phase warps of a saw keep the saw spectrum: near-duplicates)
                for (int t = 0; t < 8; ++t)
                    f.push_back ({ juce::String ("Warp ") + kWarpBases[b] + " " + kWarpTypes[t], "Warp", [=] { return warp (b, t); } });
            for (int b = 0; b < 3; ++b)
                for (int m = 0; m < 2; ++m) // Bits, Hold ("Both" measured as a near-duplicate of Hold)
                    for (int sm = 0; sm < 2; ++sm)
                        f.push_back ({ juce::String ("Stepped ") + kStepBases[b] + " " + kStepModes[m] + (sm ? " Smooth" : ""), "Stepped",
                                       [=] { return stepped (b, m, sm != 0); } });
            for (int t = 0; t < 4; ++t)
                for (int sq = 0; sq < 2; ++sq)
                    for (int full = 0; full < 2; ++full)
                        f.push_back ({ juce::String ("Comb ") + kCombTypes[t] + (sq ? " Square" : " Saw") + (full ? " Deep" : " Light"), "Comb",
                                       [=] { return comb (t, sq != 0, full != 0); } });
            for (int i = 0; i < 12; ++i)
                for (int click = 0; click < 2; ++click)
                {
                    const int a = i, b = (i + 5) % 13;
                    f.push_back ({ juce::String ("Organ ") + kRegNames[a] + " to " + kRegNames[b] + (click ? " Click" : ""), "Organ",
                                   [=] { return organ (a, b, click != 0); } });
                }
            for (int s = 0; s < 68; ++s)
                f.push_back ({ "Cluster " + juce::String (s + 1).paddedLeft ('0', 2), "Cluster", [=] { return cluster (s); } });
            for (int s = 0; s < 4; ++s)
                for (int t = 0; t < 3; ++t)
                    f.push_back ({ juce::String ("Texture ") + kTextureTilts[t] + " " + juce::String (s + 1), "Texture", [=] { return texture (s, t); } });
            for (int i = 1; i <= 10; ++i)
                for (int j = i + 1; j <= 10; ++j)
                    f.push_back ({ "Partials " + juce::String (i) + " to " + juce::String (j), "Partials", [=] { return partialGlide (i, j); } });
            for (int k : { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16 })
                f.push_back ({ "Partials Duo 1+" + juce::String (k), "Partials", [=] { return duo (k); } });
            for (int a = 2; a <= 8; ++a)
                for (int b = a + 1; b <= 8; ++b)
                    f.push_back ({ "Ring " + juce::String (a) + "x" + juce::String (b), "Ring", [=] { return ring (a, b); } });
            return f;
        }();
        return list;
    }

    const juce::StringArray kClassic { "Basic Shapes", "PWM Sweep", "Harmonic Sweep", "Formant Vowels", "Vocal Choir", "Digital FM",
                                       "FM Ratios", "Hard Sync", "Wavefold", "Drawbar Organ", "Glass", "Hollow Square", "Bitcrush Saw",
                                       "Saw Phase", "Comb Sweep", "Growl", "Stepped Digital", "Strings Ensemble", "Brass", "Soft Pad",
                                       "Electric Tine", "Noise Spectra", "Bass Weight", "Analog Drift Saw", "Mirror Sine", "Chip Pulse",
                                       "Saw Stack" };
}

juce::StringArray names()
{
    static const juce::StringArray all = []
    {
        juce::StringArray n = kClassic;
        for (const auto& f : families()) n.add (f.name);
        return n;
    }();
    return all;
}

juce::String category (const juce::String& name)
{
    static const std::map<juce::String, juce::String> map = []
    {
        std::map<juce::String, juce::String> m;
        for (const auto& n : kClassic) m[n] = "Classic";
        for (const auto& f : families()) m[f.name] = f.category;
        return m;
    }();
    const auto it = map.find (name);
    return it != map.end() ? it->second : juce::String();
}

juce::StringArray categories()
{
    juce::StringArray c { "Classic" };
    for (const auto& f : families()) c.addIfNotAlreadyThere (f.category);
    return c;
}

std::vector<std::vector<float>> generateFrames (const juce::String& name)
{
    if (name == "Basic Shapes") return basicShapes();
    if (name == "PWM Sweep") return pwmSweep();
    if (name == "Harmonic Sweep") return harmonicSweep();
    if (name == "Formant Vowels") return formantVowels (0.9f, 0.0f);
    if (name == "Vocal Choir") return formantVowels (1.2f, 1.0f);
    if (name == "Digital FM") return digitalFm();
    if (name == "FM Ratios") return fmRatios();
    if (name == "Hard Sync") return hardSync();
    if (name == "Wavefold") return wavefold();
    if (name == "Drawbar Organ") return drawbarOrgan();
    if (name == "Glass") return glass();
    if (name == "Hollow Square") return hollowSquare();
    if (name == "Bitcrush Saw") return bitcrushSaw();
    if (name == "Saw Phase") return sawPhase();
    if (name == "Comb Sweep") return combSweep();
    if (name == "Growl") return growl();
    if (name == "Stepped Digital") return steppedDigital();
    if (name == "Strings Ensemble") return stringsEnsemble();
    if (name == "Brass") return brass();
    if (name == "Soft Pad") return softPad();
    if (name == "Electric Tine") return electricTine();
    if (name == "Noise Spectra") return noiseSpectra();
    if (name == "Bass Weight") return bassWeight();
    if (name == "Analog Drift Saw") return analogDriftSaw();
    if (name == "Mirror Sine") return mirrorSine();
    if (name == "Chip Pulse") return chipPulse();
    if (name == "Saw Stack") return sawStackMorph();
    for (const auto& f : families()) if (f.name == name) return f.make();
    return basicShapes();
}

} // namespace vsx::WavetableFactory
