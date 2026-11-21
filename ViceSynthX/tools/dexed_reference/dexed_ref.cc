// Reference renderer: Dexed 1ed5a4d MSFA "Modern" engine (FmCore), driven the way Dexed's
// DexedAudioProcessor::processSamples / keydown / keyup do (16 voices, LFO trigger on first key,
// >>4 then >>9 per-voice conversion incl. Dexed's negative-overflow behaviour, sum, PluginFx DC blocker,
// default filter bypassed, gain 1). Local test tool only; not distributed.
// Built by build_and_compare.sh against a separate Dexed checkout. usage: dexed_ref patch156.bin events.txt out.f32 numSamples   (events: "<sample> <note> <velocity>", vel 0 = off)
#include "msfa/synth.h"
#include "msfa/dx7note.h"
#include "msfa/fm_core.h"
#include "msfa/freqlut.h"
#include "msfa/lfo.h"
#include "msfa/pitchenv.h"
#include "msfa/env.h"
#include "msfa/porta.h"
#include "msfa/controllers.h"
#include "msfa/tuning.h"
#include "msfa/exp2.h"
#include "msfa/sin.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <memory>
#include <algorithm>

struct Ev { long t; int note, vel; };
struct Voice { std::unique_ptr<Dx7Note> n; int midi = -1; bool keydown = false, live = false; };

int main (int argc, char** argv)
{
    if (argc < 5) return 2;
    uint8_t raw[156] = {};
    FILE* f = fopen (argv[1], "rb"); if (! f) return 3; fread (raw, 1, 156, f); fclose (f);
    std::vector<Ev> evs; f = fopen (argv[2], "r"); if (! f) return 4;
    Ev e; while (fscanf (f, "%ld %d %d", &e.t, &e.note, &e.vel) == 3) evs.push_back (e); fclose (f);
    const long total = atol (argv[4]);
    const double sr = 48000.0;
    Exp2::init(); Tanh::init(); Sin::init(); // as DexedAudioProcessor::DexedAudioProcessor()
    Freqlut::init (sr); Lfo::init (sr); PitchEnv::init (sr); Env::init_sr (sr); Porta::init_sr (sr);

    FmCore core;
    Controllers c;
    c.pitch_range_up = c.pitch_range_dn = 3; c.pitch_step = 0; c.masterTune = 0;
    c.pitch_cc = 0x2000; c.modwheel_cc = c.foot_cc = c.breath_cc = c.aftertouch_cc = 0;
    c.portamento_enable_cc = false; c.portamento_cc = 0;
    c.core = &core;
    c.refresh();

    auto tuning = createStandardTuning();
    std::shared_ptr<TuningState> ts = tuning;
    MTSClient* mts = nullptr;
    Voice voices[16];
    for (auto& v : voices) v.n = std::make_unique<Dx7Note> (ts, mts);
    Lfo lfo; lfo.reset (raw + 137);
    int current = 0;
    const int transposeShift = raw[144] - 24; // tuningTranspositionShift() for standard tuning

    std::vector<float> out ((size_t) total, 0.0f);
    float dc_id = 0, dc_od = 0; const float dc_r = (float) (1.0 - 126.0 / sr);
    size_t ei = 0;
    for (long i = 0; i < total; i += N)
    {
        while (ei < evs.size() && evs[ei].t <= i)
        {
            const auto& ev = evs[ei++];
            const int pitch = ev.note + transposeShift;
            if (ev.vel > 0)
            {
                bool trig = true;
                for (auto& v : voices) if (v.keydown) { trig = false; if (c.mpeEnabled && true) c.mpeEnabled = false; }
                int note = current; current = (current + 1) % 16;
                voices[note].n->init (raw, pitch, ev.vel, 1, &c);
                if (raw[136]) voices[note].n->oscSync();
                voices[note].midi = pitch; voices[note].keydown = true; voices[note].live = true;
                if (trig) lfo.keydown();
            }
            else
                for (auto& v : voices)
                    if (v.keydown && v.midi == pitch) { v.n->keyup(); v.keydown = false; break; }
        }
        AlignedBuf<int32_t, N> buf; float sum[N] = {};
        for (int j = 0; j < N; ++j) buf.get()[j] = 0;
        const int32_t lv = lfo.getsample(), ld = lfo.getdelay();
        for (auto& v : voices)
            if (v.live)
            {
                v.n->compute (buf.get(), lv, ld, &c);
                for (int j = 0; j < N; ++j)
                {
                    int32_t val = buf.get()[j] >> 4;
                    int cv = val < -(1 << 24) ? 0x8000 : val >= (1 << 24) ? 0x7fff : val >> 9;
                    float x = (float) cv / (float) 0x8000; x = std::clamp (x, -1.0f, 1.0f);
                    sum[j] += x; buf.get()[j] = 0;
                }
            }
        for (int j = 0; j < N && i + j < total; ++j) out[(size_t) (i + j)] = sum[j];
    }
    // PluginFx DC blocker (applied per host block in Dexed; identical as a continuous filter)
    for (long i = 0; i < total; ++i) { const float t = out[(size_t) i]; out[(size_t) i] = t - dc_id + dc_r * dc_od; dc_id = t; dc_od = out[(size_t) i]; }
    f = fopen (argv[3], "wb"); fwrite (out.data(), sizeof (float), out.size(), f); fclose (f);
    return 0;
}
