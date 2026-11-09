#include "D50Engine.h"

namespace dc
{
namespace
{
    // Structure No. 1..7 (value 0..6): which partials are synth (S) or PCM (P) and whether
    // partial 2 is replaced by the ring-modulated product of both waveform generators.
    struct StructureInfo { bool p1Synth, p2Synth, ring; };
    constexpr StructureInfo kStructures[7] = {
        { true,  true,  false }, // 1: S + S
        { true,  true,  true  }, // 2: S + R(S x S)
        { false, true,  false }, // 3: P + S
        { false, true,  true  }, // 4: P + R(P x S)
        { true,  false, true  }, // 5: S + R(S x P)
        { false, false, false }, // 6: P + P
        { false, false, true  }, // 7: P + R(P x P)
    };

    // Chorus Type 1..8 (value 0..7): original settings, not measured from hardware.
    struct ChorusShape { float baseMs, depthMs, feedback, tremolo; };
    constexpr ChorusShape kChorusShapes[8] = {
        { 7.0f, 3.0f, 0.0f, 0.0f },  // chorus 1
        { 12.0f, 5.0f, 0.0f, 0.0f }, // chorus 2
        { 2.0f, 2.0f, 0.5f, 0.0f },  // flanger 1
        { 1.5f, 3.0f, 0.7f, 0.0f },  // flanger 2
        { 8.0f, 4.0f, 0.4f, 0.0f },  // feedback chorus
        { 5.0f, 0.0f, 0.0f, 1.0f },  // tremolo
        { 7.0f, 3.0f, 0.0f, 0.6f },  // chorus + tremolo
        { 10.0f, 1.0f, 0.0f, 0.0f }, // dimension-like
    };

    void reverbSettings (int type, float& size, float& decay, float& damp, float& pre) noexcept
    {
        // Reverb Type 1..32 (value 0..31). Our own spread of rooms/halls/plates; not Roland's programs.
        const int family = (type / 4) % 4;   // room, hall, plate, special
        const int variant = type % 4;
        const bool card = type >= 16;        // types 17..32 lived on a card on the original
        size  = 0.25f + 0.25f * float (family) * 0.7f + 0.12f * float (variant);
        decay = 0.5f + 0.7f * float (variant) + 0.8f * float (family) + (card ? 0.8f : 0.0f);
        damp  = family == 2 ? 9000.0f : 5000.0f - 600.0f * float (variant);
        pre   = family == 3 ? 40.0f : 5.0f + 6.0f * float (variant);
    }

    // Bias Point/Dir 0..127: 0-63 = "<A1 .. <C7" (keys below the point), 64-127 = ">A1 .. >C7" (above).
    inline float octavesBeyondBias (int biasPoint, float note) noexcept
    {
        const bool below = biasPoint < 64;
        const float point = 33.0f + float (biasPoint & 63); // A1 (MIDI 33) .. C7 (MIDI 96): 64 steps
        const float d = below ? point - note : note - point;
        return d > 0.0f ? d / 12.0f : 0.0f;
    }

    // Keyfollow 0..4 for envelope times / depth: 0 = none, 4 = halves per octave above C4 (estimate).
    inline float keyScale (int kf0to4, float note) noexcept
    {
        return std::exp2 (-(note - 60.0f) / 12.0f * float (kf0to4) * 0.25f);
    }

    inline float lfoSelect (int sel, const float* lv) noexcept
    {
        // (+1, -1, +2, -2, +3, -3)
        const int idx = std::min (2, sel / 2);
        return (sel % 2 == 0 ? 1.0f : -1.0f) * lv[idx];
    }

    void decodePartial (const uint8_t* b, D50PartialParams& p) noexcept
    {
        using namespace d50::partial;
        auto v = [b] (int o) { return (int) std::min<uint8_t> (b[o], d50::partialParam (o).maxValue); };
        p.waveform = v (Waveform);
        p.pcmWave = v (PcmWave);
        p.coarseSemis = float (v (Coarse) - 36); // 36 = C4 (doc example 10.1)
        p.fineCents = float (v (Fine) - 50);
        p.keyfollow = d50::kPitchKeyfollow[v (Keyfollow)];
        p.lfoMode = v (LfoMode);
        p.penvMode = v (PEnvMode);
        p.bendMode = v (BendMode);
        p.pulseWidth = float (v (PulseWidth)) / 100.0f;
        p.pwLfoSelect = v (PwLfoSelect);
        p.pwLfoDepth = float (v (PwLfoDepth)) / 100.0f;
        p.cutoff = float (v (TvfCutoff));
        p.resonance = float (v (TvfResonance));
        p.tvfKeyfollow = d50::kTvfKeyfollow[v (TvfKeyfollow)];
        p.tvfEnvDepth = float (v (TvfEnvDepth)) / 100.0f;
        p.tvfVelRange = float (v (TvfEnvVelocity)) / 100.0f;
        for (int i = 0; i < 5; ++i) p.tvfTimes[i] = timeFromValue (float (v (TvfT1 + i)));
        for (int i = 0; i < 3; ++i) p.tvfLevels[i] = float (v (TvfL1 + i)) / 100.0f;
        p.tvfSustain = float (v (TvfSustain)) / 100.0f;
        p.tvfEnd = v (TvfEnd) ? 1.0f : 0.0f;
        p.tvfLfoSelect = v (TvfLfoSelect);
        p.tvfLfoDepth = float (v (TvfLfoDepth)) / 100.0f;
        p.tvaLevel = float (v (TvaLevel)) / 100.0f;
        p.tvaVelRange = float (v (TvaVelocity) - 50) / 50.0f;
        for (int i = 0; i < 5; ++i) p.tvaTimes[i] = timeFromValue (float (v (TvaT1 + i)));
        for (int i = 0; i < 3; ++i) p.tvaLevels[i] = float (v (TvaL1 + i)) / 100.0f;
        p.tvaSustain = float (v (TvaSustain)) / 100.0f;
        p.tvaEnd = v (TvaEnd) ? 1.0f : 0.0f;
        p.tvaLfoSelect = v (TvaLfoSelect);
        p.tvaLfoDepth = float (v (TvaLfoDepth)) / 100.0f;
        p.pwVelRange = float (v (PwVelocity) - 7) / 7.0f;
        p.pwAtRange = float (v (PwAftertouch) - 7) / 7.0f;
        p.tvfBiasPoint = v (TvfBiasPoint);
        p.tvfBiasLevel = float (v (TvfBiasLevel) - 7) / 7.0f;
        p.tvfDepthKf = v (TvfEnvDepthKf);
        p.tvfTimeKf = v (TvfEnvTimeKf);
        p.tvfAtRange = float (v (TvfAftertouch) - 7) / 7.0f;
        p.tvaBiasPoint = v (TvaBiasPoint);
        p.tvaBiasLevelDb = float (v (TvaBiasLevel) - 12);
        p.tvaVelFollow = v (TvaVelocityFollow);
        p.tvaTimeKf = v (TvaTimeKf);
        p.tvaAtRange = float (v (TvaAftertouch) - 7) / 7.0f;
    }

