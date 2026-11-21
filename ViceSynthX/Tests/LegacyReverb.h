#pragma once

#include "../Source/FX/ReverbEngine.h"

namespace vsx
{
/** The reverb as shipped before 2026-11-21 (8-line FDN), kept only for measured before / after comparisons. */
    /** 8-line feedback delay network with input diffusion, modulation and frequency-dependent decay. */
    class LegacyReverb
    {
    public:
        
        void prepare (double s)
        {
            sr = s;
            const double scale = sr / 48000.0;
            for (int i = 0; i < 8; ++i) lines[(size_t) i].prepare ((int) (kBase[i] * 1.6 * scale) + 400);
            for (int i = 0; i < 8; ++i) diff[(size_t) i].prepare ((int) (kDiff[i % 4] * scale) + 8);
            pre.prepare ((int) (0.3 * sr));
            reset();
        }
        void reset()
        {
            for (auto& l : lines) l.reset();
            for (auto& d : diff) d.reset();
            pre.reset();
            for (auto& d : damp) d.reset();
            for (auto& h : lowcut) h.reset();
            modPhase = 0.0;
        }
        void process (float* l, float* r, int n, const ReverbEngine::Settings& st) noexcept
        {
            const float size = 0.3f + 1.0f * st.size;
            const float rt60 = st.decay;
            const float preMs = st.predelayMs;
            const float dampHz = 20000.0f * std::pow (0.07f, st.damping);
            const float diffusion = st.diffusion * 0.7f;
            const float modDepth = st.modulation * 0.0015f * (float) sr;
            const float width = st.width;
            for (auto& h : lowcut) h.setCutoff (st.lowCut, sr);
            const double scale = sr / 48000.0;
            float len[8], g[8];
            for (int i = 0; i < 8; ++i)
            {
                len[i] = (float) (kBase[i] * size * scale);
                g[i] = std::pow (10.0f, -3.0f * len[i] / (rt60 * (float) sr));
                damp[(size_t) i].setCutoff (dampHz, sr);
            }
            const float preSamples = std::max (1.0f, preMs * 0.001f * (float) sr);
            for (int i = 0; i < n; ++i)
            {
                modPhase += 0.37 / sr; modPhase -= std::floor (modPhase);
                pre.push (0.5f * (l[i] + r[i]));
                float x = pre.read (preSamples);
                float xl = x + 0.15f * (l[i] - r[i]), xr = x - 0.15f * (l[i] - r[i]);
                // input diffusion: 4 all-passes per side
                for (int k = 0; k < 4; ++k)
                {
                    xl = allpass (diff[(size_t) k], xl, (int) (kDiff[k] * scale), diffusion);
                    xr = allpass (diff[(size_t) k + 4], xr, (int) (kDiff[k] * scale * 1.07), diffusion);
                }
                float y[8];
                for (int k = 0; k < 8; ++k)
                {
                    const float m = modDepth * std::sin (kTwoPi * (float) (modPhase + k * 0.125));
                    y[k] = damp[(size_t) k].process (lines[(size_t) k].read (len[k] + m)) * g[k];
                }
                // Hadamard 8x8 mixing (normalised)
                float h[8];
                hadamard8 (y, h);
                for (int k = 0; k < 8; ++k)
                    lines[(size_t) k].push (flushDenormal (h[k] + ((k & 1) ? xr : xl)));
                float ol = (y[0] + y[2] + y[4] + y[6]) * 0.5f, orr = (y[1] + y[3] + y[5] + y[7]) * 0.5f;
                ol = ol - lowcut[0].lp.process (ol);
                orr = orr - lowcut[1].lp.process (orr);
                const float mid = 0.5f * (ol + orr), side = 0.5f * (ol - orr) * width;
                l[i] = mid + side;
                r[i] = mid - side;
            }
        }
    private:
        double sr = 48000.0;
        static constexpr int kBase[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        static constexpr int kDiff[4] = { 142, 107, 379, 277 };
        static float allpass (DelayLine& d, float x, int len, float gcoef) noexcept
        {
            const float del = d.readInt (std::max (1, len));
            const float v = x + gcoef * del;
            d.push (v);
            return del - gcoef * v;
        }
        static void hadamard8 (const float* in, float* out) noexcept
        {
            float a[8];
            for (int i = 0; i < 8; ++i) a[i] = in[i];
            for (int h = 1; h < 8; h <<= 1)
                for (int i = 0; i < 8; i += h << 1)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float x = a[j], y = a[j + h];
                        a[j] = x + y; a[j + h] = x - y;
                    }
            const float norm = 0.35355339f;
            for (int i = 0; i < 8; ++i) out[i] = a[i] * norm;
        }
        std::array<DelayLine, 8> lines, diff;
        DelayLine pre;
        std::array<OnePoleLP, 8> damp;
        OnePoleHP lowcut[2];
        double modPhase = 0.0;
    };

} // namespace vsx
