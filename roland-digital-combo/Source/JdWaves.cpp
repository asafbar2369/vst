#include "JdWaves.h"

namespace dc
{
namespace
{
    using R = JdWaveRecipe;

    // Inharmonic / organ partial sets: ratios and amplitudes (estimated, not measured).
    struct PartialSet { float ratio[6]; float amp[6]; };
    const PartialSet kSets[] = {
        { { 1, 2.004f, 3.009f, 4.016f, 5.025f, 6.036f }, { 1, .5f, .33f, .25f, .2f, .16f } }, //  0 Wire Str (stiff string)
        { { 1, 1.0023f, 2.01f, 3.0f }, { 1, .9f, .25f, .08f } },                               //  1 Fine Wine (glass)
        { { 1, 6.27f, 17.6f }, { 1, .35f, .1f } },                                             //  2 Kalimba
        { { 1, 3.0f, 6.2f, 9.9f }, { 1, .45f, .2f, .08f } },                                   //  3 Xylo
        { { 1, 3.93f, 9.2f }, { 1, .25f, .06f } },                                             //  4 Marimba
        { { 1, 1.52f, 2.9f, 4.1f }, { 1, .6f, .3f, .15f } },                                   //  5 Log drum
        { { 1, 2.4f, 3.9f, 5.4f }, { 1, .6f, .3f, .2f } },                                     //  6 Agogo
        { { 1, 2.8f, 4.9f, 7.3f }, { 1, .4f, .2f, .1f } },                                     //  7 Bottle
        { { 1, 2.76f, 5.4f, 8.93f }, { 1, .5f, .3f, .2f } },                                   //  8 Gamelan 1
        { { 1, 2.2f, 3.6f, 5.8f }, { 1, .6f, .4f, .2f } },                                     //  9 Gamelan 2
        { { 1, 1.98f, 3.02f, 4.1f }, { 1, .4f, .2f, .1f } },                                   // 10 Gamelan 3
        { { 1, 2.0f, 3.0f, 4.03f, 5.1f }, { 1, .7f, .4f, .2f, .1f } },                         // 11 Tabla
        { { 1, 1.37f, 2.7f, 4.6f, 6.2f }, { 1, .7f, .5f, .3f, .2f } },                         // 12 Pole
        { { 0.5f, 1, 2, 3, 4 }, { .8f, 1, .6f, .4f, .3f } },                                   // 13 Full organ (16' doubled)
        { { 0.5f, 1, 1.5f, 2, 3, 4 }, { .8f, 1, .7f, .8f, .6f, .5f } },                        // 14 Full drawbars
        { { 1, 1.01f, 2.03f, 3.1f, 4.2f }, { 1, 1, .5f, .3f, .2f } },                          // 15 Anklungs
        { { 1, 1.6f, 2.3f, 3.7f, 4.4f, 5.9f }, { 1, .8f, .7f, .5f, .4f, .3f } },               // 16 Wind chime
        { { 1, 1.41f, 2.13f, 3.6f }, { 1, .8f, .6f, .4f } },                                   // 17 Metal wind
        { { 1, 2.4f, 3.9f, 5.4f }, { 1, .6f, .3f, .2f } },                                     // 18 Windago
    };

    R tab (float slope, float odd = 0, float formant = 0, float gain = 1, int seed = 0) { return { R::Table, slope, odd, formant, gain, seed }; }
    R vox (float noise, float formant, float gain) { return { R::VoxNoise, noise, 0, formant, gain, 0 }; }
    R inh (int set, int n) { return { R::Inharmonic, (float) set, (float) n, 0, 0, 0 }; }
    R fm (float ratio, float index, float tine = 0) { return { R::Fm, ratio, index, 0, tine, 0 }; }
    R shot (float tonal, float centre, float decayMs, float inharm, int seed) { return { R::OneShot, tonal, centre, decayMs, inharm, seed }; }