    void decodeTone (const uint8_t* p1, const uint8_t* p2, const uint8_t* c, D50ToneParams& t) noexcept
    {
        using namespace d50::common;
        decodePartial (p1, t.partial[0]);
        decodePartial (p2, t.partial[1]);
        auto v = [c] (int o) { return (int) std::min<uint8_t> (c[o], d50::commonParam (o).maxValue); };
        t.structure = v (Structure);
        const auto& s = kStructures[t.structure];
        t.partial[0].synth = s.p1Synth;
        t.partial[1].synth = s.p2Synth;
        t.penvVelRange = float (v (PEnvVelocity)) * 0.5f;
        t.penvTimeKf = v (PEnvTimeKf);
        for (int i = 0; i < 4; ++i) t.penvTimes[i] = timeFromValue (float (v (PEnvT1 + i)) * 2.0f);
        for (int i = 0; i < 5; ++i) t.penvLevels[i] = float (v (PEnvL0 + i) - 50) / 50.0f * 12.0f;
        t.pModLfoDepth = float (v (PModLfoDepth)) / 100.0f;
        t.pModLever = float (v (PModLever)) / 100.0f;
        t.pModAftertouch = float (v (PModAftertouch)) / 100.0f;
        for (int k = 0; k < 3; ++k)
        {
            t.lfoWave[k] = v (Lfo1Wave + 4 * k);
            t.lfoRate[k] = lfoRateHz (float (v (Lfo1Rate + 4 * k)));
            t.lfoDelay[k] = v (Lfo1Delay + 4 * k) == 0 ? 0.0f : timeFromValue (float (v (Lfo1Delay + 4 * k)));
            t.lfoSync[k] = v (Lfo1Sync + 4 * k);
        }
        t.lowEqHz = d50::kLowEqFreqHz[v (LowEqFreq)];
        t.lowEqDb = float (v (LowEqGain) - 12);
        t.highEqHz = d50::kHighEqFreqHz[v (HighEqFreq)];
        t.highEqQ = d50::kHighEqQ[v (HighEqQ)];
        t.highEqDb = float (v (HighEqGain) - 12);
        t.chorusType = v (ChorusType);
        t.chorusRate = float (v (ChorusRate));
        t.chorusDepth = float (v (ChorusDepth)) / 100.0f;
        t.chorusBalance = float (v (ChorusBalance)) / 100.0f;
        // Partial Mute (00, 01, 10, 11): left digit = partial 1, right digit = partial 2.
        const int mute = v (PartialMute);
        t.partialOn[0] = (mute & 2) != 0;
        t.partialOn[1] = (mute & 1) != 0;
        t.partialBalance = float (v (PartialBalance));
    }
} // namespace

D50Params D50Params::decode (const uint8_t* raw) noexcept
{
    D50Params p;
    decodeTone (raw + d50::kUpperPartial1, raw + d50::kUpperPartial2, raw + d50::kUpperCommon, p.upper);
    decodeTone (raw + d50::kLowerPartial1, raw + d50::kLowerPartial2, raw + d50::kLowerCommon, p.lower);
    const uint8_t* b = raw + d50::kPatchFactors;
    using namespace d50::patch;
    auto v = [b] (int o) { return (int) std::min<uint8_t> (b[o], d50::patchParam (o).maxValue); };
    p.patch.keyMode = v (KeyMode);
    p.patch.splitNote = 36 + v (SplitPoint); // 0..60 = C2..C7 with C4 = 60
    p.patch.upperShift = v (UpperKeyShift) - 24;
    p.patch.lowerShift = v (LowerKeyShift) - 24;
    p.patch.upperFine = float (v (UpperFineTune) - 50);
    p.patch.lowerFine = float (v (LowerFineTune) - 50);
    p.patch.benderRange = v (BenderRange);
    p.patch.reverbType = v (ReverbType);
    p.patch.reverbBalance = float (v (ReverbBalance)) / 100.0f;
    p.patch.totalVolume = float (v (TotalVolume)) / 100.0f;
    p.patch.toneBalance = float (v (ToneBalance));
    p.patch.portamentoMode = v (PortamentoMode);
    p.patch.holdMode = v (HoldMode);
    p.patch.portamentoSeconds = v (PortamentoTime) == 0 ? 0.0f : timeFromValue (float (v (PortamentoTime)));
    p.patch.aftertouchBend = v (AftertouchBendRange) - 12;
    return p;
}

// ---------------------------------------------------------------------------
namespace
{
// One-shot recipe (PCM 1-47). ratios[0] > 0 selects explicit (possibly inharmonic) partial
// ratios; otherwise nHarm harmonics with amplitude 1/h^slope (even harmonics scaled by even).
struct HitRecipe
{
    float lenMs;
    float ratios[6];
    int nHarm; float slope, even;
    float decayMs;     // fundamental decay time constant; 0 = sustained (blown / bowed attack)
    float hfDamp;      // higher partials decay (1 + hfDamp * (ratio - 1)) times faster
    float attackMs;
    float noiseHz, noiseQ, noiseMs, noiseGain;
    float tone;        // 0 = noise only .. 1 = partials only
    float inharm;      // stiff-string stretch (piano)
    float drive;       // tanh saturation (dirty guitar)
    float amHz;        // ratchet amplitude modulation (guiro)
    float detuneCents; // ensemble spread (violins)
};

constexpr float H = 0.0f; // marker: harmonic series
const HitRecipe kHits[47] = {
//    len   ratios                              nH slope even  decay  hfD  att   nHz   nQ   nMs  nG    tone inh     drv  am  det
    { 600, {1, 3.93f, 9.2f},                    0, 0,   1,    350,  1.5f, 0,   2000, 2,   5,  .20f, .90f, 0,     0,   0,  0 }, // 1  Marimba
    {1500, {1, 4.0f, 10.0f},                    0, 0,   1,    1200, .8f,  0,   3000, 2,   4,  .15f, .95f, 0,     0,   0,  0 }, // 2  Vibraphone
    { 400, {1, 3.0f, 6.0f, 9.0f},               0, 0,   1,    200,  1.2f, 0,   4000, 2,   4,  .30f, .85f, 0,     0,   0,  0 }, // 3  Xylophone 1
    { 500, {1, 2.76f, 5.4f, 8.93f},             0, 0,   1,    300,  1.0f, 0,   3000, 1.5f,6,  .25f, .85f, 0,     0,   0,  0 }, // 4  Xylophone 2
    { 600, {1, 2.0f, 3.0f},                     0, 0,   1,    400,  2.0f, 0,    600, 3,  10,  .30f, .80f, 0,     0,   0,  0 }, // 5  Log Bass
    { 250, {H},                                 8, .8f, 1,     80,  1.0f, 0,   1500, 1,  15,  .80f, .50f, 0,     0,   0,  0 }, // 6  Hammer
    { 600, {1, 1.59f, 2.14f, 2.3f},             0, 0,   1,    300,  1.0f, 0,    300, 1,  40,  .70f, .60f, 0,     0,   0,  0 }, // 7  Japanese Drum
    { 700, {1, 5.9f, 14.0f},                    0, 0,   1,    500,  3.0f, 0,   2500, 2,   3,  .15f, .90f, 0,     0,   0,  0 }, // 8  Kalimba
    { 450, {H},                                12, 1,   1,    250,  1.5f, 0,   3000, 1,   5,  .30f, .80f, 0,     0,   0,  0 }, // 9  Pluck 1
    { 200, {1, 2.7f, 4.4f, 6.1f},               0, 0,   1,     60,  .5f,  0,   5000, 3,  30,  .60f, .40f, 0,     0,   0,  0 }, // 10 Chink
    { 500, {1, 2.5f, 4.1f},                     0, 0,   1,    350,  1.0f, 0,   3500, 2,   3,  .20f, .90f, 0,     0,   0,  0 }, // 11 Agogo
    {1500, {1, 2.76f, 5.4f, 8.93f, 13.3f},      0, 0,   1,    1200, .2f,  0,   8000, 2,   2,  .20f, .90f, 0,     0,   0,  0 }, // 12 Triangle
    {1500, {1, 2.0f, 2.4f, 3.0f, 4.5f, 5.33f},  0, 0,   1,    1200, .5f,  0,   4000, 2,   2,  .10f, .95f, 0,     0,   0,  0 }, // 13 Bell's
    { 200, {H},                                 1, 1,   1,     20,  0,    30,  6000, .7f,150, 1.0f, .05f, 0,     0,   0,  0 }, // 14 Nail File
    { 100, {H},                                 6, 1,   1,     30,  1.0f, 0,   4000, 1,   8,  1.0f, .30f, 0,     0,   0,  0 }, // 15 Pick
    { 800, {H},                                16, 1.1f,1,    600,  1.0f, 0,   1000, 1,   6,  .20f, .90f, .0008f,0,   0,  0 }, // 16 Low Piano
    { 700, {H},                                14, 1.2f,1,    500,  1.2f, 0,   2000, 1,   5,  .20f, .90f, .0005f,0,   0,  0 }, // 17 Mid Piano
    { 600, {H},                                10, 1.4f,1,    400,  1.5f, 0,   3500, 1,   4,  .20f, .90f, .0003f,0,   0,  0 }, // 18 High Piano
    { 700, {H},                                24, .6f, 1,    400,  .6f,  0,   4000, 1,   4,  .30f, .85f, 0,     0,   0,  0 }, // 19 Harpsichord
    { 900, {H},                                 8, 1.6f,1,    600,  1.2f, 0,   2000, 1,   3,  .10f, .95f, 0,     0,   0,  0 }, // 20 Harp
    { 400, {1, 2.0f, 3.0f},                     0, 0,   1,    200,  0,    0,   3000, 1,   2,  .20f, .90f, 0,     0,   0,  0 }, // 21 Organ Percussion
    { 800, {H},                                20, .9f, 1,    500,  1.0f, 0,   4000, 1,   4,  .30f, .85f, 0,     0,   0,  0 }, // 22 Steel Strings
    { 700, {H},                                12, 1.3f,1,    450,  1.5f, 0,   2000, 1,   4,  .20f, .90f, 0,     0,   0,  0 }, // 23 Nylon Strings
    { 900, {H},                                16, 1.0f,1,    600,  .8f,  0,   2500, 1,   4,  .20f, .90f, 0,     0,   0,  0 }, // 24 Electric Guitar 1
    { 800, {H},                                16, .8f, .5f,  500,  .8f,  0,   2500, 1,   4,  .20f, .90f, 0,     0,   0,  0 }, // 25 Electric Guitar 2
    { 900, {H},                                30, .5f, 1,    700,  .5f,  0,   2500, 1,   4,  .20f, .90f, 0,     4,   0,  0 }, // 26 Dirty Guitar
    { 700, {H},                                14, .9f, 1,    500,  1.2f, 0,   2000, 1,   4,  .40f, .85f, 0,     0,   0,  0 }, // 27 Pick Bass
    { 600, {H},                                20, .7f, 1,    350,  .8f,  0,   3000, 1,   6,  .50f, .80f, 0,     0,   0,  0 }, // 28 Pop Bass
    { 300, {H},                                 4, 2.0f,1,    150,  1.0f, 0,    300, 1,  20,  .60f, .60f, 0,     0,   0,  0 }, // 29 Thump
    { 600, {H},                                10, 1.4f,1,    400,  1.5f, 0,    800, 1,  10,  .30f, .80f, 0,     0,   0,  0 }, // 30 Upright Bass
    { 400, {H},                                15, 1.0f,.1f,    0,  0,    30,  1500, 1,  40,  .10f, .90f, 0,     0,   0,  0 }, // 31 Clarinet
    { 350, {H},                                 2, 1,   1,      0,  0,    40,  1500, .8f,200, 1.0f, .10f, 0,     0,   0,  0 }, // 32 Breath
    { 600, {H},                                 1, 1,   1,      0,  0,    60,  5000, .6f,400, 1.0f, 0,    0,     0,   0,  0 }, // 33 Steamer
    { 400, {H},                                 4, 2.0f,1,      0,  0,    40,  3000, 1, 100,  .40f, .75f, 0,     0,   0,  0 }, // 34 High Flute
    { 450, {H},                                 5, 1.8f,1,      0,  0,    60,  1500, 1, 100,  .40f, .75f, 0,     0,   0,  0 }, // 35 Low Flute
    { 400, {H},                                 1, 1,   1,      0,  0,    10,  3000, 2, 400,  1.0f, 0,    0,     0,  40,  0 }, // 36 Guiro
    { 450, {H},                                 6, 1.5f,1,      0,  0,    30,  2000, 1,  80,  .50f, .70f, 0,     0,   0,  0 }, // 37 Indian Flute
    { 400, {2, 3, 4},                           0, 0,   1,      0,  0,    20,  4000, 1,  60,  .30f, .80f, 0,     0,   0,  0 }, // 38 Flute Harmonics
    { 300, {H},                                20, .7f, 1,      0,  0,    15,   800, 2,  15,  .40f, .80f, 0,     0,   0,  0 }, // 39 Lips 1
    { 300, {H},                                25, .5f, 1,      0,  0,    10,  1200, 2,  15,  .40f, .80f, 0,     0,   0,  0 }, // 40 Lips 2
    { 350, {H},                                20, .6f, 1,      0,  0,    20,  1500, 1,  20,  .20f, .90f, 0,     0,   0,  0 }, // 41 Trumpet
    { 400, {H},                                18, .8f, 1,      0,  0,    30,  1000, 1,  25,  .20f, .90f, 0,     0,   0, 12 }, // 42 Trombones
    { 450, {H},                                20, .9f, 1,      0,  0,    40,   600, 1,  60,  .30f, .85f, 0,     0,   0,  0 }, // 43 Contrabass
    { 450, {H},                                20, .8f, 1,      0,  0,    40,  1000, 1,  60,  .30f, .85f, 0,     0,   0,  0 }, // 44 Cello
    { 300, {H},                                 6, 1.0f,1,      0,  0,    20,  3000, 1.5f,120,.80f, .30f, 0,     0,   0,  0 }, // 45 Violin Bow
    { 500, {H},                                24, .7f, 1,      0,  0,    50,  2500, 1,  60,  .20f, .90f, 0,     0,   0, 15 }, // 46 Violins
    { 500, {H},                                10, 1.2f,1,    300,  1.5f, 0,   2000, 1,   4,  .20f, .90f, 0,     0,   0,  0 }, // 47 Pizzicart
};

// Looped single-cycle recipe (PCM 48-76). Explicit harmonic amplitudes override the series.
struct LoopRecipe
{
    int nHarm; float slope, even;
    float f1, g1, f2, g2;   // formant centres (Hz at middle C) and gains; Spectrum 1-7 centre on
                            // harmonics 2, 3, 5, 8, 12, 18 and 27 of middle C
    float width;            // formant width in octaves
    float amps[17];         // explicit amplitudes for harmonics 1..16 (amps[0] unused); all 0 = series
    int rootHarm;           // which harmonic is the played pitch (2 for a 16' drawbar)
};

const LoopRecipe kLoops[29] = {
    { 0, 0, 1, 0, 0, 0, 0, .35f, {0, .6f, 1, .8f, .6f, 0, .4f, 0, .3f, 0, .2f, 0, .15f, 0, 0, 0, .1f}, 2 }, // 48 Drawbars
    { 0, 0, 1, 0, 0, 0, 0, .35f, {0, 1, .7f, .4f, .5f, 0, .3f, 0, .4f}, 1 },                               // 49 High Organ
    { 0, 0, 1, 0, 0, 0, 0, .35f, {0, .8f, 1, .5f, .4f}, 2 },                                               // 50 Low Organ
    { 0, 0, 1, 0, 0, 0, 0, .35f, {0, 1, .3f, .1f, .05f}, 1 },                                              // 51 Electric Piano 1
    { 0, 0, 1, 0, 0, 0, 0, .35f, {0, 1, .5f, .2f, 0, 0, 0, .1f}, 1 },                                      // 52 Electric Piano 2
    { 30, .6f, .6f, 2000, 3, 0, 0, .5f, {}, 1 },                                                           // 53 Clavi
    { 40, .5f, 1,   3000, 1, 0, 0, .6f, {}, 1 },                                                           // 54 Harpsichord
    { 12, 1.2f, 1,     0, 0, 0, 0, .35f, {}, 1 },                                                          // 55 Electric Bass 1
    {  8, 1.8f, 1,     0, 0, 0, 0, .35f, {}, 1 },                                                          // 56 Acoustic Bass
    { 16, .9f, 1,    800, 2, 0, 0, .5f, {}, 1 },                                                           // 57 Electric Bass 2
    { 20, .7f, 1,   1200, 1, 0, 0, .5f, {}, 1 },                                                           // 58 Electric Bass 3
    { 24, .8f, 1,   1500, 2, 0, 0, .5f, {}, 1 },                                                           // 59 Electric Guitar
    { 30, .8f, 1,    300, 3, 1200, 2, .4f, {}, 1 },                                                        // 60 Cello
    { 40, .7f, 1,    500, 2, 2500, 3, .4f, {}, 1 },                                                        // 61 Violin
    { 30, .6f, .5f, 1500, 2, 0, 0, .5f, {}, 1 },                                                           // 62 Lead (reed)
    { 30, .7f, 1,    600, 3, 1800, 2, .4f, {}, 1 },                                                        // 63 Sax 1
    { 30, .6f, 1,    900, 3, 2500, 2, .4f, {}, 1 },                                                        // 64 Sax 2
    { 40, 1.0f, 1,   750, 6, 1200, 4, .3f, {}, 1 },                                                        // 65 Aah
    { 30, 1.3f, 1,   300, 6,  870, 3, .3f, {}, 1 },                                                        // 66 Ooh
    { 40, 1.0f, 1,   500, 5, 1500, 3, .3f, {}, 1 },                                                        // 67 Male 1
    { 48, 1.2f, 1,   523, 8, 0, 0, .15f, {}, 1 },                                                          // 68 Spectrum 1
    { 48, 1.2f, 1,   785, 8, 0, 0, .15f, {}, 1 },                                                          // 69 Spectrum 2
    { 48, 1.2f, 1,  1308, 8, 0, 0, .15f, {}, 1 },                                                          // 70 Spectrum 3
    { 48, 1.2f, 1,  2093, 8, 0, 0, .15f, {}, 1 },                                                          // 71 Spectrum 4
    { 48, 1.2f, 1,  3140, 8, 0, 0, .15f, {}, 1 },                                                          // 72 Spectrum 5
    { 48, 1.2f, 1,  4709, 8, 0, 0, .15f, {}, 1 },                                                          // 73 Spectrum 6
    { 48, 1.2f, 1,  7064, 8, 0, 0, .15f, {}, 1 },                                                          // 74 Spectrum 7
    { 40, 1.0f, 1,   400, 5, 2000, 3, .3f, {}, 1 },                                                        // 75 Male 2
    { -1, 0, 0, 0, 0, 0, 0, 0, {}, 1 },                                                                    // 76 Noise
};

void normalise (std::vector<float>& d, float target) noexcept
{
    float peak = 1.0e-9f;
    for (auto x : d) peak = std::max (peak, std::abs (x));
    for (auto& x : d) x *= target / peak;
}
} // namespace

PcmBank::PcmBank()
{
    constexpr float sr = 32000.0f;
    constexpr double twoPi = 6.283185307179586;

    // ---- 1-47: one-shot attack sounds ----
    for (int n = 0; n < 47; ++n)
    {
        const HitRecipe& r = kHits[n];
        Wave& w = waves[n];
        Rng rng;
        rng.state = 0x9E3779B9u * uint32_t (n + 1);
        const int len = int (sr * r.lenMs * 0.001f);
        w.data.assign ((size_t) len, 0.0f);
        w.loop = false;
        w.rootHz = 261.6256f;
        w.rate = sr;

        float ratio[48], amp[48], decay[48];
        int np = 0;
        if (r.ratios[0] > 0.0f)
        {
            for (int k = 0; k < 6 && r.ratios[k] > 0.0f; ++k, ++np)
            {
                ratio[np] = r.ratios[k];
                amp[np] = 1.0f / (1.0f + 0.6f * float (k));
            }
        }
        else
        {
            for (int h = 1; h <= r.nHarm && np < 48; ++h, ++np)
            {
                const float hn = float (h);
                ratio[np] = hn * std::sqrt (1.0f + r.inharm * hn * hn);
                amp[np] = ((h % 2 == 0) ? r.even : 1.0f) / std::pow (hn, r.slope);
            }
        }
        for (int k = 0; k < np; ++k)
            decay[k] = r.decayMs <= 0.0f ? 0.0f : 1000.0f / r.decayMs * (1.0f + r.hfDamp * (ratio[k] - 1.0f));

        Svf bp;
        bp.set (r.noiseHz, r.noiseQ, sr);
        const float noiseRate = 1000.0f / std::max (1.0f, r.noiseMs);
        const float detune = std::exp2 (r.detuneCents / 1200.0f);
        double phase[48][3] {};
        for (int i = 0; i < len; ++i)
        {
            const float t = float (i) / sr;
            float lp, b, hp;
            bp.process (rng.next(), lp, b, hp);
            float s = (1.0f - r.tone) * r.noiseGain * b * std::exp (-noiseRate * t);
            float tonal = 0.0f;
            for (int k = 0; k < np; ++k)
            {
                const float env = decay[k] > 0.0f ? std::exp (-decay[k] * t) : 1.0f;
                const double f = double (w.rootHz * ratio[k]) / double (sr);
                const int copies = r.detuneCents > 0.0f ? 3 : 1;
                for (int c = 0; c < copies; ++c)
                {
                    const double fc = c == 0 ? f : (c == 1 ? f * double (detune) : f / double (detune));
                    phase[k][c] += fc;
                    if (ratio[k] * w.rootHz * (c == 1 ? detune : 1.0f) < sr * 0.45f)
                        tonal += amp[k] * env * float (std::sin (twoPi * phase[k][c])) / float (copies);
                }
            }
            if (r.drive > 0.0f)
                tonal = std::tanh (r.drive * tonal) / std::tanh (r.drive);
            s += r.tone * tonal;
            if (r.amHz > 0.0f)
                s *= 0.5f + 0.5f * std::sin (kTwoPi * r.amHz * t);
            const float attack = r.attackMs > 0.0f ? std::min (1.0f, t * 1000.0f / r.attackMs) : 1.0f;
            // Sustained (blown/bowed) attack samples fade over their last 40 %; decaying ones over 8 ms.
            const int fadeLen = r.decayMs <= 0.0f ? std::max (256, len * 2 / 5) : 256;
            const float fadeOut = std::min (1.0f, float (len - i) / float (fadeLen));
            w.data[(size_t) i] = s * attack * fadeOut;
        }
        normalise (w.data, 0.9f);
    }

    // ---- 48-76: looped waves ----
    for (int n = 47; n < 76; ++n)
    {
        const LoopRecipe& r = kLoops[n - 47];
        Wave& w = waves[n];
        w.loop = true;
        w.rate = sr;
        if (r.nHarm < 0) // Noise (Loop): a long loop of white noise
        {
            Rng rng;
            rng.state = 0xC0FFEEu;
            w.data.assign (16384, 0.0f);
            for (auto& x : w.data) x = rng.next();
            w.rootHz = 261.6256f;
            normalise (w.data, 0.7f);
            w.loopStart = 0; w.loopEnd = (int) w.data.size();
            continue;
        }
        constexpr int len = 512;
        w.data.assign ((size_t) len, 0.0f);
        w.rootHz = sr / float (len) * float (r.rootHarm);
        float amp[49] {};
        bool explicitAmps = false;
        for (int h = 1; h <= 16; ++h) if (r.amps[h] > 0.0f) explicitAmps = true;
        const int nh = explicitAmps ? 16 : std::min (48, r.nHarm);
        for (int h = 1; h <= nh; ++h)
        {
            float a = explicitAmps ? r.amps[h] : ((h % 2 == 0) ? r.even : 1.0f) / std::pow (float (h), r.slope);
            // Formants are placed relative to the harmonic's frequency when the wave plays middle C.
            const float hz = 261.6256f * float (h) / float (r.rootHarm);
            for (int f = 0; f < 2; ++f)
            {
                const float fc = f == 0 ? r.f1 : r.f2, g = f == 0 ? r.g1 : r.g2;
                if (fc > 0.0f && g > 0.0f)
                {
                    const float oct = std::log2 (hz / fc) / std::max (0.05f, r.width);
                    a *= 1.0f + g * std::exp (-0.5f * oct * oct);
                }
            }
            amp[h] = a;
        }
        Rng rng;
        rng.state = 0xA5A5u + uint32_t (n);
        float ph[49] {};
        for (int h = 1; h <= nh; ++h) ph[h] = explicitAmps ? 0.0f : kTwoPi * 0.5f * (rng.next() + 1.0f);
        for (int i = 0; i < len; ++i)
        {
            float s = 0.0f;
            for (int h = 1; h <= nh; ++h)
                s += amp[h] * std::sin (kTwoPi * float (h * i) / float (len) + ph[h]);
            w.data[(size_t) i] = s;
        }
        normalise (w.data, 0.9f);
        w.loopStart = 0; w.loopEnd = len;
    }

    // ---- 77-100: Loop01-24, loops that combine other sounds ----
    for (int k = 0; k < 24; ++k)
    {
        Wave& w = waves[76 + k];
        w.loop = true;
        w.rate = sr;
        w.rootHz = 261.6256f;
        const Wave& a = waves[(k * 7 + 3) % 47];
        const Wave& b = waves[(k * 11 + 5) % 47];
        const Wave& c = waves[47 + (k * 5) % 28];    // a looped wave underneath (not the noise loop)
        const int la = int (sr * (0.12f + 0.04f * float (k % 4)));
        const int lb = int (sr * (0.12f + 0.05f * float ((k / 4) % 3)));
        const int len = la + lb;
        w.data.assign ((size_t) len, 0.0f);
        const float cStep = w.rootHz / c.rootHz;
        float cPos = 0.0f;
        const float cLevel = 0.15f + 0.05f * float (k % 3);
        for (int i = 0; i < len; ++i)
        {
            const bool first = i < la;
            const Wave& src = first ? a : b;
            const int j = first ? i : i - la;
            const int segLen = first ? la : lb;
            float x = j < (int) src.data.size() ? src.data[(size_t) j] : 0.0f;
            const float edge = std::min (1.0f, std::min (float (j), float (segLen - j)) / 64.0f);
            x *= edge;
            x += cLevel * c.read (cPos, cStep);
            w.data[(size_t) i] = x;
        }
        normalise (w.data, 0.9f);
        w.loopStart = 0; w.loopEnd = len;
    }
}

const PcmBank& PcmBank::get()
{
    static const PcmBank bank;
    return bank;
}

// ---------------------------------------------------------------------------
void D50Engine::prepare (double sr, int maxBlock)
{
    sampleRate = float (sr);
    const size_t n = (size_t) std::max (64, maxBlock);
    for (auto* b : { &upperBuf, &lowerBuf, &tmpL, &tmpR, &revInL, &revInR, &revOutL, &revOutR })
        b->assign (n, 0.0f);
    for (auto& c : chorus) c.prepare (sampleRate);
    reverb.prepare (sampleRate);
    dcL.prepare (sampleRate);
    dcR.prepare (sampleRate);
    (void) PcmBank::get(); // build substitute waves outside the audio callback
    reset();
}

void D50Engine::reset() noexcept
{
    allSoundOff();
    for (auto& c : chorus) c.reset();
    for (auto& e : eqLow) e.reset();
    for (auto& e : eqHigh) e.reset();
    reverb.reset();
    dcL.reset(); dcR.reset();
}

int D50Engine::activeVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voices) c += v.isActive() ? 1 : 0;
    return c;
}

