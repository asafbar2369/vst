// Original sample content for VICE SYNTH X, rendered procedurally on demand.
// No third-party recordings are used: all instruments below are synthesised with
// formant, modal, subtractive and physical-modelling techniques written for this project.

#include "SampleBank.h"
#include "../Core/DspUtils.h"

namespace vsx::BuiltinSamples
{
namespace
{
    constexpr double kSr = 44100.0;

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void bandpass (float f, float q)
        {
            const float w = kTwoPi * f / (float) kSr, alpha = std::sin (w) / (2.0f * q), c = std::cos (w);
            const float a0 = 1.0f + alpha;
            b0 = alpha / a0; b1 = 0.0f; b2 = -alpha / a0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
        }
        void lowpass (float f, float q)
        {
            const float w = kTwoPi * std::min (f, 20000.0f) / (float) kSr, alpha = std::sin (w) / (2.0f * q), c = std::cos (w);
            const float a0 = 1.0f + alpha;
            b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
        }
        void peak (float f, float q, float gainDb)
        {
            const float A = std::pow (10.0f, gainDb / 40.0f), w = kTwoPi * f / (float) kSr;
            const float alpha = std::sin (w) / (2.0f * q), c = std::cos (w), a0 = 1.0f + alpha / A;
            b0 = (1.0f + alpha * A) / a0; b1 = -2.0f * c / a0; b2 = (1.0f - alpha * A) / a0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha / A) / a0;
        }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct BlepSaw
    {
        float phase = 0.0f;
        float next (float inc)
        {
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            return 2.0f * phase - 1.0f - polyBlep (phase, inc);
        }
    };

    void normalise (juce::AudioBuffer<float>& b, float peakTarget = 0.7f)
    {
        const float mag = b.getMagnitude (0, b.getNumSamples());
        if (mag > 1.0e-6f) b.applyGain (peakTarget / mag);
    }