    const R kRecipes[jd800::kNumWaveforms] = {
        { R::Saw },                 tab (0.85f, 0, 6, 2.5f),    { R::SuperSaw, 0.12f, 3 }, { R::Square, 0.1f },     //   1-  4
        { R::Pulse, 0.5f },         { R::Pulse, 0.3f },         { R::Pulse, 0.2f },        { R::Pulse, 0.14f },     //   5-  8
        { R::Pulse, 0.07f },        { R::Pwm, 0.8f, 0.35f },    { R::Triangle },           { R::Sine },             //   9- 12
        tab (1.8f, 0, 3, 1.5f),     inh (0, 6),                 tab (0.7f, 0.3f, 12, 2.5f), vox (0.1f, 4, 4),       //  13- 16
        vox (0.15f, 7, 3),          tab (0.9f, 0.6f, 4, 2),     tab (0.6f, 0.2f, 10, 2),   tab (1.0f, 0.1f, 3, 2.5f), // 17- 20
        tab (0.4f, 0, 0, 1, 21),    tab (0.5f, 0.5f, 9, 3, 22), tab (1.6f, 0, 2, 2),       tab (1.4f, 0.8f, 3, 1.5f), // 21- 24
        vox (0.2f, 5, 3),           tab (0.9f, 0, 0, 1, 26),    tab (1.2f, 0.3f, 4, 2, 27), tab (0.3f, 0, 16, 2, 28), // 25- 28
        tab (0.8f, 0.4f, 9, 3, 29), tab (1.0f, 0.6f, 5, 2, 30), { R::Fifth, 0.8f },        tab (0.9f, 0.2f, 6, 2, 32), // 29- 32
        tab (0.5f, 0.4f, 3, 3, 33), tab (1.5f, 0, 3, 1.5f, 34), inh (1, 4),                tab (1.1f, 0, 3, 2.5f),  //  33- 36
        tab (0.9f, 0, 6, 2),        tab (1.0f, 0.2f, 4, 2, 38), tab (3.0f, 0, 2, 2),       { R::Organ, 13, 5 },     //  37- 40
        { R::Organ, 14, 6 },        tab (1.3f, 0.5f, 2, 2),     vox (0.55f, 6, 3),         vox (0.3f, 3, 2),        //  41- 44
        vox (0.25f, 2, 3),          inh (2, 3),                 inh (3, 4),                inh (4, 3),              //  45- 48
        inh (5, 4),                 inh (6, 4),                 inh (7, 4),                inh (8, 4),              //  49- 52
        inh (9, 4),                 inh (10, 4),                inh (11, 5),               inh (12, 5),             //  53- 56
        tab (1.4f),                 tab (1.3f, 0, 3, 1.8f),     tab (0.9f, 0, 5, 1.5f, 59), tab (1.8f),             //  57- 60
        tab (0.6f, 0, 8, 2, 61),    fm (3.5f, 2.0f),            fm (2.8f, 3.0f),           fm (4.1f, 1.5f),         //  61- 64
        fm (5.2f, 2.5f),            fm (1.4f, 1.2f),            fm (3.0f, 2.0f, 0.3f),     { R::SawNoise, 0.4f },   //  65- 68
        tab (0.8f, 0.1f, 7, 2, 69), tab (1.2f, 0, 2, 2.5f),     fm (1.0f, 1.5f, 0.1f),     fm (1.0f, 2.2f, 0.25f),  //  69- 72
        tab (2.0f, 0.3f, 4, 1.5f),  tab (0.7f, 0.3f, 3, 2, 74), shot (0.2f, 1500, 80, 0, 75),  shot (0.8f, 2500, 300, 0, 76), // 73-76
        shot (0.1f, 2000, 30, 0, 77),    shot (0.8f, 1200, 120, 0.05f, 78), shot (0.6f, 1500, 60, 0.02f, 79), shot (0.7f, 600, 50, 0, 80), // 77-80
        shot (0.7f, 3000, 40, 0, 81),    shot (0.85f, 800, 300, 0.4f, 82),   shot (0.7f, 1800, 80, 0.3f, 83),  shot (0.9f, 900, 500, 0.2f, 84), // 81-84
        shot (0.9f, 2000, 200, 0, 85),   shot (0.0f, 4000, 10, 0, 86),       shot (0.6f, 400, 60, 0, 87),      shot (0.8f, 5000, 40, 0.3f, 88), // 85-88
        shot (0.85f, 2500, 70, 0.15f, 89), shot (0.85f, 2000, 150, 0.01f, 90), shot (-1.0f, 0, 1, 0, 91),      { R::SineNoise, 0.08f, 2 }, // 89-92
        { R::SineNoise, 0.25f, 1 },      { R::SineNoise, 0.4f, 1 },          { R::SineNoise, 0.5f, 2 },        tab (1.4f, 0, 4, 2), // 93-96
        tab (1.8f, 0, 3, 2),             { R::Noise, 0, 0, 0, 0 },           { R::Noise, 1, 0, 0, 0 },         { R::Noise, 0, 1, 8, 0 }, // 97-100
        { R::Noise, 0, 3, 1.5f, 0 },     { R::Noise, 0, 2, 5, 0 },           { R::Noise, 0, 0, 1.2f, 3000 },   { R::Noise, 1, 0, 2, 700 }, // 101-104
        { R::MetalNoise, 17, 4, 0.5f },  { R::MetalNoise, 18, 4, 0.4f },     inh (15, 5),                      inh (16, 6), // 105-108
    };