void D50Engine::configureEnvelopes (PartialVoice& pv, const D50PartialParams& pp, float note, float velocity) noexcept
{
    const float tvaScale = keyScale (pp.tvaTimeKf, note);
    const float tvfScale = keyScale (pp.tvfTimeKf, note);
    auto& a = pv.tva;
    a.sampleRate = sampleRate;
    a.numSegments = 4;
    for (int i = 0; i < 4; ++i) a.times[i] = pp.tvaTimes[i] * tvaScale;
    // Velocity Follow 0..4: harder playing shortens Time 1 (estimate: up to 75 % at 4)
    a.times[0] *= 1.0f - 0.75f * float (pp.tvaVelFollow) / 4.0f * velocity;
    a.levels[0] = pp.tvaLevels[0]; a.levels[1] = pp.tvaLevels[1]; a.levels[2] = pp.tvaLevels[2]; a.levels[3] = pp.tvaSustain;
    a.releaseTime = pp.tvaTimes[4] * tvaScale;
    a.releaseLevel = 0.0f; // End Level 100 is approximated as 0 so released notes always finish
    a.startLevel = 0.0f;

    auto& f = pv.tvf;
    f.sampleRate = sampleRate;
    f.numSegments = 4;
    for (int i = 0; i < 4; ++i) f.times[i] = pp.tvfTimes[i] * tvfScale;
    f.levels[0] = pp.tvfLevels[0]; f.levels[1] = pp.tvfLevels[1]; f.levels[2] = pp.tvfLevels[2]; f.levels[3] = pp.tvfSustain;
    f.releaseTime = pp.tvfTimes[4] * tvfScale;
    f.releaseLevel = pp.tvfEnd;
    f.startLevel = 0.0f;

    pv.tvaBiasGain = dbToGain (pp.tvaBiasLevelDb * octavesBeyondBias (pp.tvaBiasPoint, note));
    pv.tvfBiasOct = pp.tvfBiasLevel * octavesBeyondBias (pp.tvfBiasPoint, note);
    pv.tvfDepthScale = std::min (2.0f, keyScale (pp.tvfDepthKf, note));
}