    void fadeEdges (juce::AudioBuffer<float>& b, int fadeIn, int fadeOut)
    {
        const int n = b.getNumSamples();
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < fadeIn && i < n; ++i) d[i] *= (float) i / (float) fadeIn;
            for (int i = 0; i < fadeOut && i < n; ++i) d[n - 1 - i] *= (float) i / (float) fadeOut;
        }
    }

    /** Crossfades the loop tail with the material before the loop start so the loop is seamless. */
    void makeSeamlessLoop (juce::AudioBuffer<float>& b, int loopStart, int loopEnd, int xfade)
    {
        xfade = std::min ({ xfade, loopStart, (loopEnd - loopStart) / 2 });
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int i = 0; i < xfade; ++i)
            {
                const float t = (float) (i + 1) / (float) (xfade + 1);
                const float g1 = std::sqrt (1.0f - t), g2 = std::sqrt (t);
                d[loopEnd - xfade + i] = d[loopEnd - xfade + i] * g1 + d[loopStart - xfade + i] * g2;
            }
        }
    }

    std::shared_ptr<SampleData> finish (juce::AudioBuffer<float> buf, const juce::String& name, const juce::String& id, int root,
                                        int loopStart = -1, int loopEnd = -1)
    {
        normalise (buf);
        auto s = SampleData::create (std::move (buf), kSr, name, id, root);
        if (loopStart >= 0 && loopEnd > loopStart)
        {
            s->hasLoop = true;
            s->loopStart = loopStart;
            s->loopEnd = loopEnd;
        }
        return s;
    }

    // ---------------------------------------------------------------------------------
    struct Formants { float f[4]; float bw[4]; float g[4]; };
    const Formants kAah { { 800.0f, 1150.0f, 2800.0f, 3500.0f }, { 80.0f, 90.0f, 120.0f, 130.0f }, { 1.0f, 0.5f, 0.25f, 0.12f } };
    const Formants kOoh { { 350.0f, 600.0f, 2400.0f, 3300.0f }, { 60.0f, 70.0f, 110.0f, 120.0f }, { 1.0f, 0.35f, 0.08f, 0.05f } };

    juce::AudioBuffer<float> renderChoir (int root, const Formants& fm, double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        Rng rng (seed);
        const int singers = 6;
        const float f0 = midiToHz ((float) root);
        for (int s = 0; s < singers; ++s)
        {
            BlepSaw saw;
            saw.phase = rng.uni();
            const float detune = rng.bi() * 0.006f;
            const float vibRate = 5.0f + rng.uni() * 1.2f;
            const float vibDepth = 0.004f + rng.uni() * 0.003f;
            float vibPhase = rng.uni();
            Biquad bp[4];
            for (int k = 0; k < 4; ++k)
            {
                // a little formant variation per singer
                bp[k].bandpass (fm.f[k] * (1.0f + rng.bi() * 0.04f), fm.f[k] / fm.bw[k]);
            }
            OnePoleLP tilt; tilt.setCutoff (1200.0f, kSr);
            const float panL = 0.5f + 0.4f * ((float) s / (singers - 1) - 0.5f), panR = 1.0f - panL;
            float jitter = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                vibPhase += vibRate / (float) kSr;
                if (vibPhase >= 1.0f) vibPhase -= 1.0f;
                jitter = jitter * 0.9995f + rng.bi() * 0.00005f;
                const float freq = f0 * (1.0f + detune + vibDepth * std::sin (kTwoPi * vibPhase) + jitter);
                const float src = tilt.process (saw.next (freq / (float) kSr)) + rng.bi() * 0.03f;
                float y = 0.0f;
                for (int k = 0; k < 4; ++k) y += bp[k].process (src) * fm.g[k];
                const float env = std::min (1.0f, (float) i / (float) (0.35 * kSr));
                out.addSample (0, i, y * env * panL);
                out.addSample (1, i, y * env * panR);
            }
        }
        return out;
    }

    juce::AudioBuffer<float> renderStrings (int root, double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        Rng rng (seed);
        const float f0 = midiToHz ((float) root);
        Biquad body[2][3];
        for (int c = 0; c < 2; ++c)
        {
            body[c][0].peak (280.0f, 1.2f, 6.0f);
            body[c][1].peak (1050.0f, 1.0f, 4.0f);
            body[c][2].peak (2900.0f, 1.4f, 3.0f);
        }
        Biquad lp[2]; lp[0].lowpass (6500.0f, 0.7f); lp[1].lowpass (6500.0f, 0.7f);
        const int players = 8;
        std::vector<BlepSaw> saws ((size_t) players);
        std::vector<float> det ((size_t) players), vr ((size_t) players), vp ((size_t) players), pan ((size_t) players);
        for (int p = 0; p < players; ++p)
        {
            saws[(size_t) p].phase = rng.uni();
            det[(size_t) p] = rng.bi() * 0.007f;
            vr[(size_t) p] = 4.6f + rng.uni() * 1.5f;
            vp[(size_t) p] = rng.uni();
            pan[(size_t) p] = (float) p / (players - 1);
        }
        Biquad bowNoise; bowNoise.bandpass (2500.0f, 0.8f);
        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (int p = 0; p < players; ++p)
            {
                vp[(size_t) p] += vr[(size_t) p] / (float) kSr;
                if (vp[(size_t) p] >= 1.0f) vp[(size_t) p] -= 1.0f;
                const float freq = f0 * (1.0f + det[(size_t) p] + 0.0035f * std::sin (kTwoPi * vp[(size_t) p]));
                const float s = saws[(size_t) p].next (freq / (float) kSr);
                l += s * (1.0f - pan[(size_t) p]);
                r += s * pan[(size_t) p];
            }
            const float noise = bowNoise.process (rng.bi()) * 0.15f;
            float yl = l + noise, yr = r + noise;
            for (int k = 0; k < 3; ++k) { yl = body[0][k].process (yl); yr = body[1][k].process (yr); }
            const float env = std::min (1.0f, (float) i / (float) (0.28 * kSr));
            out.setSample (0, i, lp[0].process (yl) * env);
            out.setSample (1, i, lp[1].process (yr) * env);
        }
        return out;
    }

    juce::AudioBuffer<float> renderBrass (int root, double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        Rng rng (seed);
        const float f0 = midiToHz ((float) root);
        const int players = 4;
        for (int p = 0; p < players; ++p)
        {
            BlepSaw saw; saw.phase = rng.uni();
            const float det = rng.bi() * 0.004f;
            Biquad lp1, lp2, form;
            form.peak (1250.0f, 1.3f, 7.0f);
            const float pan = (float) p / (players - 1);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) kSr;
                const float atk = std::min (1.0f, t / 0.07f);
                const float scoop = -0.012f * std::exp (-t / 0.05f); // slight pitch scoop into the note
                const float bright = 400.0f + f0 * (3.0f + 9.0f * atk) * (1.0f + 0.15f * std::sin (kTwoPi * 5.2f * t));
                lp1.lowpass (bright, 0.8f); lp2.lowpass (bright * 1.3f, 0.6f);
                const float s = saw.next (f0 * (1.0f + det + scoop) / (float) kSr);
                const float y = form.process (lp2.process (lp1.process (s))) * atk;
                out.addSample (0, i, y * (1.0f - pan));
                out.addSample (1, i, y * pan);
            }
        }
        return out;
    }

    juce::AudioBuffer<float> renderModal (int root, double seconds, const std::vector<float>& ratios, const std::vector<float>& amps,
                                          const std::vector<float>& decays, float detuneStrings, float thump, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        Rng rng (seed);
        const float f0 = midiToHz ((float) root);
        for (size_t k = 0; k < ratios.size(); ++k)
        {
            const float f = f0 * ratios[k];
            if (f > kSr * 0.45) continue;
            for (int str = 0; str < (detuneStrings > 0.0f ? 2 : 1); ++str)
            {
                const float fr = f * (1.0f + (str == 0 ? -1.0f : 1.0f) * detuneStrings);
                const float d = decays[k];
                const float ph0 = rng.uni() * kTwoPi;
                const float pan = 0.5f + 0.25f * rng.bi();
                const double w = kTwoPi * fr / kSr;
                const double cr = std::cos (w), sr = std::sin (w);
                double x = std::cos (ph0), y = std::sin (ph0);
                double amp = amps[k];
                const double decayMul = std::exp (-1.0 / (d * kSr));
                auto* l = out.getWritePointer (0);
                auto* r = out.getWritePointer (1);
                for (int i = 0; i < n; ++i)
                {
                    const float v = (float) (amp * y);
                    l[i] += v * (1.0f - pan);
                    r[i] += v * pan;
                    const double nx = x * cr - y * sr;
                    y = x * sr + y * cr;
                    x = nx;
                    amp *= decayMul;
                    if ((i & 4095) == 0) { const double m = 1.0 / std::sqrt (x * x + y * y); x *= m; y *= m; }
                }
            }
        }
        if (thump > 0.0f)
        {
            Biquad lp; lp.lowpass (f0 * 4.0f + 300.0f, 0.7f);
            for (int i = 0; i < std::min (n, (int) (0.02 * kSr)); ++i)
            {
                const float e = 1.0f - (float) i / (float) (0.02 * kSr);
                const float v = lp.process (rng.bi()) * thump * e;
                out.addSample (0, i, v); out.addSample (1, i, v);
            }
        }
        fadeEdges (out, 16, (int) (0.05 * kSr));
        return out;
    }

    juce::AudioBuffer<float> renderPiano (int root, uint32_t seed)
    {
        std::vector<float> ratios, amps, decays;
        const float B = 0.00035f * std::pow (2.0f, (float) (root - 60) / 24.0f);
        const float baseDecay = 4.5f * std::pow (2.0f, -(float) (root - 48) / 30.0f);
        for (int k = 1; k <= 28; ++k)
        {
            ratios.push_back ((float) k * std::sqrt (1.0f + B * (float) (k * k)));
            const float hammer = std::abs (std::sin (kPi * (float) k / 7.3f)) + 0.15f;
            amps.push_back (hammer / (float) k * std::exp (-(float) k / 10.0f));
            decays.push_back (baseDecay / (1.0f + 0.35f * (float) k));
        }
        return renderModal (root, 4.5, ratios, amps, decays, 0.0012f, 0.25f, seed);
    }

    juce::AudioBuffer<float> renderTine (int root, uint32_t seed)
    {
        const int n = (int) (4.0 * kSr);
        juce::AudioBuffer<float> out (2, n);
        Rng rng (seed);
        const float f0 = midiToHz ((float) root);
        float pc = 0.0f, pm = 0.0f, pb = 0.0f;
        const float trem = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) kSr;
            const float index = 0.4f + 2.6f * std::exp (-t / 0.18f);
            pm += f0 / (float) kSr; if (pm >= 1.0f) pm -= 1.0f;
            pc += f0 / (float) kSr; if (pc >= 1.0f) pc -= 1.0f;
            pb += f0 * 7.03f / (float) kSr; if (pb >= 1.0f) pb -= 1.0f;
            const float body = std::sin (kTwoPi * pc + index * std::sin (kTwoPi * pm));
            const float bell = 0.18f * std::exp (-t / 0.06f) * std::sin (kTwoPi * pb);
            const float env = std::exp (-t / (2.2f * std::pow (2.0f, -(float) (root - 60) / 36.0f)));
            const float v = (body + bell) * env * (1.0f + trem);
            out.setSample (0, i, v);
            out.setSample (1, i, v);
        }
        juce::ignoreUnused (rng);
        fadeEdges (out, 8, (int) (0.05 * kSr));
        return out;
    }

    juce::AudioBuffer<float> renderKarplus (int root, double seconds, float damping, float brightness, float subLevel, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        Rng rng (seed);
        const float f0 = midiToHz ((float) root);
        const float period = (float) kSr / f0;
        const int len = (int) period;
        const float frac = period - (float) len;
        std::vector<float> line ((size_t) len + 2);
        // excitation: filtered noise burst with pluck-position comb
        OnePoleLP ex; ex.setCutoff (1000.0f + 9000.0f * brightness, kSr);
        for (size_t i = 0; i < line.size(); ++i) line[i] = ex.process (rng.bi());
        const int pluckPos = std::max (1, len / 7);
        for (int i = len - 1; i >= pluckPos; --i) line[(size_t) i] -= line[(size_t) (i - pluckPos)] * 0.6f;

        // first-order allpass for the fractional delay
        const float apC = (1.0f - frac) / (1.0f + frac);
        float apX1 = 0.0f, apY1 = 0.0f, lpz = 0.0f;
        int idx = 0;
        float subPh = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float y = line[(size_t) idx];
            lpz = lpz + (y - lpz) * (0.5f + 0.45f * brightness);
            const float damped = lpz * damping;
            const float ap = apC * damped + apX1 - apC * apY1;
            apX1 = damped; apY1 = ap;
            line[(size_t) idx] = ap;
            idx = (idx + 1) % len;
            subPh += f0 / (float) kSr; if (subPh >= 1.0f) subPh -= 1.0f;
            const float t = (float) i / (float) kSr;
            const float v = y + subLevel * std::sin (kTwoPi * subPh) * std::exp (-t / 1.2f);
            out.setSample (0, i, v);
            out.setSample (1, i, v);
        }
        fadeEdges (out, 4, (int) (0.05 * kSr));
        return out;
    }

    // ---- single samples --------------------------------------------------------------
    juce::AudioBuffer<float> renderBreath (double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        Rng rng (seed);
        Biquad f[2][3];
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) kSr;
            const float sweep = 0.5f + 0.5f * std::sin (kTwoPi * 0.17f * t);
            if (i % 64 == 0)
                for (int c = 0; c < 2; ++c)
                {
                    f[c][0].bandpass (500.0f + 600.0f * sweep + 50.0f * (float) c, 3.0f);
                    f[c][1].bandpass (1500.0f + 900.0f * (1.0f - sweep), 4.0f);
                    f[c][2].bandpass (3200.0f + 400.0f * sweep, 5.0f);
                }
            for (int c = 0; c < 2; ++c)
            {
                const float x = rng.bi();
                const float y = f[c][0].process (x) + 0.7f * f[c][1].process (x) + 0.4f * f[c][2].process (x);
                out.setSample (c, i, y);
            }
        }
        fadeEdges (out, (int) (0.2 * kSr), (int) (0.2 * kSr));
        return out;
    }

    juce::AudioBuffer<float> renderDrone (double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        Rng rng (seed);
        const float f0 = midiToHz (48.0f);
        BlepSaw saws[6];
        float det[6];
        for (int k = 0; k < 6; ++k) { saws[k].phase = rng.uni(); det[k] = rng.bi() * 0.008f; }
        Biquad lp[2];
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) kSr;
            if (i % 32 == 0)
            {
                const float cut = 400.0f + 2600.0f * (0.5f + 0.5f * std::sin (kTwoPi * 0.11f * t));
                lp[0].lowpass (cut, 2.0f); lp[1].lowpass (cut * 1.05f, 2.0f);
            }
            float l = 0.0f, r = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                const float mult = k < 4 ? 1.0f : (k == 4 ? 1.5f : 2.0f);
                const float s = saws[k].next (f0 * mult * (1.0f + det[k]) / (float) kSr);
                if (k % 2 == 0) l += s; else r += s;
            }
            out.setSample (0, i, lp[0].process (l));
            out.setSample (1, i, lp[1].process (r));
        }
        fadeEdges (out, (int) (0.1 * kSr), (int) (0.2 * kSr));
        return out;
    }

    juce::AudioBuffer<float> renderTexture (double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        out.clear();
        Rng rng (seed);
        // cluster of slowly gliding sines plus sparse granular noise bursts
        for (int v = 0; v < 14; ++v)
        {
            const float base = midiToHz (55.0f + (float) ((v * 7) % 24) + rng.bi() * 0.3f);
            const float glide = rng.bi() * 0.02f;
            const float pan = rng.uni();
            const float amRate = 0.1f + rng.uni() * 0.4f;
            float ph = rng.uni();
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) kSr;
                ph += base * (1.0f + glide * t / (float) seconds) / (float) kSr;
                if (ph >= 1.0f) ph -= 1.0f;
                const float am = 0.5f + 0.5f * std::sin (kTwoPi * amRate * t + (float) v);
                const float s = std::sin (kTwoPi * ph) * am * 0.15f;
                out.addSample (0, i, s * (1.0f - pan));
                out.addSample (1, i, s * pan);
            }
        }
        Biquad bp; bp.bandpass (3000.0f, 2.0f);
        for (int g = 0; g < 120; ++g)
        {
            const int start = (int) (rng.uni() * (n - 4000));
            const int len = 400 + (int) (rng.uni() * 3000);
            const float pan = rng.uni();
            for (int i = 0; i < len; ++i)
            {
                const float w = std::sin (kPi * (float) i / (float) len);
                const float s = bp.process (rng.bi()) * w * w * 0.2f;
                out.addSample (0, start + i, s * (1.0f - pan));
                out.addSample (1, start + i, s * pan);
            }
        }
        fadeEdges (out, (int) (0.2 * kSr), (int) (0.3 * kSr));
        return out;
    }

    juce::AudioBuffer<float> renderVinyl (double seconds, uint32_t seed)
    {
        const int n = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, n);
        Rng rng (seed);
        OnePoleLP hiss[2]; hiss[0].setCutoff (6000.0f, kSr); hiss[1].setCutoff (6000.0f, kSr);
        float crackle = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (rng.uni() < 0.0006f) crackle = rng.bi() * (0.3f + 0.7f * rng.uni());
            crackle *= 0.82f;
            for (int c = 0; c < 2; ++c)
                out.setSample (c, i, hiss[c].process (rng.bi()) * 0.05f + crackle * (c == 0 ? 1.0f : 0.8f));
        }
        return out;
    }

    juce::AudioBuffer<float> renderMetal (uint32_t seed)
    {
        const std::vector<float> ratios { 1.0f, 1.41f, 2.27f, 2.93f, 3.71f, 4.53f, 5.88f, 6.92f };
        const std::vector<float> amps { 1.0f, 0.8f, 0.6f, 0.55f, 0.4f, 0.35f, 0.25f, 0.2f };
        const std::vector<float> decays { 2.5f, 2.0f, 1.6f, 1.3f, 1.0f, 0.8f, 0.6f, 0.5f };
        return renderModal (57, 3.0, ratios, amps, decays, 0.0f, 0.6f, seed);
    }

    juce::AudioBuffer<float> renderBowl (uint32_t seed)
    {
        const std::vector<float> ratios { 1.0f, 2.76f, 5.40f, 8.93f, 13.3f };
        const std::vector<float> amps { 1.0f, 0.6f, 0.35f, 0.2f, 0.1f };
        const std::vector<float> decays { 5.0f, 3.5f, 2.2f, 1.4f, 0.9f };
        return renderModal (64, 5.0, ratios, amps, decays, 0.0008f, 0.1f, seed);
    }

    juce::AudioBuffer<float> renderKalimba (int root, uint32_t seed)
    {
        const std::vector<float> ratios { 1.0f, 5.95f, 9.2f };
        const std::vector<float> amps { 1.0f, 0.25f, 0.08f };
        const std::vector<float> decays { 1.6f, 0.25f, 0.1f };
        return renderModal (root, 2.5, ratios, amps, decays, 0.0f, 0.3f, seed);
    }

    juce::AudioBuffer<float> renderMallet (int root, uint32_t seed)
    {
        const std::vector<float> ratios { 1.0f, 2.76f, 5.40f, 8.93f };
        const std::vector<float> amps { 1.0f, 0.45f, 0.2f, 0.08f };
        const float d = 2.2f * std::pow (2.0f, -(float) (root - 60) / 30.0f);
        const std::vector<float> decays { d, d * 0.45f, d * 0.25f, d * 0.15f };
        return renderModal (root, 3.0, ratios, amps, decays, 0.0006f, 0.4f, seed);
    }

    // ---------------------------------------------------------------------------------
    template <typename RenderFn>
    std::shared_ptr<MultiSample> buildMulti (const juce::String& name, int lowRoot, int highRoot, int step, RenderFn render, bool loop,
                                             double loopStartSec = 1.0, double loopEndSec = 2.6)
    {
        auto ms = std::make_shared<MultiSample>();
        ms->name = name;
        ms->sourceId = "builtin:" + name;
        std::vector<int> roots;
        for (int r = lowRoot; r <= highRoot; r += step) roots.push_back (r);
        for (size_t i = 0; i < roots.size(); ++i)
        {
            auto buf = render (roots[i], (uint32_t) (1000 + roots[i] * 7));
            int ls = -1, le = -1;
            if (loop)
            {
                ls = (int) (loopStartSec * kSr);
                le = std::min (buf.getNumSamples() - 1, (int) (loopEndSec * kSr));
                makeSeamlessLoop (buf, ls, le, (int) (0.12 * kSr));
            }
            auto s = finish (std::move (buf), name + " " + juce::String (roots[i]), "builtin:" + name + "/" + juce::String (roots[i]), roots[i], ls, le);
            SampleZone z;
            z.sample = s;
            z.rootNote = roots[i];
            z.loKey = i == 0 ? 0 : roots[i] - step / 2 + (step % 2 == 0 ? 0 : 0);
            z.hiKey = i + 1 == roots.size() ? 127 : roots[i] + (step - 1) / 2 + (step % 2 == 0 ? 0 : 0);
            if (i > 0) z.loKey = ms->zones.back().hiKey + 1;
            z.loopMode = loop ? 1 : 0;
            ms->zones.push_back (z);
        }
        return ms;
    }
}