    std::vector<float> makeOneShot (const R& r)
    {
        Rng rng;
        rng.state = 0x2545F491u * uint32_t (r.seed + 7);
        auto uni = [&rng] { return 0.5f * (rng.next() + 1.0f); };
        const float rate = 32000.0f;
        if (r.a < 0.0f) // TVF_Trig: impulse
        {
            std::vector<float> d (64, 0.0f);
            d[0] = 1.0f; d[1] = 0.5f; d[2] = 0.2f;
            return d;
        }
        const int len = std::max (64, int (rate * r.c * 0.006f)); // ~6 time constants
        std::vector<float> d ((size_t) len, 0.0f);
        Svf bp;
        bp.set (r.b, 2.5f, rate);
        const float tonal = clampf (r.a, 0.0f, 1.0f);
        const float decay = 1000.0f / std::max (1.0f, r.c);
        double ph[6] {};
        float peak = 1.0e-6f;
        for (int i = 0; i < len; ++i)
        {
            const float t = float (i) / rate;
            float l, b, h;
            bp.process (rng.next(), l, b, h);
            float s = (1.0f - tonal) * b * std::exp (-decay * 1.5f * t);
            for (int k = 0; k < 6; ++k)
            {
                const float hn = float (k + 1);
                const float ratio = hn * (1.0f + r.d * hn * hn * 0.05f);
                ph[k] += double (261.6256f * ratio) / rate;
                s += tonal * std::exp (-decay * (1.0f + 0.5f * hn) * t) / hn * float (std::sin (6.283185307179586 * ph[k]));
            }
            const float fadeOut = std::min (1.0f, float (len - i) / 64.0f);
            d[(size_t) i] = s * fadeOut;
            peak = std::max (peak, std::abs (s));
        }
        for (auto& x : d) x *= 0.9f / peak;
        (void) uni;
        return d;
    }
} // namespace

const JdWaveRecipe& jdWaveRecipe (int i) noexcept
{
    return kRecipes[i < 0 ? 0 : (i >= jd800::kNumWaveforms ? jd800::kNumWaveforms - 1 : i)];
}

JdWaveData::JdWaveData()
{
    sineTable.resize (kTableSize + 1);
    for (int i = 0; i <= kTableSize; ++i)
        sineTable[(size_t) i] = (float) std::sin (6.283185307179586 * double (i) / double (kTableSize));

    tables.assign ((size_t) (jd800::kNumWaveforms * kNumMips), {});
    oneShots.assign ((size_t) jd800::kNumWaveforms, {});
    std::vector<double> acc ((size_t) kTableSize);

    for (int w = 0; w < jd800::kNumWaveforms; ++w)
    {
        const auto& r = kRecipes[w];
        if (r.kind == R::OneShot)
        {
            oneShots[(size_t) w].data = makeOneShot (r);
            continue;
        }
        if (r.kind != R::Table && r.kind != R::VoxNoise)
            continue;

        const float slope = r.kind == R::VoxNoise ? 1.2f : r.a;
        const float odd = r.kind == R::VoxNoise ? 0.0f : r.b;
        Rng rng;
        rng.state = 0x9E3779B9u * uint32_t (r.seed + 1);
        std::fill (acc.begin(), acc.end(), 0.0);
        std::vector<std::vector<float>> mips ((size_t) kNumMips);
        for (int h = 1; h <= 512; ++h)
        {
            double amp = std::pow (double (h), -double (slope));
            if (h % 2 == 0) amp *= 1.0 - odd;
            if (r.c > 0.0f)
            {
                const double x = (double (h) - r.c) / (0.3 * r.c + 1.0);
                amp *= 1.0 + (double (r.d) - 1.0) * std::exp (-x * x);
            }
            if (r.seed > 0) amp *= 0.4 + 1.2 * 0.5 * (rng.next() + 1.0f);
            const int phaseIdx = r.seed > 0 ? int (0.5f * (rng.next() + 1.0f) * float (kTableSize)) : 0;
            if (amp > 1.0e-7)
                for (int i = 0; i < kTableSize; ++i)
                    acc[(size_t) i] += amp * sineTable[(size_t) ((h * i + phaseIdx) & (kTableSize - 1))];
            if ((h & (h - 1)) == 0) // power of two: snapshot mip holding harmonics 1..h
            {
                const int mip = 9 - int (std::log2 (double (h)) + 0.5);
                auto& t = mips[(size_t) mip];
                t.resize ((size_t) kTableSize + 1);
                for (int i = 0; i < kTableSize; ++i) t[(size_t) i] = (float) acc[(size_t) i];
                t[(size_t) kTableSize] = t[0];
            }
        }
        float peak = 1.0e-6f;
        for (float x : mips[0]) peak = std::max (peak, std::abs (x));
        for (int m = 0; m < kNumMips; ++m)
        {
            for (auto& x : mips[(size_t) m]) x *= 0.9f / peak;
            tables[(size_t) (w * kNumMips + m)] = std::move (mips[(size_t) m]);
        }
    }
}

const JdWaveData& JdWaveData::get()
{
    static const JdWaveData data;
    return data;
}

const float* JdWaveData::table (int wave, int mip) const noexcept
{
    const auto& t = tables[(size_t) (wave * kNumMips + mip)];
    return t.empty() ? nullptr : t.data();
}

float JdWaveData::sine (float p) const noexcept
{
    p -= std::floor (p);
    const float x = p * float (kTableSize);
    const int i = int (x);
    const float f = x - float (i);
    return sineTable[(size_t) i] + f * (sineTable[(size_t) i + 1] - sineTable[(size_t) i]);
}

// ---------------------------------------------------------------------------
void JdOscillator::reset (int wave, uint32_t seed) noexcept
{
    for (int k = 0; k < 8; ++k) phase[k] = float (k) * 0.137f;
    aux = auxPhase = 0.0f;
    fmEnv = 1.0f;
    pink[0] = pink[1] = pink[2] = 0.0f;
    band.reset();
    rng.state = 0x5bd1e995u ^ seed;
    pos = 0.0f;
    (void) wave;
}

float JdOscillator::process (int wave, float hz, const SampleWave* user) noexcept
{
    if (user != nullptr)
        return user->read (pos, (hz / user->rootHz) * (user->rate / sampleRate));
    const auto& data = JdWaveData::get();
    const auto& r = jdWaveRecipe (wave);
    const float dt = std::min (0.45f, hz / sampleRate);
    auto advance = [] (float& p, float inc) { p += inc; if (p >= 1.0f) p -= std::floor (p); };
    auto saw = [] (float p, float d) { return 2.0f * p - 1.0f - polyBlep (p, d); };
    auto pulse = [] (float p, float d, float duty)
    {
        float x = p < duty ? 1.0f : -1.0f;
        x += polyBlep (p, d);
        float p2 = p + 1.0f - duty;
        p2 -= std::floor (p2);
        return x - polyBlep (p2, d) - (2.0f * duty - 1.0f); // remove the duty-cycle DC component
    };
    float& p = phase[0];
    float x = 0.0f;

    switch (r.kind)
    {
        case R::Saw:      x = saw (p, dt); break;
        case R::Square:
            x = pulse (p, dt, 0.5f);
            if (r.a > 0.0f)
            {
                const float d2 = std::min (0.45f, dt * std::exp2 (r.a / 12.0f));
                x = 0.6f * (x + pulse (phase[1], d2, 0.5f));
                advance (phase[1], d2);
            }
            break;
        case R::Pulse:    x = pulse (p, dt, r.a); break;
        case R::Pwm:
            advance (auxPhase, r.a / sampleRate);
            x = pulse (p, dt, 0.5f + r.b * data.sine (auxPhase));
            break;
        case R::Triangle: x = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
        case R::Sine:     x = data.sine (p); break;
        case R::SuperSaw:
        {
            const int voices = std::max (1, (int) r.b);
            for (int k = 0; k < voices; ++k)
            {
                const float det = (float (k) - float (voices - 1) * 0.5f) * r.a;
                const float d = std::min (0.45f, dt * std::exp2 (det / 12.0f));
                x += saw (phase[k], d);
                if (k > 0) advance (phase[k], d); // phase[0] is advanced at the end with dt
            }
            x *= 1.6f / float (voices);
            break;
        }
        case R::Table:
        case R::VoxNoise:
        {
            const float maxHarm = 0.45f / std::max (1.0e-6f, dt);
            int mip = 0;
            while (mip < JdWaveData::kNumMips - 1 && float (512 >> mip) > maxHarm) ++mip;
            if (const float* t = data.table (wave, mip))
            {
                const float pos2 = p * float (JdWaveData::kTableSize);
                const int i = int (pos2);
                const float f = pos2 - float (i);
                x = t[i] + f * (t[i + 1] - t[i]);
            }
            if (r.kind == R::VoxNoise)
            {
                band.set (clampf (hz * r.c, 100.0f, sampleRate * 0.4f), 3.0f, sampleRate);
                float l, b, h;
                band.process (rng.next(), l, b, h);
                x = x * (1.0f - r.a) + 2.0f * b * r.a;
            }
            break;
        }
        case R::Fifth:
        {
            const float d2 = std::min (0.45f, dt * 1.4983f);
            x = 0.6f * (saw (p, dt) + r.a * saw (phase[1], d2));
            advance (phase[1], d2);
            break;
        }
        case R::Organ:
        case R::Inharmonic:
        case R::MetalNoise:
        {
            const auto& set = kSets[(size_t) r.a];
            const int n = (int) r.b;
            float sum = 0, norm = 0;
            for (int k = 0; k < n; ++k)
            {
                const float d = std::min (0.45f, dt * set.ratio[k]);
                if (d < 0.45f) sum += set.amp[k] * data.sine (phase[k + 1]);
                norm += set.amp[k];
                advance (phase[k + 1], d);
            }
            x = sum / std::max (1.0f, norm * 0.7f);
            if (r.kind == R::MetalNoise)
                x = x * (1.0f - r.c) + r.c * rng.next() * (0.5f + 0.5f * std::abs (x));
            break;
        }
        case R::Fm:
        {
            const float dm = std::min (0.45f, dt * r.a);
            advance (auxPhase, dm);
            float mod = r.b * fmEnv * data.sine (auxPhase);
            if (r.d > 0.0f)
            {
                const float dtine = std::min (0.45f, dt * 14.0f);
                advance (phase[1], dtine);
                mod += r.d * data.sine (phase[1]);
            }
            x = data.sine (p + mod * 0.15915494f);
            break;
        }
        case R::SawNoise:
            x = saw (p, dt) * (1.0f - r.a) + rng.next() * r.a;
            break;
        case R::SineNoise:
        {
            band.set (clampf (hz * r.b, 100.0f, sampleRate * 0.4f), 2.0f, sampleRate);
            float l, b, h;
            band.process (rng.next(), l, b, h);
            x = data.sine (p) * (1.0f - r.a) + 2.5f * b * r.a;
            break;
        }
        case R::Noise:
        {
            float n = rng.next();
            if (r.a > 0.5f) // pink (3-pole approximation)
            {
                pink[0] = 0.99765f * pink[0] + n * 0.0990460f;
                pink[1] = 0.96300f * pink[1] + n * 0.2965164f;
                pink[2] = 0.57000f * pink[2] + n * 1.0526913f;
                n = 0.25f * (pink[0] + pink[1] + pink[2] + n * 0.1848f);
            }
            if (r.b > 0.0f || r.d > 0.0f)
            {
                const float centre = r.d > 0.0f ? r.d : hz * r.b;
                band.set (clampf (centre, 40.0f, sampleRate * 0.4f), std::max (0.5f, r.c), sampleRate);
                float l, b, h;
                band.process (n, l, b, h);
                n = b * std::sqrt (std::max (0.5f, r.c)) * 1.5f;
            }
            x = n;
            break;
        }
        case R::OneShot:
        {
            const auto& os = data.oneShot (wave);
            const int len = (int) os.data.size();
            if (len > 1 && pos < float (len - 1))
            {
                const int i0 = int (pos);
                const float f = pos - float (i0);
                x = os.data[(size_t) i0] + f * (os.data[(size_t) i0 + 1] - os.data[(size_t) i0]);
                pos += (hz / os.rootHz) * (os.rate / sampleRate);
            }
            break;
        }
    }
    advance (p, dt);
    return x;
}
} // namespace dc