void D50Engine::startTone (ToneVoice& tv, const D50ToneParams& tp, float toneNote, float velocity) noexcept
{
    tv.active = true;
    tv.elapsed = 0.0f;
    tv.glideOffset = tv.glideRate = 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        auto& pv = tv.p[i];
        pv.phase = 0.0f;
        pv.pcmPos = 0.0f;
        pv.f1.reset();
        pv.f2.reset();
        pv.lastOsc = 0.0f;
        configureEnvelopes (pv, tp.partial[i], toneNote, velocity);
        pv.tva.noteOn();
        pv.tvf.noteOn();
    }
    auto& pe = tv.penv;
    pe.sampleRate = sampleRate;
    pe.numSegments = 3;
    const float penvScale = keyScale (tp.penvTimeKf, toneNote);
    pe.startLevel = tp.penvLevels[0];
    pe.times[0] = tp.penvTimes[0] * penvScale; pe.levels[0] = tp.penvLevels[1];
    pe.times[1] = tp.penvTimes[1] * penvScale; pe.levels[1] = tp.penvLevels[2];
    pe.times[2] = tp.penvTimes[2] * penvScale; pe.levels[2] = tp.penvLevels[3];
    pe.releaseTime = tp.penvTimes[3] * penvScale;
    pe.releaseLevel = tp.penvLevels[4];
    pe.noteOn();
    const int t = (&tp == &params.upper) ? 0 : 1;
    for (int k = 0; k < 3; ++k)
    {
        tv.lfo[k].wave = tp.lfoWave[k];
        tv.lfo[k].phase = tp.lfoSync[k] == 0 ? freeLfoPhase[t][k] : 0.0f;
    }
}