juce::StringArray singleNames()
{
    return { "Breath Pad", "Analog Drone", "Granular Texture", "Vocal Aah", "Plucked String", "Metal Hit", "Glass Bowl",
             "Kalimba", "Felt Piano C4", "Vinyl Noise" };
}

juce::StringArray multiNames()
{
    return { "Choir Aah", "Choir Ooh", "Ensemble Strings", "Brass Section", "Felt Piano", "Tine Piano", "Glass Mallet",
             "Nylon Pluck", "Warm Bass" };
}

std::shared_ptr<SampleData> renderSingle (const juce::String& name)
{
    const auto id = "builtin:" + name;
    if (name == "Breath Pad") { auto b = renderBreath (6.0, 11); const int ls = (int) (1.0 * kSr), le = (int) (5.0 * kSr); makeSeamlessLoop (b, ls, le, (int) (0.3 * kSr)); return finish (std::move (b), name, id, 60, ls, le); }
    if (name == "Analog Drone") { auto b = renderDrone (8.0, 12); const int ls = (int) (1.0 * kSr), le = (int) (7.0 * kSr); makeSeamlessLoop (b, ls, le, (int) (0.4 * kSr)); return finish (std::move (b), name, id, 48, ls, le); }
    if (name == "Granular Texture") return finish (renderTexture (8.0, 13), name, id, 60);
    if (name == "Vocal Aah") { auto b = renderChoir (60, kAah, 4.0, 14); const int ls = (int) (1.0 * kSr), le = (int) (3.5 * kSr); makeSeamlessLoop (b, ls, le, (int) (0.15 * kSr)); return finish (std::move (b), name, id, 60, ls, le); }
    if (name == "Plucked String") return finish (renderKarplus (48, 3.0, 0.996f, 0.7f, 0.0f, 15), name, id, 48);
    if (name == "Metal Hit") return finish (renderMetal (16), name, id, 57);
    if (name == "Glass Bowl") return finish (renderBowl (17), name, id, 64);
    if (name == "Kalimba") return finish (renderKalimba (64, 18), name, id, 64);
    if (name == "Felt Piano C4") return finish (renderPiano (60, 19), name, id, 60);
    if (name == "Vinyl Noise") { auto b = renderVinyl (4.0, 20); makeSeamlessLoop (b, (int) (0.5 * kSr), (int) (3.5 * kSr), (int) (0.1 * kSr)); return finish (std::move (b), name, id, 60, (int) (0.5 * kSr), (int) (3.5 * kSr)); }
    return nullptr;
}

