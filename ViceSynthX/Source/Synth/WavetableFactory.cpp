// Original, procedurally designed factory wavetables for VICE SYNTH X.
// Every table below is computed from mathematical descriptions written for this project.

#include "Wavetable.h"
#include "../Core/DspUtils.h"

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

juce::StringArray names()
{
    return { "Basic Shapes", "PWM Sweep", "Harmonic Sweep", "Formant Vowels", "Vocal Choir", "Digital FM",
             "FM Ratios", "Hard Sync", "Wavefold", "Drawbar Organ", "Glass", "Hollow Square", "Bitcrush Saw",
             "Saw Phase", "Comb Sweep", "Growl", "Stepped Digital", "Strings Ensemble", "Brass", "Soft Pad",
             "Electric Tine", "Noise Spectra", "Bass Weight", "Analog Drift Saw", "Mirror Sine", "Chip Pulse",
             "Saw Stack" };
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
    return basicShapes();
}

} // namespace vsx::WavetableFactory