void D50Engine::noteOn (int note, float velocity) noexcept
{
    const auto& pt = params.patch;
    bool useUpper = true, useLower = false;
    switch (pt.keyMode)
    {
        case 0: case 4: useUpper = true; useLower = false; break;             // Whole, Whole-S
        case 1: case 5: useUpper = true; useLower = true; break;              // Dual, Dual-S
        default: useUpper = note >= pt.splitNote; useLower = !useUpper; break; // Split / Separate variants
    }
    // Solo ("-S") key modes: one note at a time for the solo part(s).
    const bool soloUpper = pt.keyMode == 4 || pt.keyMode == 5 || pt.keyMode == 6 || pt.keyMode == 8;
    const bool soloLower = pt.keyMode == 5 || pt.keyMode == 7 || pt.keyMode == 8;
    for (auto& v : voices)
    {
        if (! v.isActive() || ! v.held) continue;
        if ((useUpper && soloUpper && v.upper.active) || (useLower && soloLower && v.lower.active))
            releaseVoice (v);
    }

    Voice* target = nullptr;
    for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
    if (target == nullptr)
    {
        uint64_t best = UINT64_MAX;
        for (auto& v : voices) // prefer the oldest released voice
            if (! v.held && ! v.sustained && v.age < best) { best = v.age; target = &v; }
        if (target == nullptr)
            for (auto& v : voices)
                if (v.age < best) { best = v.age; target = &v; }
    }
    auto& v = *target;
    v.note = note;
    v.velocity = clampf (velocity, 0.0f, 1.0f);
    v.held = true;
    v.sustained = false;
    v.age = ++counter;
    v.upper.active = v.lower.active = false;
    if (useUpper) startTone (v.upper, params.upper, float (note + pt.upperShift), v.velocity);
    if (useLower) startTone (v.lower, params.lower, float (note + pt.lowerShift), v.velocity);

    // Portamento (CC65 switch, Patch "Portamento Mode" U / L / UL, "Portamento Time")
    if (portamentoOn && lastNote >= 0 && pt.portamentoSeconds > 0.0f && lastNote != note)
    {
        const float offset = float (lastNote - note);
        const float rate = std::abs (offset) / pt.portamentoSeconds;
        if (useUpper && pt.portamentoMode != 1) { v.upper.glideOffset = offset; v.upper.glideRate = rate; }
        if (useLower && pt.portamentoMode != 0) { v.lower.glideOffset = offset; v.lower.glideRate = rate; }
    }
    lastNote = note;
}