std::shared_ptr<MultiSample> renderMulti (const juce::String& name)
{
    if (name == "Choir Aah") return buildMulti (name, 36, 84, 6, [] (int r, uint32_t s) { return renderChoir (r, kAah, 3.0, s); }, true);
    if (name == "Choir Ooh") return buildMulti (name, 36, 84, 6, [] (int r, uint32_t s) { return renderChoir (r, kOoh, 3.0, s); }, true);
    if (name == "Ensemble Strings") return buildMulti (name, 28, 96, 6, [] (int r, uint32_t s) { return renderStrings (r, 3.0, s); }, true);
    if (name == "Brass Section") return buildMulti (name, 34, 82, 6, [] (int r, uint32_t s) { return renderBrass (r, 3.0, s); }, true);
    if (name == "Felt Piano") return buildMulti (name, 24, 108, 6, [] (int r, uint32_t s) { return renderPiano (r, s); }, false);
    if (name == "Tine Piano") return buildMulti (name, 28, 100, 6, [] (int r, uint32_t s) { return renderTine (r, s); }, false);
    if (name == "Glass Mallet") return buildMulti (name, 48, 108, 6, [] (int r, uint32_t s) { return renderMallet (r, s); }, false);
    if (name == "Nylon Pluck") return buildMulti (name, 40, 88, 6, [] (int r, uint32_t s) { return renderKarplus (r, 3.0, 0.995f, 0.55f, 0.0f, s); }, false);
    if (name == "Warm Bass") return buildMulti (name, 24, 60, 6, [] (int r, uint32_t s) { return renderKarplus (r, 3.0, 0.998f, 0.35f, 0.4f, s); }, false);
    return nullptr;
}

} // namespace vsx::BuiltinSamples