void D50Engine::releaseTone (ToneVoice& tv) noexcept
{
    if (! tv.active) return;
    for (auto& pv : tv.p) { pv.tva.noteOff(); pv.tvf.noteOff(); }
    tv.penv.noteOff();
}

int D50Engine::activeToneCount (bool upper) const noexcept
{
    int c = 0;
    for (auto& v : voices) c += (upper ? v.upper.active : v.lower.active) ? 1 : 0;
    return c;
}

void D50Engine::releaseVoice (Voice& v) noexcept
{
    v.held = false;
    v.sustained = false;
    releaseTone (v.upper);
    releaseTone (v.lower);
}

void D50Engine::noteOff (int note) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.held && v.note == note)
        {
            if (sustainDown)
            {
                // Hold Mode (U, L, UL): the pedal only sustains the selected tone(s).
                v.held = false;
                v.sustained = true;
                const int hold = params.patch.holdMode;
                if (hold == 0) releaseTone (v.lower);
                else if (hold == 1) releaseTone (v.upper);
            }
            else releaseVoice (v);
        }
}

void D50Engine::setSustain (bool down) noexcept
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.sustained) releaseVoice (v);
}

void D50Engine::allNotesOff() noexcept
{
    sustainDown = false;
    for (auto& v : voices)
        if (v.isActive()) releaseVoice (v);
}

void D50Engine::allSoundOff() noexcept
{
    sustainDown = false;
    for (auto& v : voices)
    {
        v.upper.active = v.lower.active = false;
        v.held = v.sustained = false;
        v.note = -1;
        for (auto* tv : { &v.upper, &v.lower })
            for (auto& pv : tv->p) { pv.tva.kill(); pv.tvf.kill(); }
    }
}

void D50Engine::renderTone (ToneVoice& tv, const D50ToneParams& tp, float note, float velocity, float* out, int n) noexcept
{
    const auto& bank = PcmBank::get();
    const bool ring = kStructures[tp.structure].ring;
    const float balance[2] = { std::min (1.0f, (100.0f - tp.partialBalance) / 50.0f),
                               std::min (1.0f, tp.partialBalance / 50.0f) };
    const float bendSemis = bend * float (params.patch.benderRange) + aftertouch * float (params.patch.aftertouchBend);

    for (int start = 0; start < n; start += kControlRate)
    {
        const int cb = std::min (kControlRate, n - start);
        const float dt = float (cb) / sampleRate;
        tv.elapsed += dt;

        float lv[3];
        for (int k = 0; k < 3; ++k)
        {
            const float fade = tp.lfoDelay[k] > 0.0f ? std::min (1.0f, tv.elapsed / tp.lfoDelay[k]) : 1.0f;
            lv[k] = tv.lfo[k].advance (tp.lfoRate[k], dt) * fade;
        }
        const float penv = tv.penv.advance (cb) * (1.0f - tp.penvVelRange * (1.0f - velocity));
        const float leverMod = lv[0] * (modWheel * tp.pModLever + aftertouch * tp.pModAftertouch);
        const float lfoMod = lv[0] * tp.pModLfoDepth;
        if (tv.glideRate > 0.0f)
        {
            const float step = tv.glideRate * dt;
            tv.glideOffset = std::abs (tv.glideOffset) <= step ? 0.0f : tv.glideOffset - std::copysign (step, tv.glideOffset);
            if (std::abs (tv.glideOffset) < 1.0e-6f) tv.glideRate = 0.0f;
        }

        float wg[2][kControlRate] {};
        float amp0[2] {}, amp1[2] {};
        bool partialLive[2] {};

        for (int i = 0; i < 2; ++i)
        {
            auto& pv = tv.p[i];
            const auto& pp = tp.partial[i];
            const float tvfEnv = pv.tvf.advance (cb);
            const float prevAmp = pv.tva.value;
            const float tvaEnv = pv.tva.advance (cb);
            partialLive[i] = tp.partialOn[i] && (pv.tva.isActive() || prevAmp > 0.0f);
            if (! partialLive[i])
                continue;

            float pitch = 60.0f + (note + tv.glideOffset - 60.0f) * pp.keyfollow + pp.coarseSemis + pp.fineCents * 0.01f;
            if (pp.penvMode == 1) pitch += penv; else if (pp.penvMode == 2) pitch -= penv;
            if (pp.lfoMode == 1) pitch += lfoMod + leverMod;
            else if (pp.lfoMode == 2) pitch -= lfoMod + leverMod;
            else if (pp.lfoMode == 3) pitch += leverMod;
            if (pp.bendMode == 2) pitch += bendSemis;
            else if (pp.bendMode == 1) pitch += bendSemis * pp.keyfollow;
            const float hz = midiToHz (clampf (pitch, -24.0f, 150.0f));

            // Oscillator for this control block
            if (pp.synth)
            {
                const float dtp = std::min (0.45f, hz / sampleRate);
                if (pp.waveform == 0)
                {
                    const float pw = clampf (pp.pulseWidth + lfoSelect (pp.pwLfoSelect, lv) * pp.pwLfoDepth * 0.5f
                                                 + pp.pwVelRange * velocity * 0.5f + pp.pwAtRange * aftertouch * 0.5f, 0.0f, 1.0f);
                    const float duty = 0.5f - 0.47f * pw;
                    for (int s = 0; s < cb; ++s)
                    {
                        float x = pv.phase < duty ? 1.0f : -1.0f;
                        x += polyBlep (pv.phase, dtp);
                        float p2 = pv.phase + 1.0f - duty;
                        p2 -= std::floor (p2);
                        x -= polyBlep (p2, dtp);
                        x -= 2.0f * duty - 1.0f; // remove the duty-cycle DC component
                        wg[i][s] = x;
                        pv.phase += dtp;
                        if (pv.phase >= 1.0f) pv.phase -= 1.0f;
                    }
                }
                else
                {
                    for (int s = 0; s < cb; ++s)
                    {
                        wg[i][s] = 2.0f * pv.phase - 1.0f - polyBlep (pv.phase, dtp);
                        pv.phase += dtp;
                        if (pv.phase >= 1.0f) pv.phase -= 1.0f;
                    }
                }
            }
            else
            {
                const SampleWave* user = userWaves != nullptr ? userWaves->d50Wave (pp.pcmWave) : nullptr;
                const auto& w = user != nullptr ? *user : bank.waves[pp.pcmWave];
                const float step = (hz / w.rootHz) * (w.rate / sampleRate);
                for (int s = 0; s < cb; ++s)
                    wg[i][s] = w.read (pv.pcmPos, step);
            }

            // TVF (synth partials only; PCM partials bypass the filter as on the original)
            if (pp.synth)
            {
                const float depth = pp.tvfEnvDepth * (1.0f - pp.tvfVelRange * (1.0f - velocity)) * pv.tvfDepthScale;
                const float oct = pp.cutoff * 0.1f + pp.tvfKeyfollow * (note - 60.0f) / 12.0f
                                + depth * 7.0f * tvfEnv + lfoSelect (pp.tvfLfoSelect, lv) * pp.tvfLfoDepth * 2.0f
                                + pv.tvfBiasOct + pp.tvfAtRange * aftertouch * 2.0f;
                const float cutoffHz = 20.0f * std::exp2 (clampf (oct, 0.0f, 10.5f));
                const float q = 0.5f * std::pow (16.0f, pp.resonance / 30.0f);
                pv.f1.set (cutoffHz, 0.54f, sampleRate);
                pv.f2.set (cutoffHz, q, sampleRate);
            }

            const float velGain = pp.tvaVelRange >= 0.0f ? 1.0f - pp.tvaVelRange * (1.0f - velocity)
                                                         : 1.0f + pp.tvaVelRange * velocity;
            const float trem = 1.0f + lfoSelect (pp.tvaLfoSelect, lv) * pp.tvaLfoDepth * 0.5f;
            const float atGain = std::max (0.0f, 1.0f + pp.tvaAtRange * aftertouch * 0.5f);
            const float g = pp.tvaLevel * pp.tvaLevel * std::max (0.0f, velGain) * trem * balance[i] * pv.tvaBiasGain * atGain;
            amp0[i] = prevAmp * g;
            amp1[i] = tvaEnv * g;
        }

        // Ring modulator replaces partial 2's waveform with the product of both generators.
        if (ring && partialLive[1])
            for (int s = 0; s < cb; ++s)
                wg[1][s] = wg[0][s] * wg[1][s];

        for (int i = 0; i < 2; ++i)
        {
            if (! partialLive[i]) continue;
            auto& pv = tv.p[i];
            const bool filt = tp.partial[i].synth;
            const float inc = (amp1[i] - amp0[i]) / float (cb);
            float a = amp0[i];
            for (int s = 0; s < cb; ++s)
            {
                float x = wg[i][s];
                if (filt) x = pv.f2.lowpass (pv.f1.lowpass (x));
                a += inc;
                out[start + s] += x * a * 0.3f;
            }
        }
    }

    bool any = false;
    for (int i = 0; i < 2; ++i)
        any = any || (tp.partialOn[i] && tv.p[i].tva.isActive());
    tv.active = any;
}

void D50Engine::render (float* L, float* R, int n) noexcept
{
    const int maxChunk = (int) upperBuf.size();
    for (int offset = 0; offset < n; offset += maxChunk)
    {
        const int len = std::min (maxChunk, n - offset);
        std::fill (upperBuf.begin(), upperBuf.begin() + len, 0.0f);
        std::fill (lowerBuf.begin(), lowerBuf.begin() + len, 0.0f);

        bool anyVoice = false;
        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            anyVoice = true;
            if (v.upper.active)
                renderTone (v.upper, params.upper,
                            float (v.note + params.patch.upperShift) + params.patch.upperFine * 0.01f,
                            v.velocity, upperBuf.data(), len);
            if (v.lower.active)
                renderTone (v.lower, params.lower,
                            float (v.note + params.patch.lowerShift) + params.patch.lowerFine * 0.01f,
                            v.velocity, lowerBuf.data(), len);
            if (! v.isActive()) { v.note = -1; v.held = v.sustained = false; }
        }
        (void) anyVoice;

        for (int t = 0; t < 2; ++t)
            for (int k = 0; k < 3; ++k)
            {
                const auto& tp = t == 0 ? params.upper : params.lower;
                freeLfoPhase[t][k] += tp.lfoRate[k] * float (len) / sampleRate;
                freeLfoPhase[t][k] -= std::floor (freeLfoPhase[t][k]);
            }

        const float toneGain[2] = { std::min (1.0f, params.patch.toneBalance / 50.0f),
                                    std::min (1.0f, (100.0f - params.patch.toneBalance) / 50.0f) };
        std::fill (revInL.begin(), revInL.begin() + len, 0.0f);
        std::fill (revInR.begin(), revInR.begin() + len, 0.0f);

        for (int t = 0; t < 2; ++t)
        {
            const auto& tp = t == 0 ? params.upper : params.lower;
            float* buf = t == 0 ? upperBuf.data() : lowerBuf.data();
            eqLow[t].setLowShelf (tp.lowEqHz, tp.lowEqDb, sampleRate);
            eqHigh[t].setPeak (tp.highEqHz, tp.highEqQ, tp.highEqDb, sampleRate);
            for (int i = 0; i < len; ++i)
                buf[i] = eqHigh[t].process (eqLow[t].process (buf[i]));
            const auto& cs = kChorusShapes[tp.chorusType];
            chorus[t].process (buf, tmpL.data(), tmpR.data(), len, lfoRateHz (tp.chorusRate) * 0.5f,
                               cs.baseMs, cs.depthMs * tp.chorusDepth, cs.feedback, cs.tremolo * tp.chorusDepth,
                               tp.chorusBalance);
            for (int i = 0; i < len; ++i)
            {
                revInL[(size_t) i] += tmpL[(size_t) i] * toneGain[t];
                revInR[(size_t) i] += tmpR[(size_t) i] * toneGain[t];
            }
        }

        float size, decay, damp, pre;
        reverbSettings (params.patch.reverbType, size, decay, damp, pre);
        reverb.process (revInL.data(), revInR.data(), revOutL.data(), revOutR.data(), len, size, decay, damp, pre);
        const float bal = params.patch.reverbBalance;
        const float vol = params.patch.totalVolume;
        for (int i = 0; i < len; ++i)
        {
            L[offset + i] += dcL.process ((revInL[(size_t) i] * (1.0f - 0.5f * bal) + revOutL[(size_t) i] * bal * 0.6f) * vol);
            R[offset + i] += dcR.process ((revInR[(size_t) i] * (1.0f - 0.5f * bal) + revOutR[(size_t) i] * bal * 0.6f) * vol);
        }
    }
}
} // namespace dc
